#include "bsp_usart.h"
#include "task_modbus\task_modbus_communication.h"
#include <agile_modbus.h>
#include "wk_system.h"

/* ===================== RS485 鏀跺彂鎺у埗 ===================== */
#define RS485_TX_EN() gpio_bits_set(RS485_EN2_GPIO_PORT, RS485_EN2_PIN)
#define RS485_RX_EN() gpio_bits_reset(RS485_EN2_GPIO_PORT, RS485_EN2_PIN)

/* ===================== 3.5T 瀹氭椂鍣ㄥ弬鏁?===================== */
/*
 * Modbus 瑙勮寖锛氭尝鐗圭巼 > 19200 鏃跺浐瀹?1750us
 * 115200 > 19200锛屾墍浠ュ浐瀹氱敤 1750us
 * TMR6 璁℃暟鏃堕挓閰嶇疆涓?1MHz锛?us/tick锛?
 * 婧㈠嚭鍊?= 1750
 *
 * APB1 鏃堕挓鏍规嵁浣犵殑瀹為檯閰嶇疆淇敼
 * 渚嬪 APB1 = 120MHz锛氶鍒嗛 = 120-1锛岃鏁版椂閽?= 1MHz
 */
#define MODBUS_BAUDRATE 19200
#define APB1_CLOCK_MHZ 120
#define RS485_SEND_MUTEX_TIMEOUT_MS 10U
#define RS485_TX_FLAG_TIMEOUT_MS    20U

/* ===================== 澶栭儴缂撳啿鍖猴紙task閲屽畾涔夛級 ===================== */
extern uint8_t ctx_send_buf[AGILE_MODBUS_MAX_ADU_LENGTH];
extern uint8_t ctx_read_buf[AGILE_MODBUS_MAX_ADU_LENGTH];

/* ===================== FreeRTOS 鍚屾閲?===================== */
static SemaphoreHandle_t rx_done_sem = NULL; /* 鎺ユ敹甯у畬鎴?*/
static SemaphoreHandle_t tx_mutex = NULL;    /* 鍙戦€佷簰鏂ラ攣 */

/* ===================== 鎺ユ敹鐘舵€?===================== */
static volatile uint16_t rx_frame_len = 0; /* 褰撳墠甯ч暱搴?*/
static volatile uint8_t rx_receiving = 0;  /* 姝ｅ湪鎺ユ敹鏍囧織 */

/* ===================== 璋冭瘯鐘舵€?===================== */
volatile uint32_t g_rs485_dbg_stage = 0;
volatile uint32_t g_rs485_dbg_wait_flag = 0;
volatile uint32_t g_rs485_dbg_uart4_sts = 0;
volatile uint32_t g_rs485_dbg_uart4_ctrl1 = 0;
volatile uint32_t g_rs485_dbg_len = 0;
volatile uint32_t g_rs485_dbg_index = 0;

/* ================================================================
 * 鍐呴儴鍑芥暟
 * ================================================================ */

/* 鍚姩/閲嶇疆 3.5T 瀹氭椂鍣?*/
static void timer35_stop(void);
static void dma_rx_restart(void);
static void uart4_rx_clear_pending(void);

static void uart4_rx_event_pause(void)
{
    timer35_stop();
    rx_receiving = 0;
    rx_frame_len = 0;
    dma_channel_enable(DMA1_CHANNEL1, FALSE);
    usart_dma_receiver_enable(UART4, FALSE);
    usart_interrupt_enable(UART4, USART_IDLE_INT, FALSE);
}

static void uart4_rx_event_resume(void)
{
    (void)UART4->sts;
    (void)UART4->dt;
    usart_interrupt_enable(UART4, USART_IDLE_INT, TRUE);
}

static int uart4_wait_flag(uint32_t flag, uint32_t timeout_ms)
{
    TickType_t start = xTaskGetTickCount();
    TickType_t timeout_ticks = pdMS_TO_TICKS(timeout_ms);

    if (timeout_ticks == 0U)
    {
        timeout_ticks = 1U;
    }

    while (usart_flag_get(UART4, flag) == RESET)
    {
        if ((xTaskGetTickCount() - start) >= timeout_ticks)
        {
            return -1;
        }
    }

    return 0;
}

