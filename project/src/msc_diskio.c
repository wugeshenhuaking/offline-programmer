/* add user code begin Header */
/**
  **************************************************************************
  * @file     msc_diskio.c
  * @brief    usb mass storage disk function
  **************************************************************************
  * Copyright (c) 2025, Artery Technology, All rights reserved.
  *
  * The software Board Support Package (BSP) that is made available to
  * download from Artery official website is the copyrighted work of Artery.
  * Artery authorizes customers to use, copy, and distribute the BSP
  * software and its related documentation for the purpose of design and
  * development in conjunction with Artery microcontrollers. Use of the
  * software is governed by this copyright notice and the following disclaimer.
  *
  * THIS SOFTWARE IS PROVIDED ON "AS IS" BASIS WITHOUT WARRANTIES,
  * GUARANTEES OR REPRESENTATIONS OF ANY KIND. ARTERY EXPRESSLY DISCLAIMS,
  * TO THE FULLEST EXTENT PERMITTED BY LAW, ALL EXPRESS, IMPLIED OR
  * STATUTORY OR OTHER WARRANTIES, GUARANTEES OR REPRESENTATIONS,
  * INCLUDING BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY,
  * FITNESS FOR A PARTICULAR PURPOSE, OR NON-INFRINGEMENT.
  *
  **************************************************************************
  */
/* add user code end Header */

#include "msc_diskio.h"
#include "msc_bot_scsi.h"

/* private includes ----------------------------------------------------------*/
/* add user code begin private includes */
#include "bsp_25qxx\bsp_25qxx.h"
#include <string.h>
/* add user code end private includes */

/* private typedef -----------------------------------------------------------*/
/* add user code begin private typedef */

/* add user code end private typedef */

/* private define ------------------------------------------------------------*/
/* add user code begin private define */
#define W25Q_MSC_BLOCK_SIZE     512     /* MSC逻辑块大小，Windows标准 */
#define W25Q_SECTOR_SIZE        4096    /* W25Q64物理扇区大小 */
#ifndef W25Q_READ_CACHE_COUNT
#define W25Q_READ_CACHE_COUNT   4       /* 4 * 4KB read cache for FAT/directory hot sectors */
#endif
/* add user code end private define */

/* private macro -------------------------------------------------------------*/
/* add user code begin private macro */

/* add user code end private macro */

/* private variables ---------------------------------------------------------*/
/* add user code begin private variables */

/*
 * ============ 扇区缓存 ============
 * W25Q64 擦除单位为 4KB，而 USB MSC 以 512 字节块写入。
 * 如果不缓存，每写 512 字节就要做一次 读4K→擦除→写4K，
 * 同一个 4KB 扇区被写 8 次就擦除 8 次（每次 100~400ms）。
 *
 * 有了缓存后：
 * - 写入同一扇区：仅合并数据到缓存，不擦除
 * - 切换到不同扇区：才 flush（擦除 + 写回）
 * - 读取命中缓存：直接从缓存返回，不读 Flash
 *
 * 这样 8 次 512 字节写入只需 1 次擦除，速度提升约 8 倍！
 */
static uint8_t  w25q_sector_buf[W25Q_SECTOR_SIZE];  /* 扇区数据缓冲 */
static uint32_t w25q_cache_sec_addr = 0xFFFFFFFF;   /* 当前缓存的扇区基址，0xFFFFFFFF=无效 */
static uint8_t  w25q_cache_valid  = 0;              /* 1=缓存中有有效数据（已从Flash读出） */
static uint8_t  w25q_cache_dirty  = 0;              /* 1=缓存已被修改，需要写回Flash */

static uint8_t  w25q_read_cache_buf[W25Q_READ_CACHE_COUNT][W25Q_SECTOR_SIZE];
static uint32_t w25q_read_cache_sec_addr[W25Q_READ_CACHE_COUNT];
static uint32_t w25q_read_cache_age[W25Q_READ_CACHE_COUNT];
static uint8_t  w25q_read_cache_valid[W25Q_READ_CACHE_COUNT];
static uint32_t w25q_read_cache_tick = 0;

/* add user code end private variables */

/* private function prototypes --------------------------------------------*/
/* add user code begin function prototypes */
static void flush_sector_cache(void);
static void load_sector_to_cache(uint32_t sector_addr);
static int find_read_cache(uint32_t sector_addr);
static uint8_t alloc_read_cache_slot(void);
static uint8_t *get_read_cache_sector(uint32_t sector_addr);
static void invalidate_read_cache_sector(uint32_t sector_addr);
/* add user code end function prototypes */

/* private user code ---------------------------------------------------------*/
/* add user code begin 0 */

/**
  * @brief  将脏缓存写回 Flash（擦除 + 逐页写回）
  */
