/**
 * @file    SW_DP.c
 * @brief   SWD driver
 *
 * DAPLink Interface Firmware
 * Copyright (c) 2009-2016, ARM Limited, All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you may
 * not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * ----------------------------------------------------------------------
 *
 * $Date:        20. May 2015
 * $Revision:    V1.10
 *
 * Project:      CMSIS-DAP Source
 * Title:        SW_DP.c CMSIS-DAP SW DP I/O
 *
 *---------------------------------------------------------------------------*/

#include "DAP_config.h"
#include "DAP.h"


// SW Macros

#define PIN_SWCLK_SET PIN_SWCLK_TCK_SET
#define PIN_SWCLK_CLR PIN_SWCLK_TCK_CLR

#define SW_CLOCK_CYCLE()                \
  PIN_SWCLK_CLR();                      \
  PIN_DELAY();                          \
  PIN_SWCLK_SET();                      \
  PIN_DELAY()

#define SW_WRITE_BIT(bit)               \
  PIN_SWDIO_OUT(bit);                   \
  PIN_SWCLK_CLR();                      \
  PIN_DELAY();                          \
  PIN_SWCLK_SET();                      \
  PIN_DELAY()

#define SW_READ_BIT(bit)                \
  PIN_SWCLK_CLR();                      \
  PIN_DELAY();                          \
  bit = PIN_SWDIO_IN();                 \
  PIN_SWCLK_SET();                      \
  PIN_DELAY()

#define PIN_DELAY() PIN_DELAY_SLOW(DAP_Data.clock_delay)


//   Generate SWJ Sequence
//   count:  sequence bit count
//   data:   pointer to sequence bit data
//   return: none
#if ((DAP_SWD != 0) || (DAP_JTAG != 0))
void SWJ_Sequence(uint32_t count, const uint8_t *data)
{
    uint32_t val;
    uint32_t n;

    val = 0U;
    n = 0U;

    while(count--)
        {
            if(n == 0U)
                {
                    val = *data++;
                    n = 8U;
                }

            if(val & 1U)
                {
                    PIN_SWDIO_TMS_SET();
                }
            else
                {
                    PIN_SWDIO_TMS_CLR();
                }

            SW_CLOCK_CYCLE();
            val >>= 1;
            n--;
        }
}
#endif


#if (DAP_SWD != 0)

// Fast path assumes SWDIO and SWCLK share the same GPIO port.
#define SW_FAST_PORT       PIN_SWDIO_TMS_PORT
#define SW_FAST_SWDIO_SET  ((uint32_t)PIN_SWDIO_TMS)
#define SW_FAST_SWDIO_CLR  ((uint32_t)PIN_SWDIO_TMS << 16)
#define SW_FAST_SWCLK_SET  ((uint32_t)PIN_SWCLK_TCK)
#define SW_FAST_SWCLK_CLR  ((uint32_t)PIN_SWCLK_TCK << 16)
#define SW_FAST_CLR_BOTH   (((uint32_t)PIN_SWDIO_TMS | (uint32_t)PIN_SWCLK_TCK) << 16)

#define SW_CLOCK_CYCLE_FAST()         \
  do {                                \
    SW_FAST_PORT->scr = SW_FAST_SWCLK_CLR; \
    SW_FAST_PORT->scr = SW_FAST_SWCLK_SET; \
  } while (0)

#define SW_WRITE_BIT_FAST(bit)                                     \
  do {                                                             \
    if ((bit) & 1U) {                                              \
      SW_FAST_PORT->scr = SW_FAST_SWDIO_SET | SW_FAST_SWCLK_CLR;   \
    } else {                                                       \
      SW_FAST_PORT->scr = SW_FAST_CLR_BOTH;                        \
    }                                                              \
    SW_FAST_PORT->scr = SW_FAST_SWCLK_SET;                         \
  } while (0)

#define SW_READ_BIT_FAST(bit)             \
  do {                                    \
    SW_FAST_PORT->scr = SW_FAST_SWCLK_CLR; \
    (bit) = PIN_SWDIO_IN();               \
    SW_FAST_PORT->scr = SW_FAST_SWCLK_SET; \
  } while (0)

#define SW_CLOCK_CYCLE_SLOW()                \
  do {                                       \
    PIN_SWCLK_CLR();                         \
    PIN_DELAY_SLOW(DAP_Data.clock_delay);    \
    PIN_SWCLK_SET();                         \
    PIN_DELAY_SLOW(DAP_Data.clock_delay);    \
  } while (0)