static void uart4_tx_prepare(void)
{
    usart_dma_transmitter_enable(UART4, FALSE);
    usart_interrupt_enable(UART4, USART_TDC_INT, FALSE);
    usart_interrupt_enable(UART4, USART_TDBE_INT, FALSE);
    usart_transmitter_enable(UART4, TRUE);
    usart_enable(UART4, TRUE);
}

static void uart4_tx_soft_recover(void)
{
    usart_enable(UART4, FALSE);
    usart_transmitter_enable(UART4, FALSE);
    usart_dma_transmitter_enable(UART4, FALSE);
    usart_interrupt_enable(UART4, USART_TDC_INT, FALSE);
    usart_interrupt_enable(UART4, USART_TDBE_INT, FALSE);
    usart_transmitter_enable(UART4, TRUE);
    usart_enable(UART4, TRUE);
}

static void timer35_reset_and_start(void)
{
    tmr_counter_enable(TMR6, FALSE);
    tmr_flag_clear(TMR6, TMR_OVF_FLAG);
    tmr_counter_value_set(TMR6, 0);
    tmr_counter_enable(TMR6, TRUE);
}

/* 鍋滄 3.5T 瀹氭椂鍣?*/
static void timer35_stop(void)
{
    tmr_counter_enable(TMR6, FALSE);
    tmr_flag_clear(TMR6, TMR_OVF_FLAG);
}

static void uart4_rx_clear_pending(void)
{
    usart_flag_clear(UART4, USART_IDLEF_FLAG | USART_ROERR_FLAG |
                                USART_NERR_FLAG | USART_FERR_FLAG);

    while (usart_flag_get(UART4, USART_RDBF_FLAG) != RESET)
    {
        (void)UART4->dt;
    }
}

/* 閲嶅惎 DMA 鎺ユ敹锛宲tr 褰掗浂锛屼粠绱㈠紩0寮€濮?*/
static void dma_rx_restart(void)
{
    dma_channel_enable(DMA1_CHANNEL1, FALSE);
    dma_flag_clear(DMA1_FDT1_FLAG);
    DMA1_CHANNEL1->maddr = (uint32_t)ctx_read_buf;
    dma_data_number_set(DMA1_CHANNEL1, AGILE_MODBUS_MAX_ADU_LENGTH);
    dma_channel_enable(DMA1_CHANNEL1, TRUE);
}

uint32_t modbus_timer_config(uint32_t baudrate)
{
    uint32_t timeout_us;

    if (baudrate > 19200)
    {
        timeout_us = 1750;
    }
    else
    {
        // 3.5 脳 11 脳 1000000 / baudrate
        timeout_us = 38500000 / baudrate;
        // 3.5 脳 11 脳 1000000 = 38,500,000
    }
    return timeout_us;
}

/* ================================================================
 * 鍒濆鍖栧嚱鏁?
 * ================================================================ */

static void usart_gpio_init(void)
{
    gpio_init_type gpio_init_struct;
    gpio_default_para_init(&gpio_init_struct);

    /* TX: PC10 */
    gpio_init_struct.gpio_drive_strength = GPIO_DRIVE_STRENGTH_MODERATE;
    gpio_init_struct.gpio_out_type = GPIO_OUTPUT_PUSH_PULL;
    gpio_init_struct.gpio_mode = GPIO_MODE_MUX;
    gpio_init_struct.gpio_pins = GPIO_PINS_10;
    gpio_init_struct.gpio_pull = GPIO_PULL_NONE;
    gpio_init(GPIOC, &gpio_init_struct);

    /* RX: PC11 */
    gpio_init_struct.gpio_mode = GPIO_MODE_INPUT;
    gpio_init_struct.gpio_pins = GPIO_PINS_11;
    gpio_init(GPIOC, &gpio_init_struct);

    /* RS485 浣胯兘寮曡剼锛岄粯璁ゆ帴鏀舵ā寮?*/
    gpio_init_struct.gpio_drive_strength = GPIO_DRIVE_STRENGTH_MODERATE;
    gpio_init_struct.gpio_out_type = GPIO_OUTPUT_PUSH_PULL;
    gpio_init_struct.gpio_mode = GPIO_MODE_OUTPUT;
    gpio_init_struct.gpio_pins = RS485_EN2_PIN;
    gpio_init_struct.gpio_pull = GPIO_PULL_NONE;
    gpio_init(RS485_EN2_GPIO_PORT, &gpio_init_struct);
    RS485_RX_EN();
}

