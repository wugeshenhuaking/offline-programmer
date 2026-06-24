#include "freertos_app.h"
#include "task_programmer.h"
#include "ff.h"
#include <stdio.h>
#include <string.h>

#include "SWD_host.h"
#include "SWD_flash.h"
#include "AT32F415_128.c"


#define LOG_TAG "PROG"
#define LOG_MODULE_LVL LOG_LVL_DEBUG
#include "log.h"
/* ===================== 烧录目标 MCU 的 flash 参数 ===================== */
#define USER_MCU_FLASH_PAGE_SIZE   (1024 * 1)      /* AT32F415 页大小 2KB */
#define USER_MCU_FLASH_START_ADDR  0x08000000

/*
 * 待烧录的 bin 文件所在文件夹（位于 W25Q64 文件系统中）。
 * "单层照明程序" 的 GBK 编码用 \x 转义，避免源文件编码差异导致乱码。
 * 如需切换文件夹（如"双层照明程序"），修改此宏即可。
 */
#define PROGRAMMER_BIN_FOLDER  "0:/\xB5\xA5\xB2\xE3\xD5\xD5\xC3\xF7\xB3\xCC\xD0\xF2"

/* g_bin_file_switch 取值：0=空闲，1=烧录"01"开头bin，2=烧录"02"开头bin，
 * 3=烧录"03"开头bin ... 前缀由 switch 值动态生成（%02u），无需维护数组。 */
#define BIN_SWITCH_MIN  1
#define BIN_SWITCH_MAX  99

uint32_t Flash_Page_Size  = USER_MCU_FLASH_PAGE_SIZE;
uint32_t Flash_Start_Addr = USER_MCU_FLASH_START_ADDR;

TaskHandle_t task_programmer_handle;

/* 烧录触发标志：置 1 才开始烧录，烧录完成后自动清 0 */
volatile uint8_t g_programmer_trigger = 0;

/* bin 文件选择：0=空闲(默认), 1=烧录01开头bin, 2=烧录02开头bin */
volatile uint8_t g_bin_file_switch = 0;

/* 烧录状态 */
volatile programmer_state_t g_programmer_state = PROGRAMMER_IDLE;

/* 读文件缓冲区（按页大小，静态分配避免占用任务栈） */
static uint8_t s_page_buf[USER_MCU_FLASH_PAGE_SIZE];

/* 回读校验缓冲区 */
static uint8_t s_verify_buf[USER_MCU_FLASH_PAGE_SIZE];

/* ===================== 20s 进度看门狗 ===================== */
#define PROG_WDOG_TIMEOUT_MS    20000   /* 20s 无进展则超时 */
#define PROG_WDOG_PERIOD_MS     1000    /* 看门狗检查周期 1s */

static TimerHandle_t      s_prog_wdog_timer  = NULL;
static volatile TickType_t g_prog_last_progress_tick = 0;   /* 最近一次进展时刻 */
static volatile uint8_t    g_prog_timeout_flag = 0;         /* 1=已超时，要求中止烧录 */

/*
 * 在指定文件夹下查找以 prefix 开头、.bin 结尾的文件。
 * 找到则把完整路径填入 out_path 并返回 FR_OK，否则返回 FR_NO_FILE。
 */
static FRESULT find_bin_file_by_prefix(const char *folder, const char *prefix,
                                       char *out_path, uint32_t out_path_size)
{
    DIR     dir;
    FILINFO fno;
    FRESULT fr;
    uint32_t prefix_len = strlen(prefix);

    fr = f_opendir(&dir, folder);
    if(fr != FR_OK)
    {
        log_error("Open folder failed: %u", (unsigned)fr);
        return fr;
    }

    while(f_readdir(&dir, &fno) == FR_OK && fno.fname[0])
    {
        if(fno.fattrib & AM_DIR)  continue;
        if(fno.fname[0] == '.')   continue;

        /* 检查文件名前缀 */
        if(strncmp(fno.fname, prefix, prefix_len) != 0)
            continue;

        /* 检查 .bin 扩展名（不区分大小写） */
        const char *dot = strrchr(fno.fname, '.');
        if(dot == NULL)
            continue;
        if(strlen(dot) != 4)
            continue;
        if(!(dot[1] == 'b' || dot[1] == 'B') ||
           !(dot[2] == 'i' || dot[2] == 'I') ||
           !(dot[3] == 'n' || dot[3] == 'N'))
            continue;

        snprintf(out_path, out_path_size, "%s/%s", folder, fno.fname);
        f_closedir(&dir);
        return FR_OK;
    }

    f_closedir(&dir);
    return FR_NO_FILE;
}

