#ifndef TASK_PROGRAMMER_H
#define TASK_PROGRAMMER_H

#include <stdint.h>

/* 烧录触发标志：置 1 开始烧录，烧录完成后自动清 0 */
extern volatile uint8_t g_programmer_trigger;

/* bin 文件选择：0=空闲(默认), 1=烧录"01"开头bin, 2=烧录"02"开头bin,
 * 3=烧录"03"开头bin ... 烧录完成后自动归零。由 Modbus 设置。 */
extern volatile uint8_t g_bin_file_switch;

/* 烧录状态 */
typedef enum {
    PROGRAMMER_IDLE = 0,
    PROGRAMMER_BUSY,
    PROGRAMMER_DONE,
    PROGRAMMER_FAILED,
} programmer_state_t;

extern volatile programmer_state_t g_programmer_state;

void task_programmer_create(void);

#endif
