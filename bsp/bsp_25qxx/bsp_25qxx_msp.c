/* add user code begin Header */
/**
  ******************************************************************************
  * @file     bsp_25qxx_msp.c
  * @brief    W25QXX SPI 驱动 (AT32F403A v3 BSP) - DMA模式
  ******************************************************************************
  */
/* add user code end Header */

/* Includes ------------------------------------------------------------------*/
#include "bsp.h"
#include "bsp_25qxx.h"

/* add user code begin 0 */
#include <stdio.h>
/* add user code end 0 */

#ifndef __FUNC_25QXX_MSP

#define FUNC_25QXX_SPI    SPI2
#define SPI_TX_Err        0x01
#define SPI_RX_Err        0x02
#define FUNC_25QXX_SPI_CS_PIN_PORT       GPIOB
#define FUNC_25QXX_SPI_CS_PIN            GPIO_PINS_12
#define FUNC_25QXX_SPI_CS_RESET          gpio_bits_reset(FUNC_25QXX_SPI_CS_PIN_PORT, FUNC_25QXX_SPI_CS_PIN)
#define FUNC_25QXX_SPI_CS_SET            gpio_bits_set(FUNC_25QXX_SPI_CS_PIN_PORT, FUNC_25QXX_SPI_CS_PIN)

/* ========== DMA 配置 ========== */
#define USE_DMA_TRANS                      /* 启用DMA传输 */

#define SPI_RX_DMA_CHANNEL   DMA1_CHANNEL4
#define SPI_TX_DMA_CHANNEL   DMA1_CHANNEL5
#define SPI2_DR_ADDR         ((uint32_t)((uint32_t *)&SPI2->dt))

/* DMA通道4/5的标志位 (用于轮询等待完成) */
#define SPI_RX_DMA_FDT_FLAG  DMA1_FDT4_FLAG
#define SPI_TX_DMA_FDT_FLAG  DMA1_FDT5_FLAG
#define SPI_RX_DMA_ALL_FLAG  (DMA1_GL4_FLAG | DMA1_FDT4_FLAG | DMA1_HDT4_FLAG | DMA1_DTERR4_FLAG)
#define SPI_TX_DMA_ALL_FLAG  (DMA1_GL5_FLAG | DMA1_FDT5_FLAG | DMA1_HDT5_FLAG | DMA1_DTERR5_FLAG)

/* 0xFF 哑字节，DMA读取时发送用 */
static uint8_t spi_dummy_ff = 0xFF;
/* 哑字节，DMA写入时丢弃接收用 */
static uint8_t spi_dummy_rx = 0x00;

/**
  * @brief  CS 拉低（选中）
  */
void __func_25qxx_cs_l(void)
{
    FUNC_25QXX_SPI_CS_RESET;
}

/**
  * @brief  CS 拉高（取消选中）
  */
void __func_25qxx_cs_h(void)
{
    FUNC_25QXX_SPI_CS_SET;
}

/**
  * @brief  SPI 底层硬件初始化（GPIO + SPI 外设 + DMA通道配置）
  * @param  p: FUNC_25QXX_T 结构体指针
  * @retval none
  */
