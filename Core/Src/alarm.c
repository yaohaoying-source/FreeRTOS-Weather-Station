#include "alarm.h"

void Alarm_UpdatePm25(
    AlarmStatus_t *status,
    const AlarmConfig_t *config,
    float pm25,
    uint8_t pm25_valid)
{
    if (status == 0 || config == 0)
    {
        return;
    }

    status->sensor_valid = (pm25_valid != 0U) ? 1U : 0U;

    /*
     * 读取成功才重新判断。
     * 读取失败时保留上次报警状态，不把无效数据当成正常值。
     */
    if (status->sensor_valid != 0U)
    {
        status->pm25_alarm =
            (pm25 > config->pm25_limit) ? 1U : 0U;
    }

    /* 超限并且允许声音时，才打开蜂鸣器 */
    status->buzzer_output =
        (status->pm25_alarm != 0U &&
         config->buzzer_enable != 0U) ? 1U : 0U;
}