static void usart_config_init(void)
{
    /* 鍩烘湰鍙傛暟 */
    usart_init(UART4, MODBUS_BAUDRATE, USART_DATA_8BITS, USART_STOP_1_BIT);
    usart_parity_selection_config(UART4, USART_PARITY_NONE);
    usart_hardware_flow_control_set(UART4, USART_HARDWARE_FLOW_NONE);
    usart_transmitter_enable(UART4, TRUE);
    usart_receiver_enable(UART4, TRUE);

    /* 浣胯兘 DMA 鎺ユ敹 */
    usart_dma_receiver_enable(UART4, TRUE);

    /*
     * 涓柇閰嶇疆锛?
     * IDLE锛氭€荤嚎绌洪棽妫€娴嬶紝鐢ㄤ簬閲嶇疆3.5T瀹氭椂鍣?
     * 鍙戦€佷娇鐢ㄨ疆璇㈡柟寮忥紝閬垮厤 TDBE/TDC 涓柇鐘舵€佹満鍗℃
     */
    usart_interrupt_enable(UART4, USART_IDLE_INT, TRUE);
    usart_interrupt_enable(UART4, USART_TDC_INT, FALSE);
    usart_interrupt_enable(UART4, USART_TDBE_INT, FALSE);

    /* NVIC 閰嶇疆
     * FreeRTOS 瑕佹眰璋冪敤 FromISR 鐨勪腑鏂紭鍏堢骇鏁板瓧
     * >= configMAX_SYSCALL_INTERRUPT_PRIORITY
     * 鏍规嵁浣犵殑 FreeRTOSConfig.h 璁剧疆
     */
    nvic_irq_enable(UART4_IRQn, 6, 0);

    usart_enable(UART4, TRUE);
}

static void usart_dma_init(void)
{
    dma_init_type dma_init_struct;

    dma_reset(DMA1_CHANNEL1);
    dma_default_para_init(&dma_init_struct);

    dma_init_struct.direction = DMA_DIR_PERIPHERAL_TO_MEMORY;
    dma_init_struct.memory_data_width = DMA_MEMORY_DATA_WIDTH_BYTE;
    dma_init_struct.memory_inc_enable = TRUE;
    dma_init_struct.peripheral_data_width = DMA_PERIPHERAL_DATA_WIDTH_BYTE;
    dma_init_struct.peripheral_inc_enable = FALSE;
    dma_init_struct.priority = DMA_PRIORITY_LOW;
    dma_init_struct.loop_mode_enable = FALSE; /* 鏅€氭ā寮忥紝甯х粨鏉熸墜鍔ㄩ噸鍚?*/
    dma_init(DMA1_CHANNEL1, &dma_init_struct);

    dma_flexible_config(DMA1, FLEX_CHANNEL1, DMA_FLEXIBLE_UART4_RX);

    /* FDT锛氱紦鍐插尯婊★紝璇存槑婧㈠嚭锛屽叧鎺塇DT鐢ㄤ笉涓?*/
    dma_interrupt_enable(DMA1_CHANNEL1, DMA_FDT_INT, TRUE);
    dma_interrupt_enable(DMA1_CHANNEL1, DMA_HDT_INT, FALSE);

    nvic_irq_enable(DMA1_Channel1_IRQn, 6, 0);

    /* 缁戝畾澶栬鍦板潃鍜屽唴瀛樺湴鍧€ */
    DMA1_CHANNEL1->paddr = (uint32_t)&UART4->dt;
    DMA1_CHANNEL1->maddr = (uint32_t)ctx_read_buf;
    dma_data_number_set(DMA1_CHANNEL1, AGILE_MODBUS_MAX_ADU_LENGTH);

    dma_channel_enable(DMA1_CHANNEL1, TRUE);
}

/*
 * 3.5T 纭欢瀹氭椂鍣ㄥ垵濮嬪寲锛圱MR6锛?
 * 鍙傝€?FreeModbus porttimer.c 瀹炵幇鎬濊矾锛?
 *   娉㈢壒鐜?> 19200 鈫?鍥哄畾 1750us
 *   娉㈢壒鐜?<= 19200 鈫?3.5 * 10 * 1000000 / baudrate us
 *
 * 璁℃暟鏃堕挓 = APB1 / (APB1_CLOCK_MHZ - 1 + 1) = 1MHz
 * 婧㈠嚭鍊?  = MODBUS_35T_US = 1750
 * 婧㈠嚭鏃堕棿 = 1750 * 1us = 1750us
 */