void __func_25qxx_msp_init(FUNC_25QXX_T *p)
{
    gpio_init_type gpio_init_struct;
    dma_init_type  dma_init_struct;

    /* ========== 使能外设时钟 ========== */
    crm_periph_clock_enable(CRM_SPI2_PERIPH_CLOCK, TRUE);
    crm_periph_clock_enable(CRM_GPIOB_PERIPH_CLOCK, TRUE);
    crm_periph_clock_enable(CRM_DMA1_PERIPH_CLOCK, TRUE);

    /* ========== 配置 GPIO 引脚 ========== */
    gpio_default_para_init(&gpio_init_struct);

    /* SCK - PB13（复用推挽） */
    gpio_init_struct.gpio_mode           = GPIO_MODE_MUX;
    gpio_init_struct.gpio_pins           = GPIO_PINS_13;
    gpio_init_struct.gpio_pull           = GPIO_PULL_NONE;
    gpio_init_struct.gpio_out_type       = GPIO_OUTPUT_PUSH_PULL;
    gpio_init_struct.gpio_drive_strength = GPIO_DRIVE_STRENGTH_MODERATE;
    gpio_init(GPIOB, &gpio_init_struct);

    /* MISO - PB14（浮空输入） */
    gpio_default_para_init(&gpio_init_struct);
    gpio_init_struct.gpio_mode = GPIO_MODE_INPUT;
    gpio_init_struct.gpio_pins = GPIO_PINS_14;
    gpio_init_struct.gpio_pull = GPIO_PULL_NONE;
    gpio_init(GPIOB, &gpio_init_struct);

    /* MOSI - PB15（复用推挽） */
    gpio_default_para_init(&gpio_init_struct);
    gpio_init_struct.gpio_mode           = GPIO_MODE_MUX;
    gpio_init_struct.gpio_pins           = GPIO_PINS_15;
    gpio_init_struct.gpio_pull           = GPIO_PULL_NONE;
    gpio_init_struct.gpio_out_type       = GPIO_OUTPUT_PUSH_PULL;
    gpio_init_struct.gpio_drive_strength = GPIO_DRIVE_STRENGTH_MODERATE;
    gpio_init(GPIOB, &gpio_init_struct);

    /* CS - PB12（软件片选，通用推挽输出） */
    gpio_default_para_init(&gpio_init_struct);
    gpio_init_struct.gpio_mode           = GPIO_MODE_OUTPUT;
    gpio_init_struct.gpio_pins           = GPIO_PINS_12;
    gpio_init_struct.gpio_pull           = GPIO_PULL_NONE;
    gpio_init_struct.gpio_out_type       = GPIO_OUTPUT_PUSH_PULL;
    gpio_init_struct.gpio_drive_strength = GPIO_DRIVE_STRENGTH_MODERATE;
    gpio_init(GPIOB, &gpio_init_struct);

    /* CS 拉高 — 默认不选中 */
    __func_25qxx_cs_h();

    /* ========== 配置 SPI2 ========== */
    spi_init_type spi_init_struct;
    spi_default_para_init(&spi_init_struct);
    spi_init_struct.transmission_mode   = SPI_TRANSMIT_FULL_DUPLEX;
    spi_init_struct.master_slave_mode   = SPI_MODE_MASTER;
    spi_init_struct.mclk_freq_division  = SPI_MCLK_DIV_4;            /* APB1/4 = 30 MHz */
    spi_init_struct.first_bit_transmission = SPI_FIRST_BIT_MSB;
    spi_init_struct.frame_bit_num       = SPI_FRAME_8BIT;
    spi_init_struct.clock_polarity      = SPI_CLOCK_POLARITY_HIGH;    /* CPOL=1 */
    spi_init_struct.clock_phase         = SPI_CLOCK_PHASE_2EDGE;      /* CPHA=1 */
    spi_init_struct.cs_mode_selection   = SPI_CS_SOFTWARE_MODE;
    spi_init(FUNC_25QXX_SPI, &spi_init_struct);

    /* 使能 SPI */
    spi_enable(FUNC_25QXX_SPI, TRUE);

    /* 注意：SPI DMA 请求不在全局使能，而是在每次 DMA 传输时使能。
     * 原因：如果在 init 中全局使能 DMA 请求，轮询操作（__func_25qxx_rwb）
     * 期间 SPI 也会产生 DMA 请求。当 DMA 通道后使能时，可能存在残留的
     * DMA 请求导致 RX DMA 多读一个字节（一字节偏移 bug）。
     */

    /* ========== 配置 DMA Flexible 映射 ========== */
    /* SPI2_RX -> DMA1 Channel4, SPI2_TX -> DMA1 Channel5 */
    dma_flexible_config(DMA1, FLEX_CHANNEL4, DMA_FLEXIBLE_SPI2_RX);
    dma_flexible_config(DMA1, FLEX_CHANNEL5, DMA_FLEXIBLE_SPI2_TX);

    /* 复位DMA通道 */
    dma_reset(SPI_RX_DMA_CHANNEL);
    dma_reset(SPI_TX_DMA_CHANNEL);

    /* ========== 配置 RX DMA 通道 (Channel4): SPI2_DR -> 内存 ========== */
    dma_default_para_init(&dma_init_struct);
    dma_init_struct.peripheral_base_addr  = SPI2_DR_ADDR;
    dma_init_struct.memory_base_addr      = (uint32_t)&spi_dummy_rx;  /* 默认丢弃，read时更新 */
    dma_init_struct.direction             = DMA_DIR_PERIPHERAL_TO_MEMORY;
    dma_init_struct.buffer_size           = 0;                        /* 传输时更新 */
    dma_init_struct.peripheral_inc_enable = FALSE;
    dma_init_struct.memory_inc_enable     = FALSE;                    /* 默认不增量，read时改TRUE */
    dma_init_struct.peripheral_data_width = DMA_PERIPHERAL_DATA_WIDTH_BYTE;
    dma_init_struct.memory_data_width     = DMA_MEMORY_DATA_WIDTH_BYTE;
    dma_init_struct.loop_mode_enable      = FALSE;
    dma_init_struct.priority              = DMA_PRIORITY_HIGH;
    dma_init(SPI_RX_DMA_CHANNEL, &dma_init_struct);

    /* ========== 配置 TX DMA 通道 (Channel5): 内存 -> SPI2_DR ========== */
    dma_default_para_init(&dma_init_struct);
    dma_init_struct.peripheral_base_addr  = SPI2_DR_ADDR;
    dma_init_struct.memory_base_addr      = (uint32_t)&spi_dummy_ff; /* 默认发0xFF，write时更新 */
    dma_init_struct.direction             = DMA_DIR_MEMORY_TO_PERIPHERAL;
    dma_init_struct.buffer_size           = 0;                       /* 传输时更新 */
    dma_init_struct.peripheral_inc_enable = FALSE;
    dma_init_struct.memory_inc_enable     = FALSE;                   /* 默认不增量，write时改TRUE */
    dma_init_struct.peripheral_data_width = DMA_PERIPHERAL_DATA_WIDTH_BYTE;
    dma_init_struct.memory_data_width     = DMA_MEMORY_DATA_WIDTH_BYTE;
    dma_init_struct.loop_mode_enable      = FALSE;
    dma_init_struct.priority              = DMA_PRIORITY_MEDIUM;
    dma_init(SPI_TX_DMA_CHANNEL, &dma_init_struct);
}

