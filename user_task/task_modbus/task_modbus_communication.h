#ifndef __TASK_MODBUS_COMMUNICATION_H__
#define __TASK_MODBUS_COMMUNICATION_H__


#include "user_task.h"
#include <agile_modbus.h>

extern uint8_t ctx_send_buf[AGILE_MODBUS_MAX_ADU_LENGTH];
extern uint8_t ctx_read_buf[AGILE_MODBUS_MAX_ADU_LENGTH];

#define MODBUS_PROGRAMMER_STATUS_IDLE    0x0000U
#define MODBUS_PROGRAMMER_STATUS_BUSY    0x0001U
#define MODBUS_PROGRAMMER_STATUS_DONE    0x0002U
#define MODBUS_PROGRAMMER_STATUS_FAILED  0x0003U

void task_modbus_communication_create(void);
void modbus_programmer_registers_clear(void);
void modbus_programmer_status_set(uint16_t status);


#endif // __TASK_MODBUS_COMMUNICATION_H__
