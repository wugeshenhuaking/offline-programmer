/***************************************************************************************
* Copyright (c) 2018,汇邦电子
* All rights reserved.
*
* 文件名称：w25qxx.c
* 文件摘要：w25qxx驱动程序
* 当前版本：1.0
* 作    者: 张建
* 完成日期：2018年5月25日
***************************************************************************************/
#define FUNC_25QXX_MAIN
#include "bsp_25qxx.h"

/* DMA模式宏：与bsp_25qxx_msp.c保持一致 */
#ifndef USE_DMA_TRANS
#define USE_DMA_TRANS
#endif

#define FUNC_25QXX_CS_L __func_25qxx_cs_l()
#define FUNC_25QXX_CS_H __func_25qxx_cs_h()
//--------------------------------------------------------------------------------------
//延时函数 3us
__attribute__((optnone)) static void delay_3us(void)
{
    volatile unsigned int i = 240 * 3;
    while (i--)
        ;
}
//--------------------------------------------------------------------------------------
//延时函数 100us(连续调用读写函数时需要延时)
__attribute__((optnone)) static void delay_100us(void)
{
    volatile unsigned int i = 240 * 100;
    while (i--)
        ;
}
//--------------------------------------------------------------------------------------
#ifdef USE_DMA_TRANS
/* DMA阻塞模式：传输在MSP函数内同步完成，无需等待SPI_STATE */
#define wait_state_free() delay_100us()
#else
#define wait_state_free() delay_100us()
#endif

/***************************************************************************************
** 创    建:	ZhangJian
** 创建日期:	2018-5-10 8:50:17
** 版    本:	1.0
** 描    述:    初始化 w25qxx
** 入口参数:    无
** 返 回 值:    芯片ID
***************************************************************************************/
void func_25qxx_init(FUNC_25QXX_T *p)
{
    unsigned int i, j;

    __func_25qxx_msp_init(&g_func_25qxx);

    p->id = func_25qxx_read_id() & 0xff; // 读取W25QXXID
    i = p->id & 0xff;

    if (i >= 0x13 && i <= 0x19) //0x13对应25Q08,8MBit,1MByte
    {
        i -= 0x13;
        j = 1;
        while (i--)
        {
            j *= 2;
        }
        p->size = j * 1024 * 1024;
    }
    else
    {
        p->size = 0;
    }
}

/***************************************************************************************
** 创    建:	ZhangJian
** 创建日期:	2018-5-10 8:50:17
** 版    本:	1.0
** 描    述:    读 w25qxx id
** 入口参数:    无
** 返 回 值:    W25QXX ID
***************************************************************************************/
unsigned short func_25qxx_read_id(void)
{
    union
    {
        unsigned char ch[2];
        unsigned short us;
    } u;
    //SPI_STATE = SPI_READ;
    FUNC_25QXX_CS_L;
    delay_100us();
    __func_25qxx_rwb(DEVICE_ID);
    __func_25qxx_rwb(0x00);
    __func_25qxx_rwb(0x00);
    __func_25qxx_rwb(0x00);
    u.ch[1] = __func_25qxx_rwb(0xFF);
    u.ch[0] = __func_25qxx_rwb(0xFF);
    FUNC_25QXX_CS_H;
    //SPI_STATE = SPI_FREE;
    return (u.us);
}

/***************************************************************************************
** 创    建:	ZhangJian
** 创建日期:	2018-5-10 8:50:17
** 版    本:	1.0
** 描    述:    读取W25QXX的状态寄存器
** 入口参数:    无
** 返 回 值:    忙标记位(1,忙;0,空闲) 默认0x00
***************************************************************************************/
unsigned char func_25qxx_read_SR(void)
{
    unsigned char temp;
    //SPI_STATE = SPI_READ;
    FUNC_25QXX_CS_L;
    delay_100us();
    __func_25qxx_rwb(READ_STATUS_1);
    temp = __func_25qxx_rwb(0xFF);
    FUNC_25QXX_CS_H;
    //SPI_STATE = SPI_FREE;
    return temp;
}