/**
  * @brief  SPI 读写一个字节（全双工交换）— 用于命令/地址字节
  * @param  TxData: 要发送的字节
  * @retval 接收到的字节
  */
unsigned char __func_25qxx_rwb(unsigned char TxData)
{
    unsigned char perr = 0;
    uint32_t retry = 0;

    /* 等待发送缓冲区空 */
    while (spi_i2s_flag_get(FUNC_25QXX_SPI, SPI_I2S_TDBE_FLAG) == RESET)
    {
        retry++;
        if (retry > 200)
        {
            perr |= SPI_TX_Err;
            return 0;
        }
    }
    spi_i2s_data_transmit(FUNC_25QXX_SPI, TxData);

    retry = 0;

    /* 等待接收缓冲区非空 */
    while (spi_i2s_flag_get(FUNC_25QXX_SPI, SPI_I2S_RDBF_FLAG) == RESET)
    {
        retry++;
        if (retry > 65536)
        {
            perr |= SPI_RX_Err;
            return 0;
        }
    }
    return (unsigned char)spi_i2s_data_receive(FUNC_25QXX_SPI);
}

/* ========== DMA 传输模式 ========== */
#ifdef USE_DMA_TRANS

/**
  * @brief  SPI DMA 读取数据（阻塞等待完成）
  *         SPI全双工：发送0xFF哑字节的同时接收数据
  *         DMA通道已在__func_25qxx_msp_init中配置，此处只更新传输参数
  * @param  rx_buffer: 接收缓冲区
  * @param  length:    数据长度
  * @retval none
  */