/* 标准 CRC32（多项式 0xEDB88320） */
static uint32_t crc32_calc(const uint8_t *data, uint32_t len)
{
    uint32_t crc = 0xFFFFFFFF;
    for(uint32_t i = 0; i < len; i++)
    {
        crc ^= data[i];
        for(uint32_t b = 0; b < 8; b++)
        {
            if(crc & 1)
                crc = (crc >> 1) ^ 0xEDB88320;
            else
                crc >>= 1;
        }
    }
    return crc ^ 0xFFFFFFFF;
}

/* 喂狗：每完成一个步骤调用一次 */
static inline void prog_feed_wdog(void)
{
    g_prog_last_progress_tick = xTaskGetTickCount();
}

/* 检查是否已超时，超时则返回 1 */
static inline uint8_t prog_check_timeout(void)
{
    return g_prog_timeout_flag;
}

/**
 * @brief  看门狗定时器回调（运行在 Timer 服务任务中）
 *         若烧录进行中且 20s 内无进展，则置超时标志、复位状态/触发变量
 */
static void prog_wdog_callback(TimerHandle_t xTimer)
{
    (void)xTimer;
    if(g_programmer_state == PROGRAMMER_BUSY)
    {
        TickType_t elapsed = xTaskGetTickCount() - g_prog_last_progress_tick;
        if(elapsed > pdMS_TO_TICKS(PROG_WDOG_TIMEOUT_MS))
        {
            if(g_prog_timeout_flag == 0)
            {
                g_prog_timeout_flag = 1;
                g_programmer_state  = PROGRAMMER_FAILED;
                g_programmer_trigger = 0;   /* 退出烧录模式，允许重新触发 */
                g_bin_file_switch = 0;     /* 烧录完成，文件选择归零 */
                log_error("TIMEOUT! No progress for %dms, abort.", PROG_WDOG_TIMEOUT_MS);
            }
        }
    }
}

/**
 * @brief  执行一次完整的 SWD 烧录流程
 * @retval 0 成功，非 0 失败（错误码）
 */