/***************************************************************************************
** 创    建:	ZhangJian
** 创建日期:	2018-5-10 8:50:17
** 版    本:	1.0
** 描    述:    写W25QXX的状态寄存器 只有SPR,TB,BP2,BP1,BP0(bit 7,5,4,3,2)可以写!!!
** 入口参数:
** 返 回 值:
***************************************************************************************/
void func_25qxx_write_SR(unsigned char data)
{
    //SPI_STATE = SPI_WRITE;
    FUNC_25QXX_CS_L;
    delay_100us();
    __func_25qxx_rwb(WRITE_STATUS_1);
    __func_25qxx_rwb(data);
    FUNC_25QXX_CS_H;
    //SPI_STATE = SPI_FREE;
}

/***************************************************************************************
** 创    建:	ZhangJian
** 创建日期:	2018-5-10 8:50:17
** 版    本:	1.0
** 描    述:    写使能
** 入口参数:    无
** 返 回 值:    无
***************************************************************************************/
void func_25qxx_write_enable(void)
{
    //SPI_STATE = SPI_WRITE;
    FUNC_25QXX_CS_L;
    delay_100us();
    __func_25qxx_rwb(WRITE_ENABLE);
    FUNC_25QXX_CS_H;
    //SPI_STATE = SPI_FREE;
}

/***************************************************************************************
** 创    建:	ZhangJian
** 创建日期:	2018-5-10 8:50:17
** 版    本:	1.0
** 描    述:    写禁止
** 入口参数:    无
** 返 回 值:    无
***************************************************************************************/
void func_25qxx_write_disable(void)
{
    //SPI_STATE = SPI_WRITE;
    FUNC_25QXX_CS_L;
    delay_100us();
    __func_25qxx_rwb(WRITE_DISABLE);
    FUNC_25QXX_CS_H;
    //SPI_STATE = SPI_FREE;
}

/***************************************************************************************
** 创    建:	ZhangJian
** 创建日期:	2018-5-10 8:50:17
** 版    本:	1.0
** 描    述:    等待W25QXX空闲
** 入口参数:    无
** 返 回 值:    无
***************************************************************************************/
void func_25qxx_wait_free(void)
{
    while ((func_25qxx_read_SR() & 0x01) == 0x01)
        ;
}

/***************************************************************************************
** 创    建:	ZhangJian
** 创建日期:	2018-5-10 8:50:17
** 版    本:	1.0
** 描    述:    擦除一个扇区
** 入口参数:    写入地址（每个扇区4k）
** 返 回 值:    无
***************************************************************************************/
void func_25qxx_erase_sector(unsigned int addr)
{
    wait_state_free();
    addr = addr * W25Q_SECTION_SIZE;
    func_25qxx_write_enable();
    func_25qxx_wait_free();
    FUNC_25QXX_CS_L; // 片选拉低
    delay_100us();
    __func_25qxx_rwb(SECTOR_ERASE_4K);
    __func_25qxx_rwb((unsigned char)(addr >> 16));
    __func_25qxx_rwb((unsigned char)(addr >> 8));
    __func_25qxx_rwb((unsigned char)addr);
    FUNC_25QXX_CS_H; // 片选拉高
    func_25qxx_wait_free();
}

/***************************************************************************************
** 创    建:	ZhangJian
** 创建日期:	2018-5-10 8:50:17
** 版    本:	1.0
** 描    述:    读数据
** 入口参数:    rx_buffer ：接收数据寄存器
				read_addr ：数据起始地址
				length ：   数据长度
** 返 回 值:    无
***************************************************************************************/
void func_25qxx_read(unsigned char *rx_buffer, unsigned int read_addr, unsigned int length)
{
    wait_state_free();
    //SPI_STATE = SPI_READ;
    //memset(SPI_TX_BUFFER, 0xff, TX_LEN);
    FUNC_25QXX_CS_L; // 片选拉低
    delay_100us();
    __func_25qxx_rwb(READ_DATA);
    __func_25qxx_rwb((unsigned char)(read_addr >> 16));
    __func_25qxx_rwb((unsigned char)(read_addr >> 8));
    __func_25qxx_rwb((unsigned char)read_addr);
    __func_25qxx_msp_trans_read(rx_buffer, length); // 读取数据
    FUNC_25QXX_CS_H;
    func_25qxx_wait_free();
}