void __func_25qxx_msp_trans_read(unsigned char *rx_buffer, unsigned int length)
{
    /* 禁用DMA通道 */
    dma_channel_enable(SPI_RX_DMA_CHANNEL, FALSE);
    dma_channel_enable(SPI_TX_DMA_CHANNEL, FALSE);

    /* 清除所有DMA标志 */
    dma_flag_clear(SPI_RX_DMA_ALL_FLAG);
    dma_flag_clear(SPI_TX_DMA_ALL_FLAG);

    /* --- 更新 RX DMA 传输参数 --- */
    SPI_RX_DMA_CHANNEL->maddr = (uint32_t)rx_buffer;
    SPI_RX_DMA_CHANNEL->dtcnt = (uint16_t)length;
    SPI_RX_DMA_CHANNEL->ctrl_bit.mincm = 1;   /* 内存地址自增：接收数据递增 */

    /* --- 更新 TX DMA 传输参数 --- */
    SPI_TX_DMA_CHANNEL->maddr = (uint32_t)&spi_dummy_ff;
    SPI_TX_DMA_CHANNEL->dtcnt = (uint16_t)length;
    SPI_TX_DMA_CHANNEL->ctrl_bit.mincm = 0;   /* 内存地址不自增：始终发0xFF */

    /* ========== 彻底清理 SPI 状态，确保 DMA 启动时无残留 ========== */

    /* 等待 SPI 完全空闲：TX 缓冲区空 + 移位寄存器空 */
    while (spi_i2s_flag_get(FUNC_25QXX_SPI, SPI_I2S_TDBE_FLAG) == RESET);
    while (spi_i2s_flag_get(FUNC_25QXX_SPI, SPI_I2S_BF_FLAG) != RESET);

    /* 清除可能残留的接收数据（RDBF=1 时读取 DR 清除） */
    if (spi_i2s_flag_get(FUNC_25QXX_SPI, SPI_I2S_RDBF_FLAG) != RESET)
    {
        (void)FUNC_25QXX_SPI->dt;
    }

    /* 清除 overrun 错误标志（读 DR + 读 STS） */
    if (spi_i2s_flag_get(FUNC_25QXX_SPI, SPI_I2S_ROERR_FLAG) != RESET)
    {
        (void)FUNC_25QXX_SPI->dt;
        (void)FUNC_25QXX_SPI->sts;
    }

    /* 使能 SPI DMA 请求（仅在 DMA 传输期间使能） */
    spi_i2s_dma_transmitter_enable(FUNC_25QXX_SPI, TRUE);
    spi_i2s_dma_receiver_enable(FUNC_25QXX_SPI, TRUE);

    /* 启动DMA传输：先使能RX，再使能TX
     * RX 先使能确保第一个接收字节到来时 DMA 已准备好读取
     */
    dma_channel_enable(SPI_RX_DMA_CHANNEL, TRUE);
    dma_channel_enable(SPI_TX_DMA_CHANNEL, TRUE);

    /* 等待RX DMA传输完成 */
    while (dma_flag_get(SPI_RX_DMA_FDT_FLAG) == RESET);

    /* 禁用DMA通道 */
    dma_channel_enable(SPI_RX_DMA_CHANNEL, FALSE);
    dma_channel_enable(SPI_TX_DMA_CHANNEL, FALSE);

    /* 清除DMA标志 */
    dma_flag_clear(SPI_RX_DMA_ALL_FLAG);
    dma_flag_clear(SPI_TX_DMA_ALL_FLAG);

    /* 禁用 SPI DMA 请求（防止轮询操作期间产生残留 DMA 请求） */
    spi_i2s_dma_transmitter_enable(FUNC_25QXX_SPI, FALSE);
    spi_i2s_dma_receiver_enable(FUNC_25QXX_SPI, FALSE);

    /* 等待SPI忙标志清除（最后一位移位完成） */
    while (spi_i2s_flag_get(FUNC_25QXX_SPI, SPI_I2S_BF_FLAG) != RESET);

    /* 清除SPI DR中可能残留的接收数据，防止污染后续轮询操作 */
    if (spi_i2s_flag_get(FUNC_25QXX_SPI, SPI_I2S_RDBF_FLAG) != RESET)
    {
        (void)FUNC_25QXX_SPI->dt;
    }
}

/**
  * @brief  SPI DMA 写入数据（阻塞等待完成）
  *         SPI全双工：发送数据的同时丢弃接收数据
  *         DMA通道已在__func_25qxx_msp_init中配置，此处只更新传输参数
  * @param  tx_buffer: 发送缓冲区
  * @param  length:    数据长度
  * @retval none
  */
