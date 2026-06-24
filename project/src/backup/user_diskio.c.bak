/* add user code begin Header */
/**
  **************************************************************************
  * @file     user_diskio.c
  * @brief    user define disk driver
  **************************************************************************
  *
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

#include "fatfs_disk.h"      /* Obtains integer types */

/* private includes ----------------------------------------------------------*/
/* add user code begin private includes */
#include "msc_diskio.h"

/* add user code end private includes */

/* private typedef -----------------------------------------------------------*/
/* add user code begin private typedef */

/* add user code end private typedef */

/* private define ------------------------------------------------------------*/
/* add user code begin private define */

/* add user code end private define */

/* private macro -------------------------------------------------------------*/
/* add user code begin private macro */

/* add user code end private macro */

/* private variables ---------------------------------------------------------*/
/* add user code begin private variables */
static uint32_t user_disk_sector_count = 0;
static uint32_t user_disk_sector_size = 512;

/* add user code end private variables */

/* private function prototypes --------------------------------------------*/
/* add user code begin function prototypes */

/* add user code end function prototypes */

static DSTATUS user_disk_initialize (BYTE lun);
static DSTATUS user_disk_status (BYTE lun);
static DRESULT user_disk_read (BYTE lun, BYTE *buff, LBA_t sector, UINT count);
static DRESULT user_disk_write (BYTE lun, const BYTE *buff, LBA_t sector, UINT count);
static DRESULT user_disk_ioctl (BYTE lun, BYTE cmd, void *buff);

disk_driver_type user_disk = {
	user_disk_initialize,
	user_disk_status,
	user_disk_read,
	user_disk_write,
	user_disk_ioctl
};

/* private user code ---------------------------------------------------------*/
/* add user code begin 0 */

/* add user code end 0 */

/*-----------------------------------------------------------------------*/
/* Inidialize a Drive                                                    */
/*-----------------------------------------------------------------------*/
static DSTATUS user_disk_initialize (BYTE lun)
{
/* add user code begin disk_initialize 0 */
  uint32_t blk_nbr = 0;
  uint32_t blk_size = 0;

  if(msc_disk_capacity(lun, &blk_nbr, &blk_size) != USB_OK)
  {
    return STA_NOINIT;
  }

  user_disk_sector_count = blk_nbr;
  user_disk_sector_size = blk_size;
  return 0;

/* add user code end disk_initialize 0 */

  DSTATUS stat = STA_NOINIT;

/* add user code begin disk_initialize 1 */

/* add user code end disk_initialize 1 */

  return stat;
}

/*-----------------------------------------------------------------------*/
/* Get Drive Status                                                      */
/*-----------------------------------------------------------------------*/
static DSTATUS user_disk_status (BYTE lun)
{
/* add user code begin disk_status 0 */
  uint32_t blk_nbr = 0;
  uint32_t blk_size = 0;

  if(msc_disk_capacity(lun, &blk_nbr, &blk_size) != USB_OK)
  {
    return STA_NOINIT;
  }

  user_disk_sector_count = blk_nbr;
  user_disk_sector_size = blk_size;
  return 0;

/* add user code end disk_status 0 */

  DSTATUS stat = STA_NOINIT;

/* add user code begin disk_status 1 */

/* add user code end disk_status 1 */

  return stat;
}

/*-----------------------------------------------------------------------*/
/* Read Sector(s)                                                        */
/*-----------------------------------------------------------------------*/
static DRESULT user_disk_read (
  BYTE lun,
  BYTE *buff,    /* Data buffer to store read data */
  LBA_t sector,  /* Start sector in LBA */
  UINT count    /* Number of sectors to read */
)
{
/* add user code begin disk_read 0 */
  uint64_t addr;
  uint32_t len;

  if(buff == NULL || count == 0)
  {
    return RES_PARERR;
  }

  addr = (uint64_t)sector * user_disk_sector_size;
  len = (uint32_t)count * user_disk_sector_size;

  if(msc_disk_read(lun, addr, buff, len) != USB_OK)
  {
    return RES_ERROR;
  }

  return RES_OK;

/* add user code end disk_read 0 */

  return RES_OK;
}



/*-----------------------------------------------------------------------*/
/* Write Sector(s)                                                       */
/*-----------------------------------------------------------------------*/
static DRESULT user_disk_write (
  BYTE lun,
  const BYTE *buff,  /* Data to be written */
  LBA_t sector,    /* Start sector in LBA */
  UINT count      /* Number of sectors to write */
)
{
/* add user code begin disk_write 0 */
  uint64_t addr;
  uint32_t len;

  if(buff == NULL || count == 0)
  {
    return RES_PARERR;
  }

  addr = (uint64_t)sector * user_disk_sector_size;
  len = (uint32_t)count * user_disk_sector_size;

  if(msc_disk_write(lun, addr, (uint8_t *)buff, len) != USB_OK)
  {
    return RES_ERROR;
  }

  return RES_OK;

/* add user code end disk_write 0 */

  return RES_OK;
}

/*-----------------------------------------------------------------------*/
/* Miscellaneous Functions                                               */
/*-----------------------------------------------------------------------*/
static DRESULT user_disk_ioctl (
  BYTE lun,
  BYTE cmd,    /* Control code */
  void *buff    /* Buffer to send/receive control data */
)
{
/* add user code begin disk_ioctl 0 */
  uint32_t blk_nbr = 0;
  uint32_t blk_size = 0;

  switch(cmd)
  {
    case CTRL_SYNC:
      return (msc_disk_flush(lun) == USB_OK) ? RES_OK : RES_ERROR;

    case GET_SECTOR_COUNT:
      if(buff == NULL)
      {
        return RES_PARERR;
      }
      if(msc_disk_capacity(lun, &blk_nbr, &blk_size) != USB_OK)
      {
        if(user_disk_sector_count == 0)
        {
          return RES_ERROR;
        }
        blk_nbr = user_disk_sector_count;
        blk_size = user_disk_sector_size;
      }
      *(LBA_t *)buff = (LBA_t)blk_nbr;
      user_disk_sector_count = blk_nbr;
      user_disk_sector_size = blk_size;
      return RES_OK;

    case GET_SECTOR_SIZE:
      if(buff == NULL)
      {
        return RES_PARERR;
      }
      *(WORD *)buff = (WORD)user_disk_sector_size;
      return RES_OK;

    case GET_BLOCK_SIZE:
      if(buff == NULL)
      {
        return RES_PARERR;
      }
      *(DWORD *)buff = 4096U / user_disk_sector_size;
      return RES_OK;
  }

/* add user code end disk_ioctl 0 */

  DRESULT res = RES_PARERR;

/* add user code begin disk_ioctl 1 */

/* add user code end disk_ioctl 1 */

  return res;
}

/* add user code begin 1 */

/* add user code end 1 */