/***************************************************************************************
** 创    建:	ZhangJian
** 创建日期:	2018-5-10 8:50:17
** 版    本:	1.0
** 描    述:    写一页数据（一个page 256个字节），该页必须已被擦除
** 入口参数:    tx_buffer ：写数据寄存器
				read_addr ：数据起始地址
				length ：   数据长度
** 返 回 值:    无
***************************************************************************************/
void func_25qxx_write_page(unsigned char *tx_buffer, unsigned int write_addr, unsigned int length)
{
    //	wait_state_free();									// 非DMA，不需要
    //	SPI_STATE = SPI_WRITE;
    func_25qxx_write_enable();
    FUNC_25QXX_CS_L; // 片选拉低
    delay_100us();
    __func_25qxx_rwb(PAGE_PROGRAM);
    __func_25qxx_rwb((unsigned char)(write_addr >> 16));
    __func_25qxx_rwb((unsigned char)(write_addr >> 8));
    __func_25qxx_rwb((unsigned char)write_addr);
    __func_25qxx_msp_trans_write(tx_buffer, length); // 发送数据
    FUNC_25QXX_CS_H;
    func_25qxx_wait_free();
}

/***************************************************************************************
** 创    建:	ZhangJian
** 创建日期:	2018-5-10 8:50:17
** 版    本:	1.0
** 描    述:    擦除整个芯片
** 入口参数:
** 返 回 值:
***************************************************************************************/
void func_25qxx_erase_chip(void)
{
    wait_state_free();
    func_25qxx_write_enable();
    func_25qxx_wait_free();
    FUNC_25QXX_CS_L;
    delay_100us();
    __func_25qxx_rwb(CHIP_RASE);
    FUNC_25QXX_CS_H;
    func_25qxx_wait_free();
}

/***************************************************************************************
** 创    建:	ZhangJian
** 创建日期:	2018-5-10 8:50:17
** 版    本:	1.0
** 描    述:    进入掉电模式
** 入口参数:
** 返 回 值:
***************************************************************************************/
void func_25qxx_power_down(void)
{
    wait_state_free();
    //SPI_STATE = SPI_WRITE;
    FUNC_25QXX_CS_L;
    delay_100us();
    __func_25qxx_rwb(POWER_DOWN);
    FUNC_25QXX_CS_H;
    delay_3us();
    //SPI_STATE = SPI_FREE;
}

/***************************************************************************************
** 创    建:	ZhangJian
** 创建日期:	2018-5-10 8:50:17
** 版    本:	1.0
** 描    述:    唤醒
** 入口参数:
** 返 回 值:
***************************************************************************************/
void func_25qxx_wakeup(void)
{
    wait_state_free();
    //SPI_STATE = SPI_WRITE;
    FUNC_25QXX_CS_L;
    delay_100us();
    __func_25qxx_rwb(RELEASE_POWER_DOWN);
    FUNC_25QXX_CS_H;
    delay_3us();
    //SPI_STATE = SPI_FREE;
}

/***************************************************************************************
** 函数名称: W25qxx_Read_Word
** 功能描述: 向W25qxx读一个字(32位)
** 参    数: None
** 返 回 值: 返回写入是否成功
** 备    注: 请确认已将FLASH的写入模式设为4字节
***************************************************************************************/
unsigned int W25qxx_Read_Word(unsigned int read_addr)
{
    //unsigned char	 rx_buffer[4];
    union
    {
        unsigned char ch[4];
        unsigned int ui;
    } u;
    //----------------------------------------------------------------------------------
    wait_state_free();
    FUNC_25QXX_CS_L; // 片选拉低
    delay_100us();
    //----------------------------------------------------------------------------------
    __func_25qxx_rwb(READ_DATA);
    __func_25qxx_rwb((unsigned char)(read_addr >> 16));
    __func_25qxx_rwb((unsigned char)(read_addr >> 8));
    __func_25qxx_rwb((unsigned char)(read_addr >> 0));
    //----------------------------------------------------------------------------------
    u.ch[0] = __func_25qxx_rwb(0xFF);
    u.ch[1] = __func_25qxx_rwb(0xFF);
    u.ch[2] = __func_25qxx_rwb(0xFF);
    u.ch[3] = __func_25qxx_rwb(0xFF);
    //----------------------------------------------------------------------------------
    FUNC_25QXX_CS_H; // 片选拉高，执行操作
    func_25qxx_wait_free();
    //----------------------------------------------------------------------------------
    return (u.ui);
}