void __func_25qxx_msp_trans_write(unsigned char *tx_buffer, unsigned int length)
{
    /* 禁用DMA通道 */
    dma_channel_enable(SPI_RX_DMA_CHANNEL, FALSE);
    dma_channel_enable(SPI_TX_DMA_CHANNEL, FALSE);

    /* 清除所有DMA标志 */
    dma_flag_clear(SPI_RX_DMA_ALL_FLAG);
    dma_flag_clear(SPI_TX_DMA_ALL_FLAG);

    /* --- 更新 RX DMA 传输参数 --- */
    SPI_RX_DMA_CHANNEL->maddr = (uint32_t)&spi_dummy_rx;
    SPI_RX_DMA_CHANNEL->dtcnt = (uint16_t)length;
    SPI_RX_DMA_CHANNEL->ctrl_bit.mincm = 0;   /* 内存地址不自增：丢弃接收数据 */

    /* --- 更新 TX DMA 传输参数 --- */
    SPI_TX_DMA_CHANNEL->maddr = (uint32_t)tx_buffer;
    SPI_TX_DMA_CHANNEL->dtcnt = (uint16_t)length;
    SPI_TX_DMA_CHANNEL->ctrl_bit.mincm = 1;   /* 内存地址自增：发送数据递增 */

    /* ========== 彻底清理 SPI 状态，确保 DMA 启动时无残留 ========== */

    /* 等待 SPI 完全空闲：TX 缓冲区空 + 移位寄存器空 */
    while (spi_i2s_flag_get(FUNC_25QXX_SPI, SPI_I2S_TDBE_FLAG) == RESET);
    while (spi_i2s_flag_get(FUNC_25QXX_SPI, SPI_I2S_BF_FLAG) != RESET);

    /* 清除可能残留的接收数据 */
    if (spi_i2s_flag_get(FUNC_25QXX_SPI, SPI_I2S_RDBF_FLAG) != RESET)
    {
        (void)FUNC_25QXX_SPI->dt;
    }

    /* 清除 overrun 错误标志 */
    if (spi_i2s_flag_get(FUNC_25QXX_SPI, SPI_I2S_ROERR_FLAG) != RESET)
    {
        (void)FUNC_25QXX_SPI->dt;
        (void)FUNC_25QXX_SPI->sts;
    }

    /* 使能 SPI DMA 请求（仅在 DMA 传输期间使能） */
    spi_i2s_dma_transmitter_enable(FUNC_25QXX_SPI, TRUE);
    spi_i2s_dma_receiver_enable(FUNC_25QXX_SPI, TRUE);

    /* 启动DMA传输 */
    dma_channel_enable(SPI_RX_DMA_CHANNEL, TRUE);
    dma_channel_enable(SPI_TX_DMA_CHANNEL, TRUE);

    /* 等待RX DMA传输完成（而非TX）
     * TX DMA FDT仅表示数据已写入DR，但SPI可能还在移位。
     * RX DMA FDT表示所有字节已收发完毕，DR已清空。
     */
    while (dma_flag_get(SPI_RX_DMA_FDT_FLAG) == RESET);

    /* 禁用DMA通道 */
    dma_channel_enable(SPI_RX_DMA_CHANNEL, FALSE);
    dma_channel_enable(SPI_TX_DMA_CHANNEL, FALSE);

    /* 清除DMA标志 */
    dma_flag_clear(SPI_RX_DMA_ALL_FLAG);
    dma_flag_clear(SPI_TX_DMA_ALL_FLAG);

    /* 禁用 SPI DMA 请求（防止轮询操作期间产生残留 DMA 请求） */
    spi_i2s_dma_transmitter_enable(FUNC_25QXX_SPI, FALSE);
    spi_i2s_dma_receiver_enable(FUNC_25QXX_SPI, FALSE);

    /* 等待SPI忙标志清除（最后一位移位完成） */
    while (spi_i2s_flag_get(FUNC_25QXX_SPI, SPI_I2S_BF_FLAG) != RESET);

    /* 清除SPI DR中可能残留的接收数据，防止污染后续轮询操作 */
    if (spi_i2s_flag_get(FUNC_25QXX_SPI, SPI_I2S_RDBF_FLAG) != RESET)
    {
        (void)FUNC_25QXX_SPI->dt;
    }
}

#else /* 轮询模式 (备用) */

/**
  * @brief  SPI 接收数据（轮询模式）
  */
void __func_25qxx_msp_trans_read(unsigned char *rx_buffer, unsigned int length)
{
    while (length--)
        *rx_buffer++ = __func_25qxx_rwb(0xFF);
}

/**
  * @brief  SPI 发送数据（轮询模式）
  */
void __func_25qxx_msp_trans_write(unsigned char *tx_buffer, unsigned int length)
{
    while (length--)
        __func_25qxx_rwb(*tx_buffer++);
}

#endif /* USE_DMA_TRANS */

#endif /* __FUNC_25QXX_MSP */
