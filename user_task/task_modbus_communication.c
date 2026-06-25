#include "task_modbus_communication.h"

TaskHandle_t task_modbus_communication_handle;


void task_modbus_communication_func(void *pvParameters)
{
    (void)pvParameters;

    while(1)
    {
      vTaskDelay(100);
    }
}

void task_modbus_communication_create(void)
{
  /* create task_modbus_communication task */
  xTaskCreate(task_modbus_communication_func,
              "task_modbus_communication",
              2048,
              NULL,
              1,
              &task_modbus_communication_handle);
}