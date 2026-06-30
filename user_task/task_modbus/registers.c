#include <stdint.h>
#include <string.h>
#include "freertos_app.h"
#include "agile_modbus.h"
#include "agile_modbus_slave_util.h"
#include "task_programmer\task_programmer.h"

static uint16_t _tab_registers[10] = {0};

#define PROGRAMMER_HR_ENABLE_INDEX      0
#define PROGRAMMER_HR_BIN_INDEX         1
#define PROGRAMMER_HR_TRIGGER_INDEX     2
#define PROGRAMMER_HR_ENABLE_VALUE      0xFFFFU
#define PROGRAMMER_HR_TRIGGER_VALUE     1U

static uint8_t programmer_hr_to_bin_switch(uint16_t hr_value)
{
    if (hr_value == 1U || hr_value == 2U) {
        return (uint8_t)hr_value;
    }

    return 0U;
}

static void apply_programmer_control_registers(void)
{
    uint8_t bin_switch;

    if (g_programmer_state == PROGRAMMER_BUSY) {
        return;
    }

    if (_tab_registers[PROGRAMMER_HR_ENABLE_INDEX] != PROGRAMMER_HR_ENABLE_VALUE) {
        g_bin_file_switch = 0U;
        g_programmer_trigger = 0U;
        return;
    }

    bin_switch = programmer_hr_to_bin_switch(_tab_registers[PROGRAMMER_HR_BIN_INDEX]);
    g_bin_file_switch = bin_switch;

    if ((_tab_registers[PROGRAMMER_HR_TRIGGER_INDEX] == PROGRAMMER_HR_TRIGGER_VALUE) &&
        (bin_switch != 0U)) {
        g_programmer_trigger = 1U;
    } else {
        g_programmer_trigger = 0U;
    }
}

void modbus_programmer_registers_clear(void)
{
    taskENTER_CRITICAL();
    _tab_registers[PROGRAMMER_HR_ENABLE_INDEX] = 0U;
    _tab_registers[PROGRAMMER_HR_BIN_INDEX] = 0U;
    _tab_registers[PROGRAMMER_HR_TRIGGER_INDEX] = 0U;
    g_bin_file_switch = 0U;
    g_programmer_trigger = 0U;
    taskEXIT_CRITICAL();
}

static int get_map_buf(void *buf, int bufsz)
{
    uint16_t *ptr = (uint16_t *)buf;

    (void)bufsz;

    taskENTER_CRITICAL();
    for (int i = 0; i < sizeof(_tab_registers) / sizeof(_tab_registers[0]); i++) {
        ptr[i] = _tab_registers[i];
    }
    taskEXIT_CRITICAL();

    return 0;
}

static int set_map_buf(int index, int len, void *buf, int bufsz)
{
    uint16_t *ptr = (uint16_t *)buf;

    (void)bufsz;

    taskENTER_CRITICAL();
    for (int i = 0; i < len; i++) {
        _tab_registers[index + i] = ptr[index + i];
    }
    apply_programmer_control_registers();
    taskEXIT_CRITICAL();

    return 0;
}

const agile_modbus_slave_util_map_t register_maps[1] = {
    {0x0000, 0x0009, get_map_buf, set_map_buf}};
