/******************************************************************************
* Copyright (c) 2018,汇邦电子
* All rights reserved.
*
* 文件名称：w25qxx.c
* 文件摘要：w25qxx驱动程序
* 当前版本：1.0
* 作    者: 张建
* 完成日期：2018年5月25日
******************************************************************************/
/*
25Qxx系列flash是大容量串行存储器，其尾缀表示多少M bit，25Q128代表128MBit=16MByte，
存储结构分为页page（256），扇区sector（4096），块block（65536），
写入操作必须在一业内，跨页应该在页边界先做一次写入操作，
擦除必须以扇区为单位，
Block用于群组设定保护的
*/
#ifndef __FUNC_25QXX_H__
#define __FUNC_25QXX_H__

#include <stdio.h>
#include <string.h>
#ifdef FUNC_25QXX_MAIN
#define FUNC_25QXX_EXT
#else
#define FUNC_25QXX_EXT extern
#endif

//--------------------------------------------------------------------------------------

//--------------------------------------------------------------------------------------
//#define W25Q_FLASH_SIZE    	2*1024*1024					// flash大小，读写地址不超过该值
#define W25Q_SECTION_SIZE 4096                    // 扇区大小，每次最小擦除一个扇区
#define W25Q_SECTION_MARK (W25Q_SECTION_SIZE - 1) // 扇区大小，屏蔽字
#define W25Q_PAGE_SIZE 256 // 页大小，每次最多连续写入一页，超过256字节时连续调用func_25qxx_write_page()函数
//--------------------------------------------------------------------------------------
// W25X系列/Q系列芯片列表
#define W25Q08 0xEF13
#define W25Q16 0xEF14
#define W25Q32 0xEF15
#define W25Q64 0xEF16
#define W25Q128 0xEF17

//--------------------------------------------------------------------------------------
// 指令表
#define WRITE_ENABLE ((unsigned char)0x06)
#define WRITE_DISABLE ((unsigned char)0x04)
#define READ_STATUS_1 ((unsigned char)0x05)
#define WRITE_STATUS_1 ((unsigned char)0x01)
#define READ_STATUS_2 ((unsigned char)0x35)
#define WRITE_STATUS_2 ((unsigned char)0x31)
#define READ_STATUS_3 ((unsigned char)0x15)
#define WRITE_STATUS_3 ((unsigned char)0x11)
#define CHIP_RASE ((unsigned char)0xC7)
#define ERASE_SUSPEND ((unsigned char)0x75)
#define ERASE_RESUME ((unsigned char)0x7A)
#define POWER_DOWN ((unsigned char)0xB9)
#define RELEASE_POWER_DOWN ((unsigned char)0xAB)
#define DEVICE_ID ((unsigned char)0x90)
#define JEDEC_ID ((unsigned char)0x9F)
#define GLOBAL_BLOCK_LOCK ((unsigned char)0x7E)
#define GLOBAL_BLOCK_UNLOCK ((unsigned char)0x98)
#define ENTER_QPI_MODE ((unsigned char)0x38)
#define ENTER_RESET ((unsigned char)0x66)
#define RESET_DEVICE ((unsigned char)0x99)
#define READ_UNIQUE_ID ((unsigned char)0x4B)
#define PAGE_PROGRAM ((unsigned char)0x02)
#define QUAD_PAGE_PROGRAM ((unsigned char)0x32)
#define SECTOR_ERASE_4K ((unsigned char)0x20)
#define SECTOR_ERASE_32K ((unsigned char)0x52)
#define SECTOR_ERASE_64K ((unsigned char)0xD8)
#define READ_DATA ((unsigned char)0x03)
#define FAST_READ ((unsigned char)0x0B)
#define FAST_READ_DUAL_OUT ((unsigned char)0x3B)
#define FAST_READ_QUAD_OUT ((unsigned char)0x6B)
#define READ_SFDP_REG ((unsigned char)0x5A)
#define ERASE_SECURITY_REG ((unsigned char)0x44)

//--------------------------------------------------------------------------------------
//全局变量结构体
typedef struct
{
    unsigned short id;
    unsigned int size;
} FUNC_25QXX_T;

FUNC_25QXX_EXT FUNC_25QXX_T g_func_25qxx;
//--------------------------------------------------------------------------------------
//------------------------------------ MSP相关函数 --------------------------------------
//CS引脚拉低
void __func_25qxx_cs_l(void);
//CS引脚拉高
void __func_25qxx_cs_h(void);

//读写SPI口一个字节
unsigned char __func_25qxx_rwb(unsigned char TxData);
//硬件初始化
void __func_25qxx_msp_init(FUNC_25QXX_T *p);
//读数据，有可能用DMA
void __func_25qxx_msp_trans_read(unsigned char *rx_buffer, unsigned int length); // 读取数据
//写数据，有可能用DMA
void __func_25qxx_msp_trans_write(unsigned char *tx_buffer, unsigned int length); // 发送数据
//--------------------------------------------------------------------------------------

// 读芯片ID
extern unsigned short func_25qxx_read_id(void);
// 读状态寄存器
static unsigned char func_25qxx_read_SR(void);
// 写状态寄存器
void func_25qxx_write_SR(unsigned char data);
// 写使能
extern void func_25qxx_write_enable(void);
// 写禁止
extern void func_25qxx_write_disable(void);
// 等待W25QXX空闲
extern void func_25qxx_wait_free(void);

// 写一页数据
extern void func_25qxx_write_page(unsigned char *tx_buffer, unsigned int write_addr, unsigned int length);
// 擦除整个芯片（需要时间）
extern void func_25qxx_erase_chip(void);
// 掉电
extern void func_25qxx_power_down(void);
// 唤醒
extern void func_25qxx_wakeup(void);

//**************************************************************************************
//********************************* 常用用户接口函数 ************************************
// W25QXX 初始化
extern void func_25qxx_init(FUNC_25QXX_T *p);
// 擦除一个扇区
extern void func_25qxx_erase_sector(unsigned int addr);
// W25QXX 读数据
extern void func_25qxx_read(unsigned char *rx_buffer, unsigned int read_addr, unsigned int length);
/***************************************************************************************
** 函数名称: W25qxx_Write
** 功能描述: W25qxx写入函数
** 参    数: add		/ 写入数据地址
** 参    数: n			/ 写入数据长度
** 参    数: p			/ 写入数据指针
** 参    数: b_page		/ 允许操作: FLASH_WRITE_ONLY, FLASH_ERASE_ONLY, FLASH_ERASE_WRITE
** 返 回 值: 写入结果(TRUE / FALSE)
***************************************************************************************/
unsigned char func_25qxx_write(void *p, unsigned int adr, unsigned int n);
// 测试w25q
void func_25qxx_test(void);

#endif
/***************************************************************************************
**  End Of File
***************************************************************************************/