/***************************************************************************************
** 函数名称: W25qxx_Write_Word
** 功能描述: 向W25qxx写一个字(32位)
** 参    数: None
** 返 回 值: 返回写入是否成功
** 备    注: 请确认已将FLASH的写入模式设为4字节
***************************************************************************************/
unsigned char W25qxx_Write_Word(unsigned int write_addr, unsigned int dat)
{
    //unsigned char	 tx_buffer[4];
    union
    {
        unsigned char ch[4];
        unsigned int ui;
    } u;
    u.ui = dat;
    //----------------------------------------------------------------------------------
    wait_state_free();         // 等待上次操作完成
    func_25qxx_write_enable(); // 使能写入
    FUNC_25QXX_CS_L;           // 片选拉低
    delay_100us();
    //----------------------------------------------------------------------------------
    __func_25qxx_rwb(PAGE_PROGRAM);
    __func_25qxx_rwb((unsigned char)(write_addr >> 16));
    __func_25qxx_rwb((unsigned char)(write_addr >> 8));
    __func_25qxx_rwb((unsigned char)(write_addr >> 0));
    //----------------------------------------------------------------------------------
    __func_25qxx_rwb(u.ch[0]);
    __func_25qxx_rwb(u.ch[1]);
    __func_25qxx_rwb(u.ch[2]);
    __func_25qxx_rwb(u.ch[3]);
    //----------------------------------------------------------------------------------
    FUNC_25QXX_CS_H; // 片选拉高，执行操作
    func_25qxx_wait_free();
    //----------------------------------------------------------------------------------
    return (W25qxx_Read_Word(write_addr) == dat);
}