static void timer35_init(void)
{
    crm_clocks_freq_type crm_clocks;
    uint32_t frequency = 0;

    /* get crm_clocks */
    crm_clocks_freq_get(&crm_clocks);

    frequency = crm_clocks.apb1_freq * 2;
    /* time base configuration */
    crm_periph_clock_enable(CRM_TMR6_PERIPH_CLOCK, TRUE);

    tmr_base_init(TMR6,
                  modbus_timer_config(MODBUS_BAUDRATE) - 1U,
                  (frequency / 1000000U) - 1U);

    tmr_cnt_dir_set(TMR6, TMR_COUNT_UP);
    tmr_interrupt_enable(TMR6, TMR_OVF_INT, TRUE);

    nvic_irq_enable(TMR6_GLOBAL_IRQn, 6, 0);

    /* 鍏堜笉鍚姩锛屾敹鍒版暟鎹悗鎵嶅惎鍔?*/
    tmr_counter_enable(TMR6, FALSE);
}

void bsp_usart_init(void)
{
    /* Create sync objects once; bsp_usart_init() may be called again during debug. */
    if (rx_done_sem == NULL)
    {
        rx_done_sem = xSemaphoreCreateBinary();
    }
    if (tx_mutex == NULL)
    {
        tx_mutex = xSemaphoreCreateMutex();
    }

    configASSERT(rx_done_sem != NULL);
    configASSERT(tx_mutex != NULL);

    bsp_usart_blocking_tx_init(MODBUS_BAUDRATE);
    usart_dma_receiver_enable(UART4, TRUE);
    usart_dma_init();
    timer35_init();
    nvic_irq_enable(UART4_IRQn, 6, 0);
    uart4_rx_clear_pending();
    dma_rx_restart();
    uart4_rx_event_resume();
}

/* ================================================================
 * 瀵瑰鎺ュ彛
 * ================================================================ */

/*
 * rs485_send
 * 杞鍙戦€侊紝Modbus 搴旂瓟甯у緢鐭紝杩欐牱姣?TDBE/TDC 涓柇閾捐矾鏇寸ǔ
 */
int rs485_send(uint8_t *buf, int len)
{
    int rc = -1;
    BaseType_t mutex_locked = pdFALSE;

    if (buf == NULL || len <= 0)
        return -1;

    g_rs485_dbg_stage = 0x10U;
    g_rs485_dbg_len = (uint32_t)len;
    g_rs485_dbg_index = 0U;
    g_rs485_dbg_wait_flag = 0U;
    g_rs485_dbg_uart4_sts = UART4->sts;
    g_rs485_dbg_uart4_ctrl1 = UART4->ctrl1;

    if (tx_mutex != NULL)
    {
        mutex_locked = xSemaphoreTake(tx_mutex, pdMS_TO_TICKS(RS485_SEND_MUTEX_TIMEOUT_MS));
        if (mutex_locked != pdTRUE)
        {
            g_rs485_dbg_stage = 0xF1U;
            return -4;
        }
        else
        {
            g_rs485_dbg_stage = 0x11U;
        }
    }
    else
    {
        g_rs485_dbg_stage = 0xF0U;
    }

    uart4_rx_event_pause();
    g_rs485_dbg_stage = 0x12U;
    rc = rs485_send_blocking_raw(buf, len);
    g_rs485_dbg_uart4_sts = UART4->sts;
    g_rs485_dbg_uart4_ctrl1 = UART4->ctrl1;
    if (rc >= 0)
    {
        g_rs485_dbg_stage = 0x25U;
    }
    if (mutex_locked == pdTRUE)
    {
        xSemaphoreGive(tx_mutex);
    }
    return rc;
}

