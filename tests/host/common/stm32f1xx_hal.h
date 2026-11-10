#ifndef HOST_HAL_H
#define HOST_HAL_H
#include <stdint.h>
typedef enum { HAL_OK=0, HAL_ERROR=1, HAL_BUSY=2, HAL_TIMEOUT=3 } HAL_StatusTypeDef;
uint32_t HAL_GetTick(void);
#endif
