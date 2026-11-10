#ifndef ONENET_CONTROL_H
#define ONENET_CONTROL_H

#include <stdint.h>

typedef struct
{
    char id[14]; /* OneNET 消息 ID 最多 13 位数字，按字符串原样保存。 */
    float pm25_limit;
    uint8_t has_pm25_limit;
    uint8_t has_buzzer_enable;
    uint8_t buzzer_enable;
} OneNET_SetCommand_t;

/* 只解析已确认属于本设备 property/set 主题的完整 +MSUB 文本。 */
uint8_t OneNET_ParseSet(const char *line, OneNET_SetCommand_t *result);

#endif
