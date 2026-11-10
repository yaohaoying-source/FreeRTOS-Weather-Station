#ifndef ONENET_CONNECT_H
#define ONENET_CONNECT_H

#include "stm32f1xx_hal.h"

/* 保存最近一次连接过程中各步骤的结果 */
typedef struct
{
    HAL_StatusTypeDef at_status;      /* 关闭回显 */
    HAL_StatusTypeDef config_status;  /* 配置 MQTT 身份 */
    HAL_StatusTypeDef tcp_status;     /* 建立 TCP 连接 */
    HAL_StatusTypeDef close_status;   /* 关闭旧 TCP 连接 */
    HAL_StatusTypeDef mqtt_status;    /* 建立 MQTT 会话 */
    HAL_StatusTypeDef sub_status;     /* 设置消息模式并订阅 */
} OneNET_ConnectInfo_t;

/* 尝试连接一次，成功返回 HAL_OK，失败返回对应的状态 */
HAL_StatusTypeDef OneNET_Connect(
    const char *config_command,
    OneNET_ConnectInfo_t *info);

/* 尝试关闭可能残留的 MQTT 和 TCP 连接 */
HAL_StatusTypeDef OneNET_Disconnect(void);

/* 查询失败时，使用 255 表示状态未知 */
#define ONENET_MQTT_UNKNOWN 255U

/*
 * 查询模块的 MQTT 状态。
 *
 * state：
 *   0：离线
 *   1：已登录
 *   2：未认证
 *   255：未知
 *
 * 返回 HAL_OK 才表示成功取得状态。
 */
HAL_StatusTypeDef OneNET_QueryMqttState(
    uint8_t *state,
    uint32_t timeout_ms);

#endif