int rs485_send_blocking_raw(const uint8_t *buf, int len)
{
    int i;

    if (buf == NULL || len <= 0)
    {
        return -1;
    }

    g_rs485_dbg_stage = 0x20U;
    uart4_tx_prepare();
    RS485_TX_EN();
    g_rs485_dbg_stage = 0x21U;
    if (uart4_wait_flag(USART_TDBE_FLAG, RS485_TX_FLAG_TIMEOUT_MS) != 0)
    {
        uart4_tx_soft_recover();
        if (uart4_wait_flag(USART_TDBE_FLAG, RS485_TX_FLAG_TIMEOUT_MS) != 0)
        {
            g_rs485_dbg_stage = 0xE1U;
            g_rs485_dbg_uart4_sts = UART4->sts;
            g_rs485_dbg_uart4_ctrl1 = UART4->ctrl1;
            RS485_RX_EN();
            return -2;
        }
    }

    usart_flag_clear(UART4, USART_TDC_FLAG);

    for (i = 0; i < len; ++i)
    {
        g_rs485_dbg_stage = 0x22U;
        g_rs485_dbg_index = (uint32_t)i;
        g_rs485_dbg_wait_flag = USART_TDBE_FLAG;
        if (uart4_wait_flag(USART_TDBE_FLAG, RS485_TX_FLAG_TIMEOUT_MS) != 0)
        {
            g_rs485_dbg_stage = 0xE2U;
            g_rs485_dbg_uart4_sts = UART4->sts;
            g_rs485_dbg_uart4_ctrl1 = UART4->ctrl1;
            RS485_RX_EN();
            return -2;
        }

        usart_data_transmit(UART4, buf[i]);
    }

    g_rs485_dbg_stage = 0x23U;
    g_rs485_dbg_wait_flag = USART_TDC_FLAG;
    if (uart4_wait_flag(USART_TDC_FLAG, RS485_TX_FLAG_TIMEOUT_MS) != 0)
    {
        g_rs485_dbg_stage = 0xE3U;
        g_rs485_dbg_uart4_sts = UART4->sts;
        g_rs485_dbg_uart4_ctrl1 = UART4->ctrl1;
        RS485_RX_EN();
        return -3;
    }

    g_rs485_dbg_wait_flag = 0U;
    RS485_RX_EN();
    g_rs485_dbg_stage = 0x24U;
    return len;
}

/*
 * rs485_receive
 * 闃诲绛夊緟涓€甯ф帴鏀跺畬鎴?
 * 甯х粨鏉熺敱 TMR6 涓柇锛?.5T瓒呮椂锛夊垽鏂苟閲婃斁淇″彿閲?
 */
int rs485_receive(uint8_t *buf, int bufsz, uint32_t timeout_ms)
{
    (void)buf; /* DMA 鐩存帴鍐欏叆 ctx_read_buf锛屼笉闇€瑕佸啀鎷疯礉 */
    (void)bufsz;

    if (xSemaphoreTake(rx_done_sem, pdMS_TO_TICKS(timeout_ms)) != pdTRUE)
        return 0; /* 瓒呮椂锛屾棤鏁版嵁 */

    /* 搴旂敤灞傚彇璧版暟鎹悗锛岄噸鍚?DMA 鍑嗗鎺ユ敹涓嬩竴甯с€?
     * 杩欐牱澶勭悊鏈熼棿 DMA 淇濇寔鍋滄锛屼笉浼氳鐩?ctx_read_buf銆?*/
    uint16_t len = rx_frame_len;
    return (int)len;
}

void rs485_receive_rearm(void)
{
    rx_receiving = 0;
    rx_frame_len = 0;
    timer35_stop();
    uart4_rx_clear_pending();
    usart_dma_receiver_enable(UART4, TRUE);
    dma_rx_restart();
    uart4_rx_event_resume();
}

void bsp_usart_blocking_tx_init(uint32_t baudrate)
{
    usart_gpio_init();

    usart_reset(UART4);
    usart_init(UART4, baudrate, USART_DATA_8BITS, USART_STOP_1_BIT);
    usart_parity_selection_config(UART4, USART_PARITY_NONE);
    usart_hardware_flow_control_set(UART4, USART_HARDWARE_FLOW_NONE);
    usart_transmitter_enable(UART4, TRUE);
    usart_receiver_enable(UART4, TRUE);
    usart_dma_receiver_enable(UART4, FALSE);
    usart_dma_transmitter_enable(UART4, FALSE);

    usart_interrupt_enable(UART4, USART_IDLE_INT, FALSE);
    usart_interrupt_enable(UART4, USART_RDBF_INT, FALSE);
    usart_interrupt_enable(UART4, USART_TDC_INT, FALSE);
    usart_interrupt_enable(UART4, USART_TDBE_INT, FALSE);
    usart_interrupt_enable(UART4, USART_PERR_INT, FALSE);
    usart_interrupt_enable(UART4, USART_BF_INT, FALSE);
    usart_interrupt_enable(UART4, USART_ERR_INT, FALSE);
    usart_interrupt_enable(UART4, USART_CTSCF_INT, FALSE);

    NVIC_DisableIRQ(UART4_IRQn);
    NVIC_DisableIRQ(DMA1_Channel1_IRQn);
    NVIC_DisableIRQ(TMR6_GLOBAL_IRQn);

    usart_flag_clear(UART4,
                     USART_IDLEF_FLAG | USART_ROERR_FLAG |
                     USART_NERR_FLAG | USART_FERR_FLAG |
                     USART_TDC_FLAG);

    while (usart_flag_get(UART4, USART_RDBF_FLAG) != RESET)
    {
        (void)UART4->dt;
    }

    RS485_RX_EN();
    usart_enable(UART4, TRUE);
}

