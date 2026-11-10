#ifndef HOST_TASK_H
#define HOST_TASK_H
#include <stdint.h>
uint32_t ulTaskNotifyTake(int clear, uint32_t ticks);
#endif
