#include "freertos_app.h"
#include "task_programmer.h"
#include "user_task.h"

void user_task_create(void)
{
    task_programmer_create();
}