/* add user code begin Header */
/**
  **************************************************************************
  * @file     at32f403a_407_int.c
  * @brief    main interrupt service routines.
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

/* includes ------------------------------------------------------------------*/
#include "at32f403a_407_int.h"
#include "usb_app.h"
#include "wk_system.h"
#include "freertos_app.h"

/* private includes ----------------------------------------------------------*/
/* add user code begin private includes */

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

/* add user code end private variables */

/* private function prototypes --------------------------------------------*/
/* add user code begin function prototypes */

/* add user code end function prototypes */

/* private user code ---------------------------------------------------------*/
/* add user code begin 0 */

typedef struct
{
  uint32_t magic;
  uint32_t exc_return;
  uint32_t msp;
  uint32_t psp;
  uint32_t r0;
  uint32_t r1;
  uint32_t r2;
  uint32_t r3;
  uint32_t r12;
  uint32_t lr;
  uint32_t pc;
  uint32_t xpsr;
  uint32_t cfsr;
  uint32_t hfsr;
  uint32_t dfsr;
  uint32_t afsr;
  uint32_t mmfar;
  uint32_t bfar;
  uint32_t shcsr;
} hardfault_info_t;

volatile hardfault_info_t g_hardfault_info;

static void hardfault_save_context(uint32_t *stack_frame, uint32_t exc_return)
{
  g_hardfault_info.magic = 0x48464C54U; /* 'HFLT' */
  g_hardfault_info.exc_return = exc_return;
  g_hardfault_info.msp = __get_MSP();
  g_hardfault_info.psp = __get_PSP();

  if(stack_frame != 0)
  {
    g_hardfault_info.r0 = stack_frame[0];
    g_hardfault_info.r1 = stack_frame[1];
    g_hardfault_info.r2 = stack_frame[2];
    g_hardfault_info.r3 = stack_frame[3];
    g_hardfault_info.r12 = stack_frame[4];
    g_hardfault_info.lr = stack_frame[5];
    g_hardfault_info.pc = stack_frame[6];
    g_hardfault_info.xpsr = stack_frame[7];
  }

  g_hardfault_info.cfsr = SCB->CFSR;
  g_hardfault_info.hfsr = SCB->HFSR;
  g_hardfault_info.dfsr = SCB->DFSR;
  g_hardfault_info.afsr = SCB->AFSR;
  g_hardfault_info.mmfar = SCB->MMFAR;
  g_hardfault_info.bfar = SCB->BFAR;
  g_hardfault_info.shcsr = SCB->SHCSR;
}

/* add user code end 0 */

/* external variables ---------------------------------------------------------*/
/* add user code begin external variables */

/* add user code end external variables */

/**
  * @brief  this function handles nmi exception.
  * @param  none
  * @retval none
  */
void NMI_Handler(void)
{
  /* add user code begin NonMaskableInt_IRQ 0 */

  /* add user code end NonMaskableInt_IRQ 0 */

  /* add user code begin NonMaskableInt_IRQ 1 */

  /* add user code end NonMaskableInt_IRQ 1 */
}

/**
  * @brief  this function handles hard fault exception.
  * @param  none
  * @retval none
  */
__attribute__((naked)) void HardFault_Handler(void)
{
  __asm volatile
  (
    "tst lr, #4        \n"
    "ite eq            \n"
    "mrseq r0, msp     \n"
    "mrsne r0, psp     \n"
    "mov r1, lr        \n"
    "b HardFault_Handler_C \n"
  );
}

void HardFault_Handler_C(uint32_t *stack_frame, uint32_t exc_return)
{
  /* add user code begin HardFault_IRQ 0 */
  hardfault_save_context(stack_frame, exc_return);
  /* add user code end HardFault_IRQ 0 */

  while (1)
  {
    /* add user code begin W1_HardFault_IRQ 0 */

    /* add user code end W1_HardFault_IRQ 0 */
  }
}


/**
  * @brief  this function handles memory manage exception.
  * @param  none
  * @retval none
  */
void MemManage_Handler(void)
{
  /* add user code begin MemoryManagement_IRQ 0 */

  /* add user code end MemoryManagement_IRQ 0 */
  /* go to infinite loop when memory manage exception occurs */
  while (1)
  {
    /* add user code begin W1_MemoryManagement_IRQ 0 */

    /* add user code end W1_MemoryManagement_IRQ 0 */
  }
}

/**
  * @brief  this function handles bus fault exception.
  * @param  none
  * @retval none
  */
void BusFault_Handler(void)
{
  /* add user code begin BusFault_IRQ 0 */

  /* add user code end BusFault_IRQ 0 */
  /* go to infinite loop when bus fault exception occurs */
  while (1)
  {
    /* add user code begin W1_BusFault_IRQ 0 */

    /* add user code end W1_BusFault_IRQ 0 */
  }
}

/**
  * @brief  this function handles usage fault exception.
  * @param  none
  * @retval none
  */
void UsageFault_Handler(void)
{
  /* add user code begin UsageFault_IRQ 0 */

  /* add user code end UsageFault_IRQ 0 */
  /* go to infinite loop when usage fault exception occurs */
  while (1)
  {
    /* add user code begin W1_UsageFault_IRQ 0 */

    /* add user code end W1_UsageFault_IRQ 0 */
  }
}

/**
  * @brief  this function handles debug monitor exception.
  * @param  none
  * @retval none
  */
void DebugMon_Handler(void)
{
  /* add user code begin DebugMonitor_IRQ 0 */

  /* add user code end DebugMonitor_IRQ 0 */
  /* add user code begin DebugMonitor_IRQ 1 */

  /* add user code end DebugMonitor_IRQ 1 */
}

extern void xPortSysTickHandler(void);

/**
  * @brief  this function handles systick handler.
  * @param  none
  * @retval none
  */
void SysTick_Handler(void)
{
  /* add user code begin SysTick_IRQ 0 */

  /* add user code end SysTick_IRQ 0 */

#if (INCLUDE_xTaskGetSchedulerState == 1 )
  if (xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED)
  {
#endif /* INCLUDE_xTaskGetSchedulerState */
  xPortSysTickHandler();
#if (INCLUDE_xTaskGetSchedulerState == 1 )
  }
#endif /* INCLUDE_xTaskGetSchedulerState */

  /* add user code begin SysTick_IRQ 1 */

  /* add user code end SysTick_IRQ 1 */
}

/**
  * @brief  this function handles TMR8 Trigger and hall and TMR14 handler.
  * @param  none
  * @retval none
  */
void TMR8_TRG_HALL_TMR14_IRQHandler(void)
{
  /* add user code begin TMR8_TRG_HALL_TMR14_IRQ 0 */

  /* add user code end TMR8_TRG_HALL_TMR14_IRQ 0 */

  wk_timebase_handler();

  /* add user code begin TMR8_TRG_HALL_TMR14_IRQ 1 */

  /* add user code end TMR8_TRG_HALL_TMR14_IRQ 1 */
}

/**
  * @brief  this function handles USB Map Low handler.
  * @param  none
  * @retval none
  */
void USBFS_MAPL_IRQHandler(void)
{
  /* add user code begin USBFS_MAPL_IRQ 0 */

  /* add user code end USBFS_MAPL_IRQ 0 */

  wk_usbfs_irq_handler();

  /* add user code begin USBFS_MAPL_IRQ 1 */

  /* add user code end USBFS_MAPL_IRQ 1 */
}

/* add user code begin 1 */

/* add user code end 1 */
