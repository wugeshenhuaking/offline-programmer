/* add user code begin Header */
/**
  **************************************************************************
  * @file     usbd_app.c
  * @brief    usb device app
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

#include "usb_conf.h"
#include "wk_system.h"

#include "usbd_int.h"
#include "msc_class.h"
#include "msc_desc.h"

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

usbd_core_type usb_core_dev;


/* private user code ---------------------------------------------------------*/
/* add user code begin 0 */

/* add user code end 0 */

/**
  * @brief  usb application initialization
  * @param  none
  * @retval none
  */
void wk_usb_app_init(void)
{
  /* add user code begin usb_app_init 0 */

  /* add user code end usb_app_init 0 */

  /*fs1 device msc*/
  usbd_core_init(&usb_core_dev, USB, &msc_class_handler, &msc_desc_handler, 0);

  /* enable usb pull-up */
  usbd_connect(&usb_core_dev);

  /* add user code begin usb_app_init 1 */

  /* add user code end usb_app_init 1 */
}

/**
  * @brief  usb application task
  * @param  none
  * @retval none
  */
void wk_usb_app_task(void)
{
  /* add user code begin usb_app_task 0 */

  /* add user code end usb_app_task 0 */

  /* fs1 device msc */
  /*
  Implement the storage interface functions in msc_diskio.c, including msc_disk_read,
  msc_disk_write and msc_disk_capacity functions.
  */

  /* add user code begin usb_app_task 2 */

  /* add user code end usb_app_task 2 */
}

/**
  * @brief  usb interrupt handler
  * @param  none
  * @retval none
  */
void wk_usbfs_irq_handler(void)
{
  /* add user code begin otgfs1_irq_handler 0 */

  /* add user code end otgfs1_irq_handler 0 */

  usbd_irq_handler(&usb_core_dev);

  /* add user code begin otgfs1_irq_handler 1 */

  /* add user code end otgfs1_irq_handler 1 */
}

/**
  * @brief  usb delay function
  * @param  ms: delay number of milliseconds.
  * @retval none
  */
void usb_delay_ms(uint32_t ms)
{
  /* add user code begin delay_ms 0 */

  /* add user code end delay_ms 0 */

  wk_delay_ms(ms);

  /* add user code begin delay_ms 1 */

  /* add user code end delay_ms 1*/
}

/* add user code begin 1 */

/* add user code end 1 */
