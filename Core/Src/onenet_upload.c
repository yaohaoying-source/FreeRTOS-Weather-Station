#include "onenet_upload.h"
#include "onenet_config.h"
#include "air780e.h"
#include "FreeRTOS.h"
#include "task.h"
#include <string.h>
#include <stdio.h>

/* 从 JSON 中读取一个无符号整数。
 * quoted=1：值带引号，例如 "id":"8"。
 * quoted=0：值不带引号，例如 "code":200。
 */
static uint8_t ReadNumber(
    const char *json,
    const char *key,
    uint8_t quoted,
    uint32_t *value)
{
    const char *p;
    uint32_t number = 0;
    uint32_t digit;

    p = strstr(json, key);
    if (p == NULL)
        return 0;

    p += strlen(key);
    p += strspn(p, " \t\r\n");

    if (*p != ':')
        return 0;

    p++;
    p += strspn(p, " \t\r\n");

    if (quoted)
    {
        if (*p != '"')
            return 0;
        p++;
    }

    if (*p < '0' || *p > '9')
        return 0;

    while (*p >= '0' && *p <= '9')
    {
        digit = (uint32_t)(*p - '0');

        /* 防止数字超出 uint32_t。 */
        if (number > (0xFFFFFFFFUL - digit) / 10UL)
            return 0;

        number = number * 10UL + digit;
        p++;
    }

    if (quoted)
    {
        if (*p != '"')
            return 0;
        p++;
    }

    p += strspn(p, " \t\r\n");

    if (*p != ',' && *p != '}')
        return 0;

    *value = number;
    return 1;
}

/* 两种上报回复格式相同，只是 MQTT 主题不同。 */
static uint8_t ParseReplyForTopic(
    const char *line, const char *topic, uint32_t *id, uint32_t *code)
{
    const char *p;
    const char *json;
    size_t topic_length;
    uint8_t quoted;

    if (line == NULL || topic == NULL || id == NULL || code == NULL)
        return 0U;

    if (strncmp(line, "+MSUB:", 6U) != 0)
        return 0U;

    p = line + 6U;
    p += strspn(p, " \t");
    quoted = (*p == '"');
    if (quoted)
        p++;

    topic_length = strlen(topic);
    if (strncmp(p, topic, topic_length) != 0)
        return 0U;
    p += topic_length;

    if (quoted)
    {
        if (*p != '"')
            return 0U;
        p++;
    }

    p += strspn(p, " \t");
    if (*p != ',')
        return 0U;

    json = strchr(p, '{');
    if (json == NULL)
        return 0U;

    return ReadNumber(json, "\"id\"", 1U, id) &&
           ReadNumber(json, "\"code\"", 0U, code);
}

/* 保持原有的属性上报回复接口。 */
uint8_t OneNET_ParseUploadReply(
    const char *line, uint32_t *id, uint32_t *code)
{
    return ParseReplyForTopic(
        line,
        "" ONENET_TOPIC_PREFIX "/thing/property/post/reply",
        id, code);
}

/* 事件上报只识别 event/post/reply，避免和属性上报混淆。 */
static uint8_t ParseEventReply(
    const char *line, uint32_t *id, uint32_t *code)
{
    return ParseReplyForTopic(
        line,
        "" ONENET_TOPIC_PREFIX "/thing/event/post/reply",
        id, code);
}

/*
 * 所有追加都检查空间；失败时调用者不得发送截断的 AT 报文。
 * 先添加始终有效的设备状态，再按传感器有效标志逐项追加。
 */
int OneNET_FormatWeather(char *buffer, size_t capacity, uint32_t message_id,
                         const OneNET_WeatherReport_t *data)
{
    size_t used = 0U;
    int written;
    if (buffer == NULL || data == NULL || capacity == 0U)
        return -1;

#define APPEND_WEATHER(...) do { \
        if (used >= capacity) return -1; \
        written = snprintf(buffer + used, capacity - used, __VA_ARGS__); \
        if (written < 0 || (size_t)written >= capacity - used) return -1; \
        used += (size_t)written; \
    } while (0)

    APPEND_WEATHER(
        "AT+MPUB=\"" ONENET_TOPIC_PREFIX "/thing/property/post\",0,0,"
        "\"{\\22id\\22:\\22%lu\\22,\\22version\\22:\\221.0\\22,"
        "\\22params\\22:{\\22pm25_limit\\22:{\\22value\\22:%.2f},"
        "\\22buzzer_enable\\22:{\\22value\\22:%s},"
        "\\22buzzer_state\\22:{\\22value\\22:%s}",
        (unsigned long)message_id, data->pm25_limit,
        data->buzzer_enable ? "true" : "false",
        data->buzzer_output ? "true" : "false");

    if (data->dht_valid)
        APPEND_WEATHER(",\\22temp\\22:{\\22value\\22:%u.0},"
                       "\\22humi\\22:{\\22value\\22:%u}",
                       (unsigned int)data->temperature, (unsigned int)data->humidity);
    if (data->bmp_valid)
        APPEND_WEATHER(",\\22press\\22:{\\22value\\22:%.2f}", data->pressure);
    if (data->light_valid)
        APPEND_WEATHER(",\\22light\\22:{\\22value\\22:%u}",
                       (unsigned int)data->light_percent);
    if (data->dust_valid)
        APPEND_WEATHER(",\\22pm25\\22:{\\22value\\22:%.2f},"
                       "\\22pm25_alarm\\22:{\\22value\\22:%s}",
                       data->pm25, data->pm25_alarm ? "true" : "false");
    APPEND_WEATHER("}}\"\r\n");
