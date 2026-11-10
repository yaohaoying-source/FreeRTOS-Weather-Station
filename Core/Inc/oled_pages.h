#ifndef OLED_PAGES_H
#define OLED_PAGES_H

#include <stdint.h>

/* 显示任务提供的传感器快照。 */
typedef struct
{
    uint8_t have_data;
    uint8_t temperature;
    uint8_t humidity;
    uint8_t dht_valid;
    uint8_t bmp_valid;
    uint8_t light_percent;
    uint8_t light_valid;
    uint8_t dust_valid;
    uint8_t pm25_alarm;
    uint8_t buzzer_output;
    uint8_t buzzer_enable;
    double pressure_hpa;
    float pm25;
    float pm25_limit;
} OLED_WeatherView_t;

/* 显示任务提供的网络状态快照。 */
typedef struct
{
    uint8_t uart_status;
    uint8_t mqtt_status;
    uint8_t mqtt_state;
    uint8_t upload_state;
    uint8_t set_reply_status;
    uint8_t event_state;
    uint32_t upload_id;
    uint32_t upload_code;
    uint32_t set_received;
    uint32_t set_applied;
    uint32_t event_code;
} OLED_NetworkView_t;

/* 启动页在调度器启动前调用；其余页面由持有 I2C 锁的显示任务调用。 */
void OLEDPage_ShowBoot(void);
void OLEDPage_ShowData(const OLED_WeatherView_t *weather);
void OLEDPage_ShowNetwork(const OLED_NetworkView_t *network);
void OLEDPage_ShowAbout(void);

#endif