static uint8_t programmer_do_flash(void)
{
    FIL      file;
    FRESULT  fr;
    UINT     br;
    uint32_t idcode = 0;
    FSIZE_t  file_size;
    uint32_t addr;
    uint32_t programmed = 0;

    log_info("Starting SWD...");
    prog_feed_wdog();

    /* 1. SWD 初始化 */
    if(!swd_init_debug())
    {
        log_error("SWD init failed");
        return 1;
    }
    prog_feed_wdog();
    log_info("SWD init done");

    /* 2. 读取 IDCODE */
    swd_read_idcode(&idcode);
    prog_feed_wdog();
    log_info("IDCODE: 0x%08X", (unsigned int)idcode);

    /* 3. 根据 g_bin_file_switch 动态生成前缀，在文件夹中查找对应 bin 文件 */
    if(g_bin_file_switch < BIN_SWITCH_MIN || g_bin_file_switch > BIN_SWITCH_MAX)
    {
        log_error("Invalid switch=%u (must be %d..%d)",
                  (unsigned)g_bin_file_switch, BIN_SWITCH_MIN, BIN_SWITCH_MAX);
        return 2;
    }
    char prefix[4];
    snprintf(prefix, sizeof(prefix), "%02u", (unsigned)g_bin_file_switch);

    char bin_path[256];
    fr = find_bin_file_by_prefix(PROGRAMMER_BIN_FOLDER, prefix, bin_path, sizeof(bin_path));
    if(fr != FR_OK)
    {
        log_error("No bin file found with prefix '%s' in folder %s",
                  prefix, PROGRAMMER_BIN_FOLDER);
        return 2;
    }

    log_info("Bin switch=%u, path=%s", (unsigned)g_bin_file_switch, bin_path);
    fr = f_open(&file, bin_path, FA_READ);
    if(fr != FR_OK)
    {
        log_error("Open bin failed: %u", (unsigned)fr);
        return 2;
    }
    prog_feed_wdog();
    file_size = f_size(&file);
    log_info("Bin size: %lu bytes", (unsigned long)file_size);

    if(file_size == 0)
    {
        f_close(&file);
        log_error("Bin is empty");
        return 3;
    }

    /* 4. 初始化目标 flash */
    if(target_flash_init(Flash_Start_Addr) != ERROR_SUCCESS)
    {
        log_error("target_flash_init failed");
        f_close(&file);
        return 4;
    }
    if(prog_check_timeout()) { f_close(&file); return 10; }
    prog_feed_wdog();

    /* 5. 擦除整个目标 flash（全片擦除） */
    log_info("Erasing whole chip...");

    if(target_flash_erase_chip() != ERROR_SUCCESS)
    {
        log_error("Erase chip failed");
        f_close(&file);
        return 5;
    }
    prog_feed_wdog();
    if(prog_check_timeout()) { f_close(&file); return 10; }
    log_info("Erase done");

    /* 6. 逐页读取 bin 并编程 */
    log_info("Programming...");
    addr = Flash_Start_Addr;
    while(programmed < file_size)
    {
        UINT to_read = (file_size - programmed > Flash_Page_Size)
                       ? Flash_Page_Size
                       : (UINT)(file_size - programmed);

        /* 清空缓冲区，保证最后一页不足部分为 0xFF（flash 擦除态） */
        memset(s_page_buf, 0xFF, sizeof(s_page_buf));

        fr = f_read(&file, s_page_buf, to_read, &br);
        if(fr != FR_OK || br != to_read)
        {
            log_error("Read file failed at offset %lu", (unsigned long)programmed);
            f_close(&file);
            return 6;
        }
        prog_feed_wdog();

        uint32_t crc_w = crc32_calc(s_page_buf, Flash_Page_Size);

        /* 打印：当前烧录地址、有效数据长度、CRC、前16字节 */
        log_debug("Addr=0x%08X Len=%u CRC=0x%08X",
                  (unsigned int)addr, (unsigned)to_read, (unsigned int)crc_w);
        log_debug("  Data[0..15]: %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X",
                  s_page_buf[0], s_page_buf[1], s_page_buf[2], s_page_buf[3],
                  s_page_buf[4], s_page_buf[5], s_page_buf[6], s_page_buf[7],
                  s_page_buf[8], s_page_buf[9], s_page_buf[10], s_page_buf[11],
                  s_page_buf[12], s_page_buf[13], s_page_buf[14], s_page_buf[15]);

        error_t pe = target_flash_program_page(addr, s_page_buf, Flash_Page_Size);
        if(pe != ERROR_SUCCESS)
        {
            log_error("Program page FAILED at 0x%08X, error=%d",
                      (unsigned int)addr, (int)pe);
            f_close(&file);
            return 7;
        }
        if(prog_check_timeout()) { f_close(&file); return 10; }
        prog_feed_wdog();

        /* 写后回读校验 */
        if(!swd_read_memory(addr, s_verify_buf, Flash_Page_Size))
        {
            log_error("Readback FAILED at 0x%08X", (unsigned int)addr);
            f_close(&file);
            return 8;
        }
        prog_feed_wdog();
        uint32_t crc_r = crc32_calc(s_verify_buf, Flash_Page_Size);
        if(crc_r == crc_w)
        {
            log_debug("  Readback CRC=0x%08X  OK", (unsigned int)crc_r);
        }
        else
        {
            log_debug("  Readback CRC=0x%08X  MISMATCH!", (unsigned int)crc_r);
        }

        if(crc_r != crc_w)
        {
            log_error("  Verify[0..15]: %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X",
                      s_verify_buf[0], s_verify_buf[1], s_verify_buf[2], s_verify_buf[3],
                      s_verify_buf[4], s_verify_buf[5], s_verify_buf[6], s_verify_buf[7],
                      s_verify_buf[8], s_verify_buf[9], s_verify_buf[10], s_verify_buf[11],
                      s_verify_buf[12], s_verify_buf[13], s_verify_buf[14], s_verify_buf[15]);
            f_close(&file);
            return 9;
        }

        addr       += Flash_Page_Size;
        programmed += to_read;
    }
    log_info("Program done, %lu bytes", (unsigned long)programmed);

    f_close(&file);

    /* 7. 复位目标运行 */
    swd_set_target_reset(0);
    log_info("Target reset & run");

    return 0;
}

