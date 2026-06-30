#include "freertos_app.h"
#include "user_task.h"
#include <stdio.h>

/* user task header file include*/
#include "task_programmer\task_programmer.h"
#include "task_modbus\task_modbus_communication.h"

void user_task_create(void)
{
    printf("[BOOT] user_task_create start\r\n");
    task_programmer_create();
    printf("[BOOT] task_programmer_create done\r\n");
    task_modbus_communication_create();
    printf("[BOOT] task_modbus_communication_create done\r\n");
}