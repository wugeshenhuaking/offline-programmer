/* add user code begin Header */
/**
  ******************************************************************************
  * File Name          : freertos_app.c
  * Description        : Code for freertos applications
  */
/* add user code end Header */

/* Includes ------------------------------------------------------------------*/
#include "freertos_app.h"
#include "usb_app.h"

/* private includes ----------------------------------------------------------*/
/* add user code begin private includes */
#include <stdio.h>
#include <string.h>
#include "ff.h"
#include ".\bsp_25qxx\bsp_25qxx.h"
#include "user_task.h"
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
static TickType_t s_flash_next_scan_tick = 0;
static FATFS s_flash_fatfs;

/* 文件索引表 */
typedef struct {
    char    path[256];
    FSIZE_t size;
} FileEntry;

static FileEntry s_file_list[32];
static uint8_t   s_file_count = 0;

/* add user code end private variables */

/* private function prototypes --------------------------------------------*/
/* add user code begin function prototypes */
static void dump_flash_file_list(void);

/* add user code end function prototypes */

/* private user code ---------------------------------------------------------*/
/* add user code begin 0 */
// 用一个待处理目录队列，完全避开递归
static void scan_files_all(void)
{
    static char dir_queue[16][256];
    int head = 0, tail = 0;

    strncpy(dir_queue[tail++], "0:", 255);

    while(head < tail)
    {
        char *cur_path = dir_queue[head++];
        DIR     dir;
        FILINFO fno;
        char    subpath[256];

        if(f_opendir(&dir, cur_path) != FR_OK) continue;

        while(f_readdir(&dir, &fno) == FR_OK && fno.fname[0])
        {
            if(fno.fattrib & AM_HID) continue;
            if(fno.fattrib & AM_SYS) continue;
            if(fno.fname[0] == '.') continue;

            snprintf(subpath, sizeof(subpath), "%s/%s", cur_path, fno.fname);

            if(fno.fattrib & AM_DIR)
            {
                if(tail < 16)
                    strncpy(dir_queue[tail++], subpath, 255);
            }
            else if(s_file_count < 32)
            {
                strncpy(s_file_list[s_file_count].path, subpath, 255);
                s_file_list[s_file_count].size = fno.fsize;
                s_file_count++;
            }
        }
        f_closedir(&dir);
    }
}

static void dump_flash_file_list(void)
{
    s_file_count = 0;
    scan_files_all();

    printf("Found %d files:\r\n", s_file_count);
    for(int i = 0; i < s_file_count; i++)
    {
        printf("  [%d] %s  size=%lu\r\n",
               i + 1,
               s_file_list[i].path,
               (unsigned long)s_file_list[i].size);
    }
}

/* add user code end 0 */

/* task handler */
TaskHandle_t my_task01_handle;

/* Idle task control block and stack */
static StackType_t idle_task_stack[configMINIMAL_STACK_SIZE];
static StackType_t timer_task_stack[configTIMER_TASK_STACK_DEPTH];

static StaticTask_t idle_task_tcb;
static StaticTask_t timer_task_tcb;

/* External Idle and Timer task static memory allocation functions */
extern void vApplicationGetIdleTaskMemory( StaticTask_t ** ppxIdleTaskTCBBuffer, StackType_t ** ppxIdleTaskStackBuffer, uint32_t *pulIdleTaskStackSize );
extern void vApplicationGetTimerTaskMemory( StaticTask_t ** ppxTimerTaskTCBBuffer, StackType_t ** ppxTimerTaskStackBuffer, uint32_t * pulTimerTaskStackSize );

/*
  vApplicationGetIdleTaskMemory gets called when configSUPPORT_STATIC_ALLOCATION
  equals to 1 and is required for static memory allocation support.
*/
void vApplicationGetIdleTaskMemory( StaticTask_t ** ppxIdleTaskTCBBuffer, StackType_t ** ppxIdleTaskStackBuffer, uint32_t *pulIdleTaskStackSize )
{
  *ppxIdleTaskTCBBuffer = &idle_task_tcb;
  *ppxIdleTaskStackBuffer = &idle_task_stack[0];
  *pulIdleTaskStackSize = (uint32_t)configMINIMAL_STACK_SIZE;
}
/*
  vApplicationGetTimerTaskMemory gets called when configSUPPORT_STATIC_ALLOCATION
  equals to 1 and is required for static memory allocation support.
*/
void vApplicationGetTimerTaskMemory( StaticTask_t ** ppxTimerTaskTCBBuffer, StackType_t ** ppxTimerTaskStackBuffer, uint32_t * pulTimerTaskStackSize )
{
  *ppxTimerTaskTCBBuffer = &timer_task_tcb;
  *ppxTimerTaskStackBuffer = &timer_task_stack[0];
  *pulTimerTaskStackSize = (uint32_t)configTIMER_TASK_STACK_DEPTH;
}

/* add user code begin 1 */

/* add user code end 1 */

/**
  * @brief  initializes all task.
  * @param  none
  * @retval none
  */
void freertos_task_create(void)
{
  /* create my_task01 task */
  xTaskCreate(my_task01_func,
              "my_task01",
              128,
              NULL,
              0,
              &my_task01_handle);
}

/**
  * @brief  freertos init and begin run.
  * @param  none
  * @retval none
  */
void wk_freertos_init(void)
{
  /* add user code begin freertos_init 0 */

  /* add user code end freertos_init 0 */

  /* enter critical */
  taskENTER_CRITICAL();

  freertos_task_create();
	
  /* add user code begin freertos_init 1 */
   user_task_create();
  /* add user code end freertos_init 1 */

  /* exit critical */
  taskEXIT_CRITICAL();

  /* start scheduler */
  vTaskStartScheduler();
}

/**
  * @brief my_task01 function.
  * @param  none
  * @retval none
  */
void my_task01_func(void *pvParameters)
{
  /* add user code begin my_task01_func 0 */
  (void)pvParameters;

  /* add user code end my_task01_func 0 */

  /* add user code begin my_task01_func 2 */
    /* func_25qxx_init已在main.c中调用，此处不再重复 */
    /* func_25qxx_test(); */  /* 如需测试可取消注释 */
    uint32_t cnt = 0;
    /* 挂载只做一次 */
    FRESULT fr = f_mount(&s_flash_fatfs, "0:", 1);
    if(fr != FR_OK)
    {
        printf("FatFs mount failed: %u\r\n", (unsigned)fr);
    }
  /* add user code end my_task01_func 2 */

  /* Infinite loop */
  while(1)
  {
    /* when use usb,the function wk_usb_app_task() will be generated,
       which is the usb application layer code that users can improve themselves */
    wk_usb_app_task();

  /* add user code begin my_task01_func 1 */
    if(xTaskGetTickCount() >= s_flash_next_scan_tick)
    {
      dump_flash_file_list();
      s_flash_next_scan_tick = xTaskGetTickCount() + pdMS_TO_TICKS(5000);
    }

    printf("hello cnt = %lu\r\n",(unsigned long)cnt++);
    vTaskDelay(pdMS_TO_TICKS(1000));

  /* add user code end my_task01_func 1 */
  }
}


/* add user code begin 2 */

/* add user code end 2 */

