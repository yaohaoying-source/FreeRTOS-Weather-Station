#include "onenet_connect.h"
#include "onenet_config.h"
#include "air780e.h"
#include "FreeRTOS.h"
#include "task.h"
#include <string.h>
/*
 * 尝试连接一次 OneNET。
 *
 * config_command：main.c 中已有的 AT+MCONFIG 指令。
 * info：保存每一步的执行结果。
 *
 * 本函数不启动 DMA，也不负责循环重试。
 */
HAL_StatusTypeDef OneNET_Connect(
    const char *config_command,
    OneNET_ConnectInfo_t *info)
{
    if (config_command == 0 || info == 0)
    {
        return HAL_ERROR;
    }

    /* 每次连接前，把各阶段标记为“尚未完成” */
    info->at_status = HAL_BUSY;
    info->config_status = HAL_BUSY;
    info->tcp_status = HAL_BUSY;
    info->close_status = HAL_BUSY;
    info->mqtt_status = HAL_BUSY;
    info->sub_status = HAL_BUSY;

    /* 1. 关闭 AT 命令回显 */
    info->at_status = Air780E_SendAndWait(
        "ATE0\r\n",
        "OK",
        3000);

    if (info->at_status != HAL_OK)
    {
        return info->at_status;
    }

    /* 2. 配置 MQTT 身份信息 */
    info->config_status = Air780E_SendAndWait(
        config_command,
        "OK",
        3000);

    if (info->config_status != HAL_OK)
    {
        return info->config_status;
    }

    /* 3. 建立 TCP 连接 */
    info->tcp_status = Air780E_SendAndWait(
        "AT+MIPSTART=\"mqtts.heclouds.com\",1883\r\n",
        "CONNECT OK",
        60000);

    /*
     * 当前驱动约定：
     * 收到 ALREADY CONNECT 时返回 HAL_BUSY。
     */
    if (info->tcp_status == HAL_BUSY)
    {
        /* 尝试关闭可能存在的 MQTT 会话 */
        (void)Air780E_SendAndWait(
            "AT+MDISCONNECT\r\n",
            "OK",
            3000);

        /* 关闭旧 TCP 连接 */
        info->close_status = Air780E_SendAndWait(
            "AT+MIPCLOSE\r\n",
            "OK",
            5000);

        if (info->close_status != HAL_OK)
        {
            return info->close_status;
        }

        /* 再次建立 TCP */
        info->tcp_status = Air780E_SendAndWait(
            "AT+MIPSTART=\"mqtts.heclouds.com\",1883\r\n",
            "CONNECT OK",
            60000);
    }

    if (info->tcp_status != HAL_OK)
    {
        return info->tcp_status;
    }

    /* 4. 登录 MQTT，等到 CONNACK OK 才算成功 */
    info->mqtt_status = Air780E_SendAndWait(
        "AT+MCONNECT=1,300\r\n",
        "CONNACK OK",
        10000);

    if (info->mqtt_status != HAL_OK)
    {
        return info->mqtt_status;
    }
    /* 5. 设置订阅消息输出模式 */
    info->sub_status = Air780E_SendAndWait(
        "AT+MQTTMSGSET=0\r\n",
        "OK",
        3000);

    if (info->sub_status != HAL_OK)
    {
        return info->sub_status;
    }

    /* 6. 订阅属性上报结果 */
    info->sub_status = Air780E_SendAndWait(
        "AT+MSUB=\"" ONENET_TOPIC_PREFIX "/thing/property/post/reply\",0\r\n",
        "SUBACK",
        5000);

    if (info->sub_status != HAL_OK)
    {
        return info->sub_status;
    }
    /* 订阅事件上报结果 */
    info->sub_status = Air780E_SendAndWait(
        "AT+MSUB=\"" ONENET_TOPIC_PREFIX "/thing/event/post/reply\",0\r\n",
        "SUBACK",
        5000);
    if (info->sub_status != HAL_OK)
    {
        return info->sub_status;
    }
    /* 7. 订阅云端属性设置 */
    info->sub_status = Air780E_SendAndWait(
        "AT+MSUB=\"" ONENET_TOPIC_PREFIX "/thing/property/set\",0\r\n",
        "SUBACK",
        5000);

    return info->sub_status;
}

/*
 * 已确认模块在线时，按顺序关闭 MQTT 和 TCP。
 * 任一步失败立即返回，避免超时后盲目继续发送新命令。
 */
