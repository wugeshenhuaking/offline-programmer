#ifndef __TASK_MODBUS_COMMUNICATION_H__
#define __TASK_MODBUS_COMMUNICATION_H__


#include "user_task.h"
#include <agile_modbus.h>

extern uint8_t ctx_send_buf[AGILE_MODBUS_MAX_ADU_LENGTH];
extern uint8_t ctx_read_buf[AGILE_MODBUS_MAX_ADU_LENGTH];


void task_modbus_communication_create(void);
void modbus_programmer_registers_clear(void);


#endif // __TASK_MODBUS_COMMUNICATION_H__
