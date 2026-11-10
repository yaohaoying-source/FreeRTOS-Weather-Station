#ifndef HOST_AIR_H
#define HOST_AIR_H
#include "stm32f1xx_hal.h"
extern char g_air_reply[256];
HAL_StatusTypeDef Air780E_BeginCommand(void);
void Air780E_EndCommand(void);
HAL_StatusTypeDef Air780E_Send(const char *command);
uint8_t Air780E_ReadLine(void);
uint8_t Air780E_HasRxError(void);
HAL_StatusTypeDef Air780E_SendAndWait(const char *, const char *, uint32_t);
#endif