#define SW_WRITE_BIT_SLOW(bit)               \
  do {                                       \
    PIN_SWDIO_OUT(bit);                      \
    PIN_SWCLK_CLR();                         \
    PIN_DELAY_SLOW(DAP_Data.clock_delay);    \
    PIN_SWCLK_SET();                         \
    PIN_DELAY_SLOW(DAP_Data.clock_delay);    \
  } while (0)

#define SW_READ_BIT_SLOW(bit)                \
  do {                                       \
    PIN_SWCLK_CLR();                         \
    PIN_DELAY_SLOW(DAP_Data.clock_delay);    \
    (bit) = PIN_SWDIO_IN();                  \
    PIN_SWCLK_SET();                         \
    PIN_DELAY_SLOW(DAP_Data.clock_delay);    \
  } while (0)

uint8_t SWD_TransferFast(uint32_t request, uint32_t *data)
{
    uint32_t ack;
    uint32_t bit;
    uint32_t val;
    uint32_t parity;
    uint32_t n;

    parity = 0U;
    SW_WRITE_BIT_FAST(1U);
    bit = request >> 0;
    SW_WRITE_BIT_FAST(bit);
    parity += bit;
    bit = request >> 1;
    SW_WRITE_BIT_FAST(bit);
    parity += bit;
    bit = request >> 2;
    SW_WRITE_BIT_FAST(bit);
    parity += bit;
    bit = request >> 3;
    SW_WRITE_BIT_FAST(bit);
    parity += bit;
    SW_WRITE_BIT_FAST(parity);
    SW_WRITE_BIT_FAST(0U);
    SW_WRITE_BIT_FAST(1U);

    PIN_SWDIO_OUT_DISABLE();
    for (n = DAP_Data.swd_conf.turnaround; n; n--) {
        SW_CLOCK_CYCLE_FAST();
    }

    SW_READ_BIT_FAST(bit);
    ack  = bit << 0;
    SW_READ_BIT_FAST(bit);
    ack |= bit << 1;
    SW_READ_BIT_FAST(bit);
    ack |= bit << 2;

    if (ack == DAP_TRANSFER_OK) {
        if (request & DAP_TRANSFER_RnW) {
            val = 0U;
            for (n = 32U; n; n--) {
                SW_READ_BIT_FAST(bit);
                val >>= 1;
                val  |= bit << 31;
            }
            parity = GetParity(val);
            SW_READ_BIT_FAST(bit);
            if ((parity ^ bit) & 1U) {
                ack = DAP_TRANSFER_ERROR;
            }
            if (data) {
                *data = val;
            }
            for (n = DAP_Data.swd_conf.turnaround; n; n--) {
                SW_CLOCK_CYCLE_FAST();
            }
            PIN_SWDIO_OUT_ENABLE();
        } else {
            for (n = DAP_Data.swd_conf.turnaround; n; n--) {
                SW_CLOCK_CYCLE_FAST();
            }
            PIN_SWDIO_OUT_ENABLE();
            val = *data;
            parity = GetParity(val);
            for (n = 32U; n; n--) {
                SW_WRITE_BIT_FAST(val);
                val >>= 1;
            }
            SW_WRITE_BIT_FAST(parity);
        }

        n = DAP_Data.transfer.idle_cycles;
        if (n) {
            SW_FAST_PORT->scr = SW_FAST_SWDIO_CLR;
            for (; n; n--) {
                SW_CLOCK_CYCLE_FAST();
            }
        }
        SW_FAST_PORT->scr = SW_FAST_SWDIO_SET;
        return ((uint8_t)ack);
    }

    if ((ack == DAP_TRANSFER_WAIT) || (ack == DAP_TRANSFER_FAULT)) {
        if (DAP_Data.swd_conf.data_phase && ((request & DAP_TRANSFER_RnW) != 0U)) {
            for (n = 32U + 1U; n; n--) {
                SW_CLOCK_CYCLE_FAST();
            }
        }
        for (n = DAP_Data.swd_conf.turnaround; n; n--) {
            SW_CLOCK_CYCLE_FAST();
        }
        PIN_SWDIO_OUT_ENABLE();
        if (DAP_Data.swd_conf.data_phase && ((request & DAP_TRANSFER_RnW) == 0U)) {
            SW_FAST_PORT->scr = SW_FAST_SWDIO_CLR;
            for (n = 32U + 1U; n; n--) {
                SW_CLOCK_CYCLE_FAST();
            }
        }
        SW_FAST_PORT->scr = SW_FAST_SWDIO_SET;
        return ((uint8_t)ack);
    }

    for (n = DAP_Data.swd_conf.turnaround + 32U + 1U; n; n--) {
        SW_CLOCK_CYCLE_FAST();
    }
    PIN_SWDIO_OUT_ENABLE();
    SW_FAST_PORT->scr = SW_FAST_SWDIO_SET;
    return ((uint8_t)ack);
}