#define FLASH_ERASE_BIT 0x01
#define FLASH_WRITE_BIT 0x02
/***************************************************************************************
** 函数名称: W25qxx_Write
** 功能描述: W25qxx写入函数
** 参    数: add		/ 写入数据地址
** 参    数: n			/ 写入数据长度
** 参    数: p			/ 写入数据指针
** 参    数: b_page		/ 允许操作: FLASH_WRITE_ONLY, FLASH_ERASE_ONLY, FLASH_ERASE_WRITE
** 返 回 值: 写入结果(TRUE / FALSE)
//--------------------------------------------------------------------------------------
#define FLASH_SIZE    	2*1024*1024						// flash大小，读写地址不超过该值
#define SECTION_SIZE	4096							// 扇区大小，每次最小擦除一个扇区
#define PAGE_SIZE	   	256								// 页大小，每次最多连续写入一页，超过256字节时连续调用func_25qxx_write_page()函数
***************************************************************************************/
unsigned char W25qxx_Write(unsigned int adr, unsigned int n, const void *p_in, unsigned char b_option)
{
    unsigned int sta_addr, end_addr, length;
    unsigned char *p;
    //----------------------------------------------------------------------------------
    if (b_option == 0)
        return 0; // 无操作：退出
    //----------------------------------------------------------------------------------
    sta_addr = adr;          // 起始地址: 四字节对齐
    end_addr = sta_addr + n; // 结束地址
    if (end_addr >= g_func_25qxx.size)
        return 0; // 地址超出
    //----------------------------------------------------------------------------------
    if (b_option == FLASH_ERASE_BIT) // 仅擦除操作
    {
        sta_addr &= 0xFFFFF000; // 去除低12位
        //------------------------------------------------------------------------------
        while (1)
        {
            func_25qxx_write_enable(); // 使能写入
            func_25qxx_wait_free();    // 等待操作完成
            FUNC_25QXX_CS_L;           // 片选拉低
            delay_100us();
            __func_25qxx_rwb(SECTOR_ERASE_4K);
            __func_25qxx_rwb((unsigned char)(sta_addr >> 16));
            __func_25qxx_rwb((unsigned char)(sta_addr >> 8));
            __func_25qxx_rwb((unsigned char)(sta_addr >> 0));
            FUNC_25QXX_CS_H; // 片选拉高
            func_25qxx_wait_free();
            //--------------------------------------------------------------------------
            sta_addr += W25Q_SECTION_SIZE; // 下一扇区
            if (sta_addr > end_addr)
                break; // 结束判断
        }
        //------------------------------------------------------------------------------
        func_25qxx_write_disable(); // 禁止写入
        return 1;
    }
    //----------------------------------------------------------------------------------
    p = (unsigned char *)p_in; // 转换成U8数据进行处理
    while (sta_addr < end_addr)
    {
        if ((b_option & FLASH_ERASE_BIT)              // 擦除有效
            && ((sta_addr & W25Q_SECTION_MARK) == 0)) // 起始地址
        {
            func_25qxx_write_enable(); // 使能写入
            func_25qxx_wait_free();    // 等待操作完成
            FUNC_25QXX_CS_L;           // 片选拉低
            delay_100us();
            __func_25qxx_rwb(SECTOR_ERASE_4K);
            __func_25qxx_rwb((unsigned char)(sta_addr >> 16));
            __func_25qxx_rwb((unsigned char)(sta_addr >> 8));
            __func_25qxx_rwb((unsigned char)(sta_addr >> 0));
            FUNC_25QXX_CS_H; // 片选拉高
            func_25qxx_wait_free();
        }
        //------------------------------------------------------------------------------
        if (b_option & FLASH_WRITE_BIT) // 写入有效
        {
            if ((end_addr & 0xFFFFFF00) // 判断是否仅在一个数据区
                == (sta_addr & 0xFFFFFF00))
            {
                length = end_addr - sta_addr; // 取全部数据范围
            }
            else
            {
                length = 0x100 - (sta_addr & 0xFF); // 取本扇区的范围
            }
            //--------------------------------------------------------------------------
            func_25qxx_write_enable(); // 使能写入
            func_25qxx_wait_free();    // 等待操作完成
            FUNC_25QXX_CS_L;           // 片选拉低
            delay_100us();
            __func_25qxx_rwb(PAGE_PROGRAM);
            __func_25qxx_rwb((unsigned char)(sta_addr >> 16));
            __func_25qxx_rwb((unsigned char)(sta_addr >> 8));
            __func_25qxx_rwb((unsigned char)(sta_addr >> 0));
            __func_25qxx_msp_trans_write(p, length); // 发送数据
            FUNC_25QXX_CS_H;
            func_25qxx_wait_free();
        }
        //------------------------------------------------------------------------------
        sta_addr += length; // 地址后移
        p += length;        // 数据指针后移
    }
    //----------------------------------------------------------------------------------
    return 1;
}

unsigned char func_25qxx_write(void *p, unsigned int adr, unsigned int n)
{
    return (W25qxx_Write(adr, n, p, FLASH_ERASE_BIT | FLASH_WRITE_BIT));
}

