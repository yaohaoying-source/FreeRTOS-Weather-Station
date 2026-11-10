#ifndef ONENET_UPLOAD_H
#define ONENET_UPLOAD_H

#include "stm32f1xx_hal.h"
#include <stddef.h>

/* state 使用 uint8_t 保存，字符串只在显示时选择。 */
#define UPLOAD_FAILED   0U
#define UPLOAD_SUCCESS  1U
#define UPLOAD_WAITING  2U
#define UPLOAD_TIMEOUT  3U
#define UPLOAD_IDLE     4U

typedef struct
{
    uint32_t id;       /* 本次上传编号 */
    uint32_t code;     /* 平台返回码；没有平台回复时为 0 */
    uint8_t state;     /* 上述状态值 */
} UploadResult_t;

/* 构造上报报文时只加入有效传感器；配置和蜂鸣器输出始终上报。 */
typedef struct
{
    uint8_t dht_valid, temperature, humidity;
    uint8_t bmp_valid, light_valid, light_percent, dust_valid;
    double pressure, pm25, pm25_limit;
    uint8_t buzzer_enable, pm25_alarm, buzzer_output;
} OneNET_WeatherReport_t;

int OneNET_FormatWeather(char *buffer, size_t capacity,
                         uint32_t message_id, const OneNET_WeatherReport_t *data);
int OneNET_FormatPm25Event(char *buffer, size_t capacity, uint32_t message_id,
                           float pm25_value, float limit_value);

/* AT 命令返回后，平台回复可以异步到达；网络任务在等待期间仍能处理下发。 */
#define ONENET_UPLOAD_PROPERTY 0U
#define ONENET_UPLOAD_EVENT    1U
typedef struct
{
    UploadResult_t result;
    uint32_t started_ms;
    uint32_t timeout_ms;
    uint8_t kind;
    uint8_t active;
    uint8_t ready;
} OneNET_PendingUpload_t;

uint8_t OneNET_PendingStart(OneNET_PendingUpload_t *pending, uint8_t kind,
                            uint32_t message_id, uint32_t now_ms, uint32_t timeout_ms);
void OneNET_PendingOnLine(OneNET_PendingUpload_t *pending, const char *line);
void OneNET_PendingFail(OneNET_PendingUpload_t *pending, uint8_t state);
uint8_t OneNET_PendingTake(OneNET_PendingUpload_t *pending, uint32_t now_ms,
                           UploadResult_t *result);

/* 从当前 OneNET 上报结果文本中提取 id 和 code。 */
uint8_t OneNET_ParseUploadReply(
    const char *line,
    uint32_t *id,
    uint32_t *code);

/*
 * 发送上传指令，并等待匹配编号的平台回复。
 * 返回值：模块 AT 操作结果。
 * result：平台上传结果，两者分开记录。
 */
HAL_StatusTypeDef OneNET_Upload(
    const char *command,
    uint32_t message_id,
    UploadResult_t *result,
    uint32_t timeout_ms);


/* 上报一次 PM2.5 超限事件；result 保存平台回复。仅网络任务调用。 */
HAL_StatusTypeDef OneNET_UploadPm25Event(
    float pm25_value, float limit_value, uint32_t message_id,
    UploadResult_t *result, uint32_t timeout_ms);

#endif