static void flush_sector_cache(void)
{
    if (!w25q_cache_valid || !w25q_cache_dirty)
        return;

    /* 擦除扇区 */
    func_25qxx_erase_sector(w25q_cache_sec_addr / W25Q_SECTOR_SIZE);

    /* 逐页写回（每页256字节） */
    for (uint32_t page = 0; page < W25Q_SECTOR_SIZE; page += W25Q_PAGE_SIZE)
    {
        func_25qxx_write_page(w25q_sector_buf + page,
                              w25q_cache_sec_addr + page,
                              W25Q_PAGE_SIZE);
    }

    invalidate_read_cache_sector(w25q_cache_sec_addr);
    w25q_cache_dirty = 0;
}

static int find_read_cache(uint32_t sector_addr)
{
    for (uint8_t index = 0; index < W25Q_READ_CACHE_COUNT; index++)
    {
        if (w25q_read_cache_valid[index] &&
            w25q_read_cache_sec_addr[index] == sector_addr)
        {
            return index;
        }
    }

    return -1;
}

static uint8_t alloc_read_cache_slot(void)
{
    uint8_t oldest_index = 0;
    uint32_t oldest_age = 0xFFFFFFFF;

    for (uint8_t index = 0; index < W25Q_READ_CACHE_COUNT; index++)
    {
        if (!w25q_read_cache_valid[index])
        {
            return index;
        }

        if (w25q_read_cache_age[index] < oldest_age)
        {
            oldest_age = w25q_read_cache_age[index];
            oldest_index = index;
        }
    }

    return oldest_index;
}

static uint8_t *get_read_cache_sector(uint32_t sector_addr)
{
    int index = find_read_cache(sector_addr);

    if (index < 0)
    {
        index = alloc_read_cache_slot();
        func_25qxx_read(w25q_read_cache_buf[index], sector_addr, W25Q_SECTOR_SIZE);
        w25q_read_cache_sec_addr[index] = sector_addr;
        w25q_read_cache_valid[index] = 1;
    }

    w25q_read_cache_age[index] = ++w25q_read_cache_tick;

    return w25q_read_cache_buf[index];
}

static void invalidate_read_cache_sector(uint32_t sector_addr)
{
    int index = find_read_cache(sector_addr);

    if (index >= 0)
    {
        w25q_read_cache_valid[index] = 0;
    }
}

/**
  * @brief  将指定扇区加载到缓存中
  *         如果当前缓存是脏的，先写回；如果目标扇区已在缓存中，直接返回
  * @param  sector_addr: 扇区基址（必须是 4096 对齐的）
  */
static void load_sector_to_cache(uint32_t sector_addr)
{
    uint8_t *cached_sector;

    /* 已在缓存中，无需重新加载 */
    if (w25q_cache_valid && w25q_cache_sec_addr == sector_addr)
        return;

    /* 先 flush 当前脏缓存 */
    flush_sector_cache();

    /* Reuse read cache when Windows has just queried this FAT/directory sector. */
    cached_sector = get_read_cache_sector(sector_addr);
    memcpy(w25q_sector_buf, cached_sector, W25Q_SECTOR_SIZE);
    w25q_cache_sec_addr = sector_addr;
    w25q_cache_valid  = 1;
    w25q_cache_dirty  = 0;
}

/* add user code end 0 */

uint8_t scsi_inquiry[MSC_SUPPORT_MAX_LUN][SCSI_INQUIRY_DATA_LENGTH] =
{
  /* lun = 0 */
  {
    0x00,         /* peripheral device type (direct-access device) */
    0x80,         /* removable media bit */
    0x00,         /* ansi version, ecma version, iso version */
    0x01,         /* respond data format */
    SCSI_INQUIRY_DATA_LENGTH - 5, /* additional length */
    0x00, 0x00, 0x00, /* reserved */
    'A', 'T', '3', '2', ' ', ' ', ' ', ' ', /* vendor information "AT32" */
    'D', 'i', 's', 'k', '0', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ', /* Product identification "Disk" */
    '2', '.', '0', '0'  /* product revision level */
  }
};

/**
  * @brief  get disk inquiry
  * @param  lun: logical units number
  * @retval inquiry string
  */
uint8_t *get_inquiry(uint8_t lun)
{
  /* add user code begin get_inquiry 0 */

  /* add user code end get_inquiry 0 */

  if(lun < MSC_SUPPORT_MAX_LUN)
    return (uint8_t *)scsi_inquiry[lun];
  else
    return NULL;

  /* add user code begin get_inquiry 1 */

  /* add user code end get_inquiry 1 */
}

/**
  * @brief  disk read
  * @param  lun: logical units number
  * @param  addr: logical address
  * @param  read_buf: pointer to read buffer
  * @param  len: read length
  * @retval status of usb_sts_type
  */
