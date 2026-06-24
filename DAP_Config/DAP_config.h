#ifndef __DAP_CONFIG_H__
#define __DAP_CONFIG_H__

#define CPU_CLOCK 240000000 ///< Specifies the CPU Clock in Hz

#define IO_PORT_WRITE_CYCLES 2 ///< I/O Cycles: 2=default, 1=Cortex-M0+ fast I/0

#define DAP_SWD 1 ///< SWD Mode:  1 = available, 0 = not available

#define DAP_JTAG 0 ///< JTAG Mode: 0 = not available

#define DAP_JTAG_DEV_CNT 8 ///< Maximum number of JTAG devices on scan chain

#define DAP_DEFAULT_PORT 1 ///< Default JTAG/SWJ Port Mode: 1 = SWD, 2 = JTAG.

#define DAP_DEFAULT_SWJ_CLOCK 10000000 ///< Default SWD/JTAG clock frequency in Hz

/// Maximum Package Size for Command and Response data.
#define DAP_PACKET_SIZE 64 ///< USB: 64 = Full-Speed, 1024 = High-Speed.

/// Maximum Package Buffers for Command and Response data.
#define DAP_PACKET_COUNT 1 ///< Buffers: 64 = Full-Speed, 4 = High-Speed.

/// Indicate that UART Serial Wire Output (SWO) trace is available.
#define SWO_UART 0 ///< SWO UART:  1 = available, 0 = not available

#define SWO_UART_MAX_BAUDRATE 10000000U ///< SWO UART Maximum Baudrate in Hz

/// Indicate that Manchester Serial Wire Output (SWO) trace is available.
#define SWO_MANCHESTER 0 ///< SWO Manchester:  1 = available, 0 = not available

#define SWO_BUFFER_SIZE 4096U ///< SWO Trace Buffer Size in bytes (must be 2^n)

/// Debug Unit is connected to fixed Target Device.
#define TARGET_DEVICE_FIXED 0 ///< Target Device: 1 = known, 0 = unknown;

//**************************************************************************************************
/**
JTAG I/O Pin                 | SWD I/O Pin          | CMSIS-DAP Hardware pin mode
---------------------------- | -------------------- | ---------------------------------------------
TCK: Test Clock              | SWCLK: Clock         | Output Push/Pull
TMS: Test Mode Select        | SWDIO: Data I/O      | Output Push/Pull; Input (for receiving data)
TDI: Test Data Input         |                      | Output Push/Pull
TDO: Test Data Output        |                      | Input
nTRST: Test Reset (optional) |                      | Output Open Drain with pull-up resistor
nRESET: Device Reset         | nRESET: Device Reset | Output Open Drain with pull-up resistor

DAP Hardware I/O Pin Access Functions
*/
#include "at32f403a_407.h"

// Configure DAP I/O pins ------------------------------

#define PIN_MODE_MASK(pin) (((uint32_t)0x0F) << ((pin) << 2))
#define PIN_MASK(pin) (((uint16_t)0x01) << (pin))

#define PIN_nRESET PIN_MASK(PIN_nRESET_PIN)
#define PIN_SWDIO_TMS PIN_MASK(PIN_SWDIO_TMS_PIN)
#define PIN_SWCLK_TCK PIN_MASK(PIN_SWCLK_TCK_PIN)

/*********************这里为SWD引脚定义**********************
*无需配置引脚时钟，引脚的时钟在SWD_PORT_RCC_INIT()自动初始化
*************************************************************/
// SWDIO/TMS Pin
#define PIN_SWDIO_TMS_PORT GPIOC
#define PIN_SWDIO_TMS_PIN 0

// SWCLK/TCK Pin
#define PIN_SWCLK_TCK_PORT GPIOC
#define PIN_SWCLK_TCK_PIN 1

// nRESET Pin
//#define PIN_nRESET_PORT     GPIOB   //预留硬件复位引脚
//#define PIN_nRESET_PIN			0

