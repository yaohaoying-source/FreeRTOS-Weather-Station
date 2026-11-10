#ifndef AIR780E_H
#define AIR780E_H

#include "stm32f1xx_hal.h"
/* 收到完整 MQTT 消息时，通知业务层 */
typedef void (*Air780E_MqttHandler_t)(const char *line);

void Air780E_SetMqttHandler(Air780E_MqttHandler_t handler);
/* 原始接收数组大小，以及一行文本的容量（包含末尾的字符串结束符）。 */
#define AIR780E_RX_SIZE   512U
#define AIR780E_LINE_SIZE 256U

/* 接收原始字节和中断观察量：事件次数不等于完整消息数量。 */
extern uint8_t g_air_rx_dma[AIR780E_RX_SIZE];
extern volatile uint16_t g_air_rx_pos;
extern volatile uint32_t g_air_rx_events;
extern volatile uint32_t g_air_uart_error;
extern volatile uint32_t g_air_uart_error_count;
extern volatile uint32_t g_air_rx_restart_count;
extern volatile uint32_t g_air_command_collision_count;

/* 仅 ReadLine 返回 1 时，g_air_reply 才代表本次取出的完整行。 */
extern char g_air_reply[AIR780E_LINE_SIZE];

/* 最近一条完整 +MSUB 消息及计数；新消息覆盖旧内容，不是消息队列。 */
extern char g_air_mqtt_message[AIR780E_LINE_SIZE];
extern volatile uint32_t g_air_mqtt_rx_count;
extern volatile uint32_t g_air_line_overflow; /* 过长文本行的丢弃次数 */

/* 以下接口由同一个网络任务调用，确保只有一个接收读取者。 */

/* 启动循环 DMA 接收，保存当前任务句柄；在网络任务中调用一次。 */
HAL_StatusTypeDef Air780E_Init(void);
HAL_StatusTypeDef Air780E_RestartRx(void);
uint8_t Air780E_HasRxError(void);

/* 只允许网络任务持有一个 AT 事务；查询等自定义等待也需成对调用。 */
HAL_StatusTypeDef Air780E_BeginCommand(void);
void Air780E_EndCommand(void);

/* 底层发送；调用前必须 BeginCommand，完成后必须 EndCommand。 */
HAL_StatusTypeDef Air780E_Send(const char *command);

/* 非阻塞取下一行：完整行返回 1，没有完整行返回 0，保留未完成内容。 */
uint8_t Air780E_ReadLine(void);

/* 发送并等待目标回复；timeout_ms 是发送完成后等待回复的时限。 */
HAL_StatusTypeDef Air780E_SendAndWait(
    const char *command,
    const char *expected,
    uint32_t timeout_ms);

#endif