usb_sts_type msc_disk_read(uint8_t lun, uint64_t addr, uint8_t *read_buf, uint32_t len)
{
  /* add user code begin msc_disk_read 0 */
  if(lun == SPI_FLASH_LUN)
  {
    uint32_t start_addr = (uint32_t)addr;
    uint32_t end_addr   = start_addr + len;
    uint32_t sec_start  = start_addr & ~(W25Q_SECTOR_SIZE - 1);
    uint32_t sec_end    = (end_addr + W25Q_SECTOR_SIZE - 1) & ~(W25Q_SECTOR_SIZE - 1);

    for (uint32_t sec = sec_start; sec < sec_end; sec += W25Q_SECTOR_SIZE)
    {
      uint32_t overlap_start = (start_addr > sec) ? start_addr : sec;
      uint32_t overlap_end   = (end_addr < sec + W25Q_SECTOR_SIZE) ? end_addr : (sec + W25Q_SECTOR_SIZE);
      uint32_t overlap_len   = overlap_end - overlap_start;
      uint32_t buf_offset    = overlap_start - start_addr;

      if (w25q_cache_valid && w25q_cache_sec_addr == sec)
      {
        memcpy(read_buf + buf_offset, w25q_sector_buf + (overlap_start - sec), overlap_len);
      }
      else
      {
        uint8_t *cached_sector = get_read_cache_sector(sec);
        memcpy(read_buf + buf_offset, cached_sector + (overlap_start - sec), overlap_len);
      }
    }

    return USB_OK;
  }

  /* add user code end msc_disk_read 0 */

  switch(lun)
  {
    case INTERNAL_FLASH_LUN:
      break;
    case SPI_FLASH_LUN:
      break;
    case SD_LUN:
      break;
    default:
      break;
  }

  /* add user code begin msc_disk_read 1 */

  /* add user code end msc_disk_read 1 */

  return USB_OK;
}

/**
  * @brief  disk write
  * @param  lun: logical units number
  * @param  addr: logical address
  * @param  buf: pointer to write buffer
  * @param  len: write length
  * @retval status of usb_sts_type
  */
usb_sts_type msc_disk_write(uint8_t lun, uint64_t addr, uint8_t *buf, uint32_t len)
{
  /* add user code begin msc_disk_write 0 */
  if(lun == SPI_FLASH_LUN)
  {
    uint32_t start_addr = (uint32_t)addr;
    uint32_t end_addr   = start_addr + len;
    uint32_t sec_start  = start_addr & ~(W25Q_SECTOR_SIZE - 1);
    uint32_t sec_end    = (end_addr + W25Q_SECTOR_SIZE - 1) & ~(W25Q_SECTOR_SIZE - 1);

    for (uint32_t sec = sec_start; sec < sec_end; sec += W25Q_SECTOR_SIZE)
    {
      load_sector_to_cache(sec);

      uint32_t overlap_start = (start_addr > sec) ? start_addr : sec;
      uint32_t overlap_end   = (end_addr < sec + W25Q_SECTOR_SIZE) ? end_addr : (sec + W25Q_SECTOR_SIZE);
      uint32_t overlap_len   = overlap_end - overlap_start;
      uint32_t buf_offset    = overlap_start - start_addr;

      memcpy(w25q_sector_buf + (overlap_start - sec), buf + buf_offset, overlap_len);
      invalidate_read_cache_sector(sec);
      w25q_cache_dirty = 1;
    }

    /* 立即把脏缓存写回 flash，避免断电丢失数据 */
    flush_sector_cache();

    return USB_OK;
  }

  /* add user code end msc_disk_write 0 */

  switch(lun)
  {
    case INTERNAL_FLASH_LUN:
      break;
    case SPI_FLASH_LUN:
      break;
    case SD_LUN:
      break;
    default:
      break;;
  }

  /* add user code begin msc_disk_write 1 */

  /* add user code end msc_disk_write 1 */

  return USB_OK;
}

/**
  * @brief  disk capacity
  * @param  lun: logical units number
  * @param  blk_nbr: pointer to number of block
  * @param  blk_size: pointer to block size
  * @retval status of usb_sts_type
  */
usb_sts_type msc_disk_capacity(uint8_t lun, uint32_t *blk_nbr, uint32_t *blk_size)
{
  /* add user code begin msc_disk_capacity 0 */
  if(lun == SPI_FLASH_LUN)
  {
    *blk_size = W25Q_MSC_BLOCK_SIZE;
    if (g_func_25qxx.size > 0)
      *blk_nbr = g_func_25qxx.size / W25Q_MSC_BLOCK_SIZE;
    else
      *blk_nbr = (8 * 1024 * 1024) / W25Q_MSC_BLOCK_SIZE;

    return USB_OK;
  }

  /* add user code end msc_disk_capacity 0 */

  switch(lun)
  {
    case INTERNAL_FLASH_LUN:
      break;
    case SPI_FLASH_LUN:
      break;
    case SD_LUN:
      break;
    default:
      break;
  }

  /* add user code begin msc_disk_capacity 1 */

  /* add user code end msc_disk_capacity 1 */

  return USB_OK;
}

/* add user code begin 1 */

usb_sts_type msc_disk_flush(uint8_t lun)
{
  if(lun == SPI_FLASH_LUN)
  {
    flush_sector_cache();
    return USB_OK;
  }

  switch(lun)
  {
    case INTERNAL_FLASH_LUN:
      break;
    case SD_LUN:
      break;
    default:
      return USB_FAIL;
  }

  return USB_OK;
}

/* add user code end 1 */