void task_programmer_func(void *pvParameters)
{
    (void)pvParameters;

    while(1)
    {
        if(g_programmer_trigger == 1)
        {
            /* 校验 g_bin_file_switch 必须在有效范围内，否则不烧录 */
            if(g_bin_file_switch < BIN_SWITCH_MIN || g_bin_file_switch > BIN_SWITCH_MAX)
            {
                log_error("g_bin_file_switch=%u, must be %d..%d, abort.",
                          (unsigned)g_bin_file_switch, BIN_SWITCH_MIN, BIN_SWITCH_MAX);
                g_programmer_trigger = 0;
                vTaskDelay(pdMS_TO_TICKS(100));
                continue;
            }

            g_programmer_state = PROGRAMMER_BUSY;
            g_prog_timeout_flag = 0;
            prog_feed_wdog();

            /* 启动进度看门狗（周期 1s） */
            if(s_prog_wdog_timer != NULL)
                xTimerStart(s_prog_wdog_timer, 0);

            /*
             * 挂起 my_task01，避免 dump_flash_file_list()(FatFs 访问 W25Q64)
             * 与烧录任务的 f_read 并发访问 W25Q64 的 SPI 总线。
             */
            if(my_task01_handle != NULL)
            {
                vTaskSuspend(my_task01_handle);
                vTaskDelay(pdMS_TO_TICKS(50));  /* 确保 my_task01 完全挂起 */
            }

            uint8_t ret = programmer_do_flash();

            /*
             * 无论烧录成功还是失败，都清理 SWD 状态（复位目标 + swd_off）。
             * 避免上次失败后目标 MCU 残留 debug/halt 状态，导致下次触发时
             * RESET_PROGRAM 的 halt 等待异常甚至卡死。
             * target_flash_uninit 内部走 RESET_RUN（无死循环），安全可重入。
             */
            target_flash_uninit();

            /* 烧录完成，恢复 my_task01 */
            if(my_task01_handle != NULL)
                vTaskResume(my_task01_handle);

            /* 停止看门狗 */
            if(s_prog_wdog_timer != NULL)
                xTimerStop(s_prog_wdog_timer, 0);

            if(ret == 0)
            {
                g_programmer_state = PROGRAMMER_DONE;
                log_info("SUCCESS");
            }
            else if(ret == 10)
            {
                /* 超时：状态/触发变量已在看门狗回调中复位，这里补充打印 */
                log_error("FAILED (timeout)");
            }
            else
            {
                g_programmer_state = PROGRAMMER_FAILED;
                log_error("FAILED (%u)", (unsigned)ret);
            }
            g_programmer_trigger = 0;   /* 退出烧录模式，允许重新触发 */
            g_bin_file_switch = 0;     /* 烧录完成，文件选择归零 */
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void task_programmer_create(void)
{
  /* 创建进度看门狗定时器（周期 1s，启动后才开始计时）
   * 已屏蔽：调试阶段发现 SWD 底层有阻塞调用，软件看门狗无法将其打断，
   *         反而增加复杂度。先注释掉，待 SWD 稳定后再启用。
   */
#if 0
  s_prog_wdog_timer = xTimerCreate("prog_wdog",
                                   pdMS_TO_TICKS(PROG_WDOG_PERIOD_MS),
                                   pdTRUE,      /* 自动重载 */
                                   NULL,
                                   prog_wdog_callback);
#endif

  /* create task_programmer task */
  xTaskCreate(task_programmer_func,
              "task_programmer",
              2048,
              NULL,
              1,
              &task_programmer_handle);
}