static __inline uint32_t PIN_SWCLK_TCK_IN(void) { return (PIN_SWCLK_TCK_PORT->idt & PIN_SWCLK_TCK) ? 1 : 0; }

static __inline void PIN_SWCLK_TCK_SET(void) { PIN_SWCLK_TCK_PORT->scr = PIN_SWCLK_TCK; }

static __inline void PIN_SWCLK_TCK_CLR(void) { PIN_SWCLK_TCK_PORT->clr = PIN_SWCLK_TCK; }

// SWDIO/TMS Pin I/O --------------------------------------

// Current status of the SWDIO/TMS DAP hardware I/O pin
static __inline uint32_t PIN_SWDIO_TMS_IN(void) { return (PIN_SWDIO_TMS_PORT->idt & PIN_SWDIO_TMS) ? 1 : 0; }

static __inline void PIN_SWDIO_TMS_SET(void) { PIN_SWDIO_TMS_PORT->scr = PIN_SWDIO_TMS; }

static __inline void PIN_SWDIO_TMS_CLR(void) { PIN_SWDIO_TMS_PORT->clr = PIN_SWDIO_TMS; }

static __inline uint32_t PIN_SWDIO_IN(void)
{
    if (PIN_SWDIO_TMS_PORT->idt & PIN_SWDIO_TMS)
        return 1;
    return 0;
}

static __inline void PIN_SWDIO_OUT(uint32_t bit)
{
    if (bit & 1)
        PIN_SWDIO_TMS_PORT->scr = PIN_SWDIO_TMS;
    else
        PIN_SWDIO_TMS_PORT->clr = PIN_SWDIO_TMS;
}

// AT32 GPIO nibble values for SWDIO:
// output push-pull, stronger drive = 0x1
// input with pull-up              = 0x8
#define PIN_SWDIO_OUTPUT_CFG 0x1U
#define PIN_SWDIO_INPUT_CFG  0x8U

static __inline void PIN_SWDIO_OUT_ENABLE(void)
{
    uint32_t cfglr = PIN_SWDIO_TMS_PORT->cfglr;

    cfglr &= ~PIN_MODE_MASK(PIN_SWDIO_TMS_PIN);
    cfglr |= (PIN_SWDIO_OUTPUT_CFG << (PIN_SWDIO_TMS_PIN << 2));
    PIN_SWDIO_TMS_PORT->cfglr = cfglr;
}

static __inline void PIN_SWDIO_OUT_DISABLE(void)
{
    uint32_t cfglr = PIN_SWDIO_TMS_PORT->cfglr;

    // Input pull-up selection depends on the output data latch level.
    PIN_SWDIO_TMS_PORT->scr = PIN_SWDIO_TMS;
    cfglr &= ~PIN_MODE_MASK(PIN_SWDIO_TMS_PIN);
    cfglr |= (PIN_SWDIO_INPUT_CFG << (PIN_SWDIO_TMS_PIN << 2));
    PIN_SWDIO_TMS_PORT->cfglr = cfglr;
}

// nTRST Pin I/O -------------------------------------------

static __inline uint32_t PIN_nTRST_IN(void) { return 0; }

static __inline void PIN_nTRST_OUT(uint32_t bit) { }

// nRESET Pin I/O------------------------------------------
static __inline uint32_t PIN_nRESET_IN(void) { return 0; }

static __inline void PIN_nRESET_OUT(uint32_t bit) { }

//**************************************************************************************************
/** Connect LED: is active when the DAP hardware is connected to a debugger
    Running LED: is active when program execution in target started
*/

static __inline void LED_CONNECTED_OUT(uint32_t bit) { }

static __inline void LED_RUNNING_OUT(uint32_t bit) { }

static uint32_t RESET_TARGET(void)
{
    return (0); // change to '1' when a device reset sequence is implemented
}

#endif // __DAP_CONFIG_H__