#undef APPEND_WEATHER
    return (int)used;
}

int OneNET_FormatPm25Event(char *buffer, size_t capacity, uint32_t message_id,
                           float pm25_value, float limit_value)
{
    int length;
    if (buffer == NULL || capacity == 0U)
        return -1;
    length = snprintf(buffer, capacity,
        "AT+MPUB=\"" ONENET_TOPIC_PREFIX "/thing/event/post\",0,0,"
        "\"{\\22id\\22:\\22%lu\\22,\\22version\\22:\\221.0\\22,"
        "\\22params\\22:{\\22pm25_alert\\22:{\\22value\\22:{"
        "\\22pm25_value\\22:%.2f,\\22limit_value\\22:%.2f}}}}\"\r\n",
        (unsigned long)message_id, (double)pm25_value, (double)limit_value);
    return (length > 0 && (size_t)length < capacity) ? length : -1;
}

uint8_t OneNET_PendingStart(OneNET_PendingUpload_t *pending, uint8_t kind,
                            uint32_t message_id, uint32_t now_ms, uint32_t timeout_ms)
{
    if (pending == NULL || pending->active ||
        (kind != ONENET_UPLOAD_PROPERTY && kind != ONENET_UPLOAD_EVENT) ||
        timeout_ms == 0U)
        return 0U;
    pending->result.id = message_id;
    pending->result.code = 0U;
    pending->result.state = UPLOAD_WAITING;
    pending->started_ms = now_ms;
    pending->timeout_ms = timeout_ms;
    pending->kind = kind;
    pending->ready = 0U;
    pending->active = 1U;
    return 1U;
}

void OneNET_PendingOnLine(OneNET_PendingUpload_t *pending, const char *line)
{
    uint32_t id, code;
    uint8_t matched;
    if (pending == NULL || !pending->active || pending->ready || line == NULL)
        return;
    matched = (pending->kind == ONENET_UPLOAD_EVENT)
            ? ParseEventReply(line, &id, &code)
            : OneNET_ParseUploadReply(line, &id, &code);
    if (matched && id == pending->result.id)
    {
        pending->result.code = code;
        pending->result.state = (code == 200U) ? UPLOAD_SUCCESS : UPLOAD_FAILED;
        pending->ready = 1U;
    }
}

void OneNET_PendingFail(OneNET_PendingUpload_t *pending, uint8_t state)
{
    if (pending != NULL && pending->active)
    {
        pending->result.code = 0U;
        pending->result.state = state;
        pending->ready = 1U;
    }
}

uint8_t OneNET_PendingTake(OneNET_PendingUpload_t *pending, uint32_t now_ms,
                           UploadResult_t *result)
{
    if (pending == NULL || result == NULL || !pending->active)
        return 0U;
    if (!pending->ready &&
        (uint32_t)(now_ms - pending->started_ms) >= pending->timeout_ms)
        OneNET_PendingFail(pending, UPLOAD_TIMEOUT);
    if (!pending->ready)
        return 0U;
    *result = pending->result;
    pending->active = 0U;
    return 1U;
}

typedef uint8_t (*OneNET_ReplyParser)(
    const char *line, uint32_t *id, uint32_t *code);