uint8_t SWD_TransferSlow(uint32_t request, uint32_t *data)
{
    uint32_t ack;
    uint32_t bit;
    uint32_t val;
    uint32_t parity;
    uint32_t n;

    parity = 0U;
    SW_WRITE_BIT_SLOW(1U);
    bit = request >> 0;
    SW_WRITE_BIT_SLOW(bit);
    parity += bit;
    bit = request >> 1;
    SW_WRITE_BIT_SLOW(bit);
    parity += bit;
    bit = request >> 2;
    SW_WRITE_BIT_SLOW(bit);
    parity += bit;
    bit = request >> 3;
    SW_WRITE_BIT_SLOW(bit);
    parity += bit;
    SW_WRITE_BIT_SLOW(parity);
    SW_WRITE_BIT_SLOW(0U);
    SW_WRITE_BIT_SLOW(1U);

    PIN_SWDIO_OUT_DISABLE();
    for (n = DAP_Data.swd_conf.turnaround; n; n--) {
        SW_CLOCK_CYCLE_SLOW();
    }

    SW_READ_BIT_SLOW(bit);
    ack  = bit << 0;
    SW_READ_BIT_SLOW(bit);
    ack |= bit << 1;
    SW_READ_BIT_SLOW(bit);
    ack |= bit << 2;

    if (ack == DAP_TRANSFER_OK) {
        if (request & DAP_TRANSFER_RnW) {
            val = 0U;
            for (n = 32U; n; n--) {
                SW_READ_BIT_SLOW(bit);
                val >>= 1;
                val  |= bit << 31;
            }
            parity = GetParity(val);
            SW_READ_BIT_SLOW(bit);
            if ((parity ^ bit) & 1U) {
                ack = DAP_TRANSFER_ERROR;
            }
            if (data) {
                *data = val;
            }
            for (n = DAP_Data.swd_conf.turnaround; n; n--) {
                SW_CLOCK_CYCLE_SLOW();
            }
            PIN_SWDIO_OUT_ENABLE();
        } else {
            for (n = DAP_Data.swd_conf.turnaround; n; n--) {
                SW_CLOCK_CYCLE_SLOW();
            }
            PIN_SWDIO_OUT_ENABLE();
            val = *data;
            parity = GetParity(val);
            for (n = 32U; n; n--) {
                SW_WRITE_BIT_SLOW(val);
                val >>= 1;
            }
            SW_WRITE_BIT_SLOW(parity);
        }

        n = DAP_Data.transfer.idle_cycles;
        if (n) {
            PIN_SWDIO_OUT(0U);
            for (; n; n--) {
                SW_CLOCK_CYCLE_SLOW();
            }
        }
        PIN_SWDIO_OUT(1U);
        return ((uint8_t)ack);
    }

    if ((ack == DAP_TRANSFER_WAIT) || (ack == DAP_TRANSFER_FAULT)) {
        if (DAP_Data.swd_conf.data_phase && ((request & DAP_TRANSFER_RnW) != 0U)) {
            for (n = 32U + 1U; n; n--) {
                SW_CLOCK_CYCLE_SLOW();
            }
        }
        for (n = DAP_Data.swd_conf.turnaround; n; n--) {
            SW_CLOCK_CYCLE_SLOW();
        }
        PIN_SWDIO_OUT_ENABLE();
        if (DAP_Data.swd_conf.data_phase && ((request & DAP_TRANSFER_RnW) == 0U)) {
            PIN_SWDIO_OUT(0U);
            for (n = 32U + 1U; n; n--) {
                SW_CLOCK_CYCLE_SLOW();
            }
        }
        PIN_SWDIO_OUT(1U);
        return ((uint8_t)ack);
    }

    for (n = DAP_Data.swd_conf.turnaround + 32U + 1U; n; n--) {
        SW_CLOCK_CYCLE_SLOW();
    }
    PIN_SWDIO_OUT_ENABLE();
    PIN_SWDIO_OUT(1U);
    return ((uint8_t)ack);
}


// SWD Transfer I/O
//   request: A[3:2] RnW APnDP
//   data:    DATA[31:0]
//   return:  ACK[2:0]
uint8_t  SWD_Transfer(uint32_t request, uint32_t *data)
{
    if(DAP_Data.fast_clock)
        {
            return SWD_TransferFast(request, data);
        }
    else
        {
            return SWD_TransferSlow(request, data);
        }
}


#endif  /* (DAP_SWD != 0) */