HAL_StatusTypeDef OneNET_Disconnect(void)
{
    HAL_StatusTypeDef status = Air780E_SendAndWait(
        "AT+MDISCONNECT\r\n",
        "OK",
        3000);
    if (status != HAL_OK)
        return status;
    return Air780E_SendAndWait(
        "AT+MIPCLOSE\r\n",
        "OK",
        5000);
}
/*
 * 解析模块返回的状态行，例如：
 *
 * +MQTTSTATU :1
 * +MQTTSTATU: 1
 *
 * 解析成功返回 1，失败返回 0。
 */
static uint8_t OneNET_ParseMqttState(
    const char *line,
    uint8_t *state)
{
    const char *p;
    uint8_t value;

    if (line == 0 || state == 0 ||
        strncmp(line, "+MQTTSTATU", 10U) != 0)
    {
        return 0U;
    }

    p = line + 10U;

    /* 跳过冒号前的空格 */
    while (*p == ' ' || *p == '\t')
    {
        p++;
    }

    if (*p != ':')
    {
        return 0U;
    }

    p++;

    /* 跳过冒号后的空格 */
    while (*p == ' ' || *p == '\t')
    {
        p++;
    }

    /* 目前只接受官方定义的 0、1、2 */
    if (*p < '0' || *p > '2')
    {
        return 0U;
    }

    value = (uint8_t)(*p - '0');
    p++;

    while (*p == ' ' || *p == '\t')
    {
        p++;
    }

    if (*p != '\0')
    {
        return 0U;
    }

    *state = value;
    return 1U;
}

/*
 * 查询模块当前的 MQTT 状态。
 *
 * 本函数只允许由网络任务调用：
 * 发送 AT+MQTTSTATU，等待状态行和最后的 OK。
 */
static HAL_StatusTypeDef OneNET_QueryMqttStateOwned(
    uint8_t *state,
    uint32_t timeout_ms)
{
    HAL_StatusTypeDef send_status;
    uint32_t start;
    uint32_t elapsed;
    uint32_t drained = 0U;

    uint8_t parsed_state = ONENET_MQTT_UNKNOWN;
    uint8_t got_state = 0U;

    if (state == 0)
    {
        return HAL_ERROR;
    }

    *state = ONENET_MQTT_UNKNOWN;

    /* 先读取之前已经到达的完整行 */
    while (Air780E_ReadLine())
    {
        if (++drained >= 32U || Air780E_HasRxError())
            return HAL_BUSY;
    }

    send_status = Air780E_Send("AT+MQTTSTATU\r\n");

    if (send_status != HAL_OK)
    {
        return send_status;
    }

    start = HAL_GetTick();

    while ((uint32_t)(HAL_GetTick() - start) < timeout_ms)
    {
        if (Air780E_HasRxError())
            return HAL_ERROR;
        while (Air780E_ReadLine())
        {
            if (Air780E_HasRxError())
                return HAL_ERROR;
            if ((uint32_t)(HAL_GetTick() - start) >= timeout_ms)
                return HAL_TIMEOUT;
            if (OneNET_ParseMqttState(g_air_reply, &parsed_state))
            {
                got_state = 1U;
            }
            else if (strcmp(g_air_reply, "OK") == 0)
            {
                /* 必须先收到状态行，再收到 OK */
                if (got_state == 0U)
                {
                    return HAL_ERROR;
                }

                *state = parsed_state;
                return HAL_OK;
            }
            else if (strcmp(g_air_reply, "ERROR") == 0 ||
                     strncmp(g_air_reply, "+CME ERROR", 10U) == 0)
            {
                return HAL_ERROR;
            }
        }

        elapsed = (uint32_t)(HAL_GetTick() - start);

        if (elapsed >= timeout_ms)
        {
            break;
        }

        /* 等待串口通知，期间让出 CPU */
        (void)ulTaskNotifyTake(
            pdTRUE,
            pdMS_TO_TICKS(timeout_ms - elapsed));
    }

    return HAL_TIMEOUT;
}

HAL_StatusTypeDef OneNET_QueryMqttState(uint8_t *state, uint32_t timeout_ms)
{
    HAL_StatusTypeDef status = Air780E_BeginCommand();
    if (status != HAL_OK)
        return status;
    status = OneNET_QueryMqttStateOwned(state, timeout_ms);
    Air780E_EndCommand();
    return status;
}

