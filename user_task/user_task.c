#include "freertos_app.h"
#include "user_task.h"

/* user task header file include*/
#include "task_programmer.h"
#include "task_modbus_communication.h"

void user_task_create(void)
{
    task_programmer_create();
    task_modbus_communication_create();
}