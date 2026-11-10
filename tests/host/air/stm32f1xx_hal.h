#ifndef HOST_HAL_H
#define HOST_HAL_H
#include <stdint.h>
typedef enum { HAL_OK=0, HAL_ERROR=1, HAL_BUSY=2, HAL_TIMEOUT=3 } HAL_StatusTypeDef;
typedef struct { uint32_t counter; } DMA_HandleTypeDef;
typedef struct { void *Instance; DMA_HandleTypeDef *hdmarx; uint32_t ErrorCode; } UART_HandleTypeDef;
#define USART1 ((void *)0x11)
#define __HAL_DMA_GET_COUNTER(h) ((h)->counter)
#define __HAL_UART_CLEAR_OREFLAG(h) ((void)(h))
HAL_StatusTypeDef HAL_UART_AbortReceive(UART_HandleTypeDef *huart);
HAL_StatusTypeDef HAL_UARTEx_ReceiveToIdle_DMA(UART_HandleTypeDef *huart, uint8_t *data, uint16_t size);
HAL_StatusTypeDef HAL_UART_Transmit(UART_HandleTypeDef *huart, uint8_t *data, uint16_t size, uint32_t timeout);
uint32_t HAL_GetTick(void);
#endif