void func_25qxx_test(void)
{
    printf(
        "[W25Q64] init done, id=0x%02X, size=%lu bytes\n", (unsigned)g_func_25qxx.id, (unsigned long)g_func_25qxx.size);

    /* ============================================
     * 测试1: 读取 JEDEC ID（厂商+型号）
     * ============================================ */
    {
        unsigned short id = func_25qxx_read_id();
        printf("[TEST1] JEDEC ID: 0x%04X\n", id);
        // W25Q64 预期: 0x4017
        // W25Q128 预期: 0x4018
        // W25Q32 预期:  0x4016
    }

    /* ============================================
     * 测试2: 擦除扇区 → 写入 → 读回 → 校验
     *         选用扇区200（地址 0xC8000），远离代码区
     * ============================================ */
    {
#define TEST_SECTOR 200
#define TEST_ADDR (TEST_SECTOR * 4096) // 0xC8000

        /* 准备测试数据 */
        unsigned char tx_buf[32];
        unsigned char rx_buf[32];
        unsigned int i;

        for (i = 0; i < sizeof(tx_buf); i++)
            tx_buf[i] = (unsigned char)i; // 0x00, 0x01, 0x02 ... 0x1F

        /* 擦除 */
        func_25qxx_erase_sector(TEST_SECTOR);
        printf("[TEST2] sector %d erased.\n", TEST_SECTOR);

        /* 擦除后读回，应该是全 0xFF */
        func_25qxx_read(rx_buf, TEST_ADDR, sizeof(rx_buf));
        unsigned char erase_ok = 1;
        for (i = 0; i < sizeof(rx_buf); i++)
        {
            if (rx_buf[i] != 0xFF)
            {
                erase_ok = 0;
                printf("[TEST2] erase verify FAIL at offset %u: 0x%02X\n", i, rx_buf[i]);
                break;
            }
        }
        printf("[TEST2] erase verify: %s\n", erase_ok ? "PASS" : "FAIL");

        /* 写入 */
        func_25qxx_write_page(tx_buf, TEST_ADDR, sizeof(tx_buf));
        printf("[TEST2] wrote %d bytes.\n", (unsigned)sizeof(tx_buf));

        /* 读回 */
        memset(rx_buf, 0, sizeof(rx_buf));
        func_25qxx_read(rx_buf, TEST_ADDR, sizeof(tx_buf));

        /* 校验 */
        unsigned char write_ok = 1;
        for (i = 0; i < sizeof(tx_buf); i++)
        {
            if (rx_buf[i] != tx_buf[i])
            {
                write_ok = 0;
                printf("[TEST2] mismatch at offset %u: wrote 0x%02X, read 0x%02X\n", i, tx_buf[i], rx_buf[i]);
                break;
            }
        }
        printf("[TEST2] read-back verify: %s\n", write_ok ? "PASS" : "FAIL");

        /* 打印读回数据 */
        printf("[TEST2] rx: ");
        for (i = 0; i < sizeof(rx_buf); i++)
            printf("%02X ", rx_buf[i]);
        printf("\n");
    }

    /* ============================================
     * 测试3: W25qxx_Write 自动擦写测试
     *         测试跨页写入
     * ============================================ */
    {
#define TEST_ADDR2 0xCA000 // 扇区 202

        /* 准备跨页数据（64字节，地址 0xCA000 跨越 page 边界） */
        unsigned char big_tx[64];
        unsigned char big_rx[64];
        unsigned int i;

        for (i = 0; i < sizeof(big_tx); i++)
            big_tx[i] = (unsigned char)(0xA0 + i); // 0xA0, 0xA1 ...

        /* 擦+写（FLASH_ERASE_BIT | FLASH_WRITE_BIT = 0x03） */
        unsigned char ret = func_25qxx_write(big_tx, TEST_ADDR2, sizeof(big_tx));
        printf("[TEST3] W25qxx_Write return: %u\n", (unsigned)ret);

        /* 读回 */
        memset(big_rx, 0, sizeof(big_rx));
        func_25qxx_read(big_rx, TEST_ADDR2, sizeof(big_tx));

        /* 校验 */
        unsigned char ok = 1;
        for (i = 0; i < sizeof(big_tx); i++)
        {
            if (big_rx[i] != big_tx[i])
            {
                ok = 0;
                printf("[TEST3] mismatch at %u: 0x%02X vs 0x%02X\n", i, big_tx[i], big_rx[i]);
                break;
            }
        }
        printf("[TEST3] cross-page write verify: %s\n", ok ? "PASS" : "FAIL");
    }

    /* ============================================
     * 测试3 完成后: 清理测试数据
     * ============================================ */
    {
        /* 擦除测试用到的扇区 */
        func_25qxx_erase_sector(TEST_SECTOR);      // 扇区200: 0xC8000
        func_25qxx_erase_sector(202);               // 扇区202: 0xCA000
        // 如果 TEST_ADDR2 跨了扇区，还需要擦对应扇区

        /* 验证已擦除 */
        unsigned char clean_rx[32];
        func_25qxx_read(clean_rx, TEST_ADDR, sizeof(clean_rx));
        unsigned char clean_ok = 1;
        for (unsigned int i = 0; i < sizeof(clean_rx); i++)
        {
            if (clean_rx[i] != 0xFF)
            {
                clean_ok = 0;
                break;
            }
        }
        printf("[CLEANUP] erase verify: %s\n", clean_ok ? "PASS" : "FAIL");
    }
}
/***************************************************************************************
**  End Of File
***************************************************************************************/