/* 一次上传只等待本次 message_id 的结果。 */
static HAL_StatusTypeDef OneNET_UploadWithParserOwned(
    const char *command,
    uint32_t message_id,
    UploadResult_t *result,
    uint32_t timeout_ms,
    OneNET_ReplyParser parse_reply)
{
    HAL_StatusTypeDef at_status;
    uint32_t start;
    uint32_t elapsed;
    uint32_t drained = 0U;
    uint32_t reply_id;
    uint32_t reply_code;
    uint8_t have_reply = 0;

    if (command == NULL || result == NULL)
        return HAL_ERROR;

    result->id = message_id;
    result->code = 0;
    result->state = UPLOAD_TIMEOUT;

    /* 清理之前已经到达的数据，不把旧回复当成本次结果。 */
    while (Air780E_ReadLine())
    {
        if (++drained >= 32U || Air780E_HasRxError())
            return HAL_BUSY;
    }

    at_status = Air780E_Send(command);
    if (at_status != HAL_OK)
    {
        result->state = UPLOAD_FAILED;
        return at_status;
    }

    /* 串口发送完成，还需要等模块自己的 OK。 */
    at_status = HAL_TIMEOUT;
    start = HAL_GetTick();

    while ((uint32_t)(HAL_GetTick() - start) < timeout_ms)
    {
        if (Air780E_HasRxError())
        {
            result->state = UPLOAD_FAILED;
            return HAL_ERROR;
        }
        while (Air780E_ReadLine())
        {
            if (Air780E_HasRxError())
            {
                result->state = UPLOAD_FAILED;
                return HAL_ERROR;
            }
            if ((uint32_t)(HAL_GetTick() - start) >= timeout_ms)
                return HAL_TIMEOUT;
            if (strcmp(g_air_reply, "OK") == 0)
            {
                at_status = HAL_OK;
            }
            else if (strcmp(g_air_reply, "ERROR") == 0 ||
                     strncmp(g_air_reply, "+CME ERROR", 10U) == 0 ||
                     strcmp(g_air_reply, "CONNECT FAIL") == 0 ||
                     strcmp(g_air_reply, "CLOSED") == 0)
            {
                if (!have_reply)
                    result->state = UPLOAD_FAILED;

                return HAL_ERROR;
            }

            if (parse_reply(
                    g_air_reply, &reply_id, &reply_code))
            {
                /* 收到旧编号的回复时，不更新本次结果。 */
                if (reply_id == message_id)
                {
                    result->code = reply_code;
                    result->state = (reply_code == 200U)
                                  ? UPLOAD_SUCCESS
                                  : UPLOAD_FAILED;
                    have_reply = 1;
                }
            }

            if (at_status == HAL_OK && have_reply)
                return at_status;
        }

        elapsed = (uint32_t)(HAL_GetTick() - start);
        if (elapsed >= timeout_ms)
            break;

        /* 等待期间让出 CPU，采集和显示任务仍可运行。 */
        (void)ulTaskNotifyTake(
            pdTRUE, pdMS_TO_TICKS(timeout_ms - elapsed));
    }

    return at_status;
}

static HAL_StatusTypeDef OneNET_UploadWithParser(
    const char *command, uint32_t message_id, UploadResult_t *result,
    uint32_t timeout_ms, OneNET_ReplyParser parse_reply)
{
    HAL_StatusTypeDef status = Air780E_BeginCommand();
    if (status != HAL_OK)
    {
        if (result != NULL)
        {
            result->id = message_id;
            result->code = 0U;
            result->state = UPLOAD_FAILED;
        }
        return status;
    }
    status = OneNET_UploadWithParserOwned(command, message_id, result,
                                          timeout_ms, parse_reply);
    Air780E_EndCommand();
    return status;
}
/* 原来的属性上传函数，调用方式不变。 */
HAL_StatusTypeDef OneNET_Upload(
    const char *command, uint32_t message_id,
    UploadResult_t *result, uint32_t timeout_ms)
{
    return OneNET_UploadWithParser(
        command, message_id, result, timeout_ms, OneNET_ParseUploadReply);
}

/* 组装 PM2.5 超限事件，并等待对应的平台回复。仅网络任务调用。 */
HAL_StatusTypeDef OneNET_UploadPm25Event(
    float pm25_value, float limit_value, uint32_t message_id,
    UploadResult_t *result, uint32_t timeout_ms)
{
    static char command[256];
    int length;

    if (result == NULL)
        return HAL_ERROR;

    result->id = message_id;
    result->code = 0U;
    result->state = UPLOAD_FAILED;

    length = snprintf(
        command, sizeof(command),
        "AT+MPUB=\"" ONENET_TOPIC_PREFIX "/thing/event/post\",0,0,"
        "\"{\\22id\\22:\\22%lu\\22,\\22version\\22:\\221.0\\22,"
        "\\22params\\22:{\\22pm25_alert\\22:{\\22value\\22:{"
        "\\22pm25_value\\22:%.2f,\\22limit_value\\22:%.2f}}}}\"\r\n",
        (unsigned long)message_id, (double)pm25_value, (double)limit_value);

    if (length <= 0 || length >= (int)sizeof(command))
        return HAL_ERROR;

    return OneNET_UploadWithParser(
        command, message_id, result, timeout_ms, ParseEventReply);
}
