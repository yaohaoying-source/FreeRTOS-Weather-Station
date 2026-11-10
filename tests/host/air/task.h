#ifndef HOST_TASK_H
#define HOST_TASK_H
#include "FreeRTOS.h"
typedef void *TaskHandle_t;
TaskHandle_t xTaskGetCurrentTaskHandle(void);
uint32_t ulTaskNotifyTake(BaseType_t clear, TickType_t ticks);
void vTaskNotifyGiveFromISR(TaskHandle_t task, BaseType_t *woken);
#endif