/* ================================================================
 * 涓柇澶勭悊鍑芥暟
 * ================================================================ */

void UART4_IRQHandler(void)
{
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    /*
     * IDLE锛氭€荤嚎鍑虹幇绌洪棽锛?涓瓧绗︽椂闂存棤鏁版嵁锛?
     * 璇存槑鏈夊瓧鑺傛鍦ㄥ埌鏉ユ垨鍒氭敹鍒帮紝閲嶇疆 3.5T 瀹氭椂鍣?
     *
     * AT32 娓呴櫎 IDLE 鏍囧織锛氬厛璇?STS 鍐嶈 DT
     * 涓嶈兘鐩存帴鐢?usart_flag_clear锛屽惁鍒欐棤鏁?
     */
    if (usart_interrupt_flag_get(UART4, USART_IDLEF_FLAG) != RESET)
    {
        (void)UART4->sts;
        (void)UART4->dt;

        rx_receiving = 1;
        timer35_reset_and_start();
    }

    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

/*
 * TMR6锛?.5T 瓒呮椂锛岃涓轰竴甯х粨鏉?
 * 鍙傝€?FreeModbus pxMBPortCBTimerExpired() 鐨勮Е鍙戞椂鏈?
 */
void TMR6_GLOBAL_IRQHandler(void)
{
    if (tmr_flag_get(TMR6, TMR_OVF_FLAG) != RESET)
    {
        tmr_flag_clear(TMR6, TMR_OVF_FLAG);
        timer35_stop();

        if (rx_receiving)
        {
            rx_receiving = 0;

            /* 鍋滄 DMA锛岃绠楁湰甯ч暱搴︼紙DMA 淇濇寔鍋滄锛岀瓑搴旂敤灞傚鐞嗗畬鍐嶉噸鍚級 */
            dma_channel_enable(DMA1_CHANNEL1, FALSE);
            uint16_t len = AGILE_MODBUS_MAX_ADU_LENGTH - dma_data_number_get(DMA1_CHANNEL1);

            if (len > 0)
            {
                rx_frame_len = len;

                /* 閫氱煡 modbus 浠诲姟澶勭悊 */
                BaseType_t xHigherPriorityTaskWoken = pdFALSE;
                xSemaphoreGiveFromISR(rx_done_sem,
                                      &xHigherPriorityTaskWoken);
                portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
            }
            else
            {
                /* 闀垮害涓?锛屽紓甯稿抚锛岀洿鎺ラ噸鍚?DMA */
                dma_rx_restart();
            }
        }
    }
}

/*
 * DMA1 Channel1锛氭帴鏀剁紦鍐插尯婊?
 * 姝ｅ父鎯呭喌涓嬩笉搴旇瑙﹀彂锛圡odbus甯ф渶澶?56瀛楄妭锛岀紦鍐插尯260瀛楄妭锛?
 * 瑙﹀彂璇存槑鏀跺埌浜嗗紓甯搁暱甯ф垨鎬荤嚎骞叉壈锛屼涪寮冩湰甯?
 */
void DMA1_Channel1_IRQHandler(void)
{
    if (dma_interrupt_flag_get(DMA1_FDT1_FLAG) != RESET)
    {
        dma_flag_clear(DMA1_FDT1_FLAG);

        /* 涓㈠純寮傚父甯э紝鍋滄瀹氭椂鍣紝閲嶅惎DMA */
        timer35_stop();
        rx_receiving = 0;
        dma_rx_restart();
    }
}

