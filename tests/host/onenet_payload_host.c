#include <stdint.h>
#include "stm32f1xx_hal.h"
char g_air_reply[256];
static uint32_t tick;
uint32_t HAL_GetTick(void) { return tick; }
uint32_t ulTaskNotifyTake(int clear, uint32_t n) { (void)clear; tick += n ? n : 1U; return 0; }
HAL_StatusTypeDef Air780E_BeginCommand(void) { return HAL_OK; }
void Air780E_EndCommand(void) {}
HAL_StatusTypeDef Air780E_Send(const char *s) { (void)s; return HAL_OK; }
uint8_t Air780E_ReadLine(void) { return 0; }
uint8_t Air780E_HasRxError(void) { return 0; }
#include "../../Core/Src/onenet_upload.c"
