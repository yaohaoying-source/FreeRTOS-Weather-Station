#ifndef ALARM_H
#define ALARM_H

#include <stdint.h>

/* 报警配置由云端更新；重启后恢复默认值。 */
typedef struct
{
    float pm25_limit;        /* PM2.5 阈值 */
    uint8_t buzzer_enable;   /* 1：允许声音；0：静音 */
} AlarmConfig_t;

/* 本次判断后的状态 */
typedef struct
{
    uint8_t sensor_valid;    /* 本轮粉尘读取是否成功 */
    uint8_t pm25_alarm;       /* 1：超限；0：未超限 */
    uint8_t buzzer_output;    /* 1：蜂鸣器打开；0：关闭 */
} AlarmStatus_t;

/* 根据采样值和配置更新报警状态 */
void Alarm_UpdatePm25(
    AlarmStatus_t *status,
    const AlarmConfig_t *config,
    float pm25,
    uint8_t pm25_valid);

#endif
