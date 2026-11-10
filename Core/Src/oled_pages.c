#include "oled_pages.h"
#include "oled.h"
#include "onenet_connect.h"
#include "onenet_upload.h"
#include <stdio.h>
#include <string.h>

#define OLED_PAGE_ROWS   8U
#define OLED_LINE_WIDTH 21U
#define WEATHER_VERSION "V1.0"

/*
 * 屏幕字体为 6×8，一行最多 21 个字符。
 * 只写发生变化的行，减少软件 I2C 占用。
 */
static char s_last_line[OLED_PAGE_ROWS][OLED_LINE_WIDTH + 1U];
static uint8_t s_line_valid[OLED_PAGE_ROWS];

static void OLEDPage_WriteLine(uint8_t row, const char *text)
{
    char line[OLED_LINE_WIDTH + 1U];
    size_t length;

    if (row >= OLED_PAGE_ROWS || text == NULL)
        return;

    length = strlen(text);
    if (length > OLED_LINE_WIDTH)
        length = OLED_LINE_WIDTH;

    memset(line, ' ', OLED_LINE_WIDTH);
    memcpy(line, text, length);
    line[OLED_LINE_WIDTH] = '\0';

    if (s_line_valid[row] == 0U ||
        memcmp(s_last_line[row], line, sizeof(line)) != 0)
    {
        OLED_ShowStr(0, row, (unsigned char *)line, 1);
        memcpy(s_last_line[row], line, sizeof(line));
        s_line_valid[row] = 1U;
    }
}

static const char *OLEDPage_ResultText(uint8_t state)
{
    switch (state)
    {
        case UPLOAD_SUCCESS: return "SUCCESS";
        case UPLOAD_FAILED:  return "FAILED";
        case UPLOAD_WAITING: return "WAIT";
        case UPLOAD_TIMEOUT: return "TIMEOUT";
        default:             return "IDLE";
    }
}

static const char *OLEDPage_MqttText(const OLED_NetworkView_t *network)
{
    if (network->mqtt_status == HAL_BUSY)
        return "CONNECTING";
    if (network->mqtt_status != HAL_OK)
        return "FAILED";

    switch (network->mqtt_state)
    {
        case 0U: return "OFFLINE";
        case 1U: return "ONLINE";
        case 2U: return "NO AUTH";
        default: return "CHECK";
    }
}

void OLEDPage_ShowBoot(void)
{
    /* 调用前由 main 完成 OLED_Init 和 OLED_CLS。 */
    OLED_ShowStr(0, 2, (unsigned char *)"   WEATHER STATION", 1);
    OLED_ShowStr(0, 4, (unsigned char *)"    INITIALIZING", 1);
    OLED_ShowStr(0, 6, (unsigned char *)"       " WEATHER_VERSION, 1);

    /* 启动页未写入行缓存，下一页需要完整覆盖。 */
    memset(s_line_valid, 0, sizeof(s_line_valid));
}

void OLEDPage_ShowData(const OLED_WeatherView_t *weather)
{
    char text[40];

    if (weather == NULL)
        return;

    OLEDPage_WriteLine(0, "    WEATHER DATA");

    if (weather->dht_valid)
    {
        snprintf(text, sizeof(text), "TEMP:%uC  HUM:%u%%",
                 (unsigned int)weather->temperature,
                 (unsigned int)weather->humidity);
        OLEDPage_WriteLine(1, text);
    }
    else
        OLEDPage_WriteLine(1, "TEMP:--C  HUM:--%");

    if (weather->bmp_valid)
    {
        snprintf(text, sizeof(text), "PRESS:%.2f hPa", weather->pressure_hpa);
        OLEDPage_WriteLine(2, text);
    }
    else
        OLEDPage_WriteLine(2, "PRESS:--- hPa");

    if (weather->light_valid)
    {
        snprintf(text, sizeof(text), "LIGHT:%u%%",
                 (unsigned int)weather->light_percent);
        OLEDPage_WriteLine(3, text);
    }
    else
        OLEDPage_WriteLine(3, "LIGHT:--%");

    if (weather->dust_valid)
    {
        snprintf(text, sizeof(text), "PM2.5:%.2f ug/m3", (double)weather->pm25);
        OLEDPage_WriteLine(4, text);
    }
    else
        OLEDPage_WriteLine(4, "PM2.5:--- ug/m3");

    if (weather->have_data)
    {
        snprintf(text, sizeof(text), "LIMIT:%.2f ug/m3", (double)weather->pm25_limit);
        OLEDPage_WriteLine(5, text);

        snprintf(text, sizeof(text), "ALARM:%s BUZZ:%s",
                 weather->dust_valid ? (weather->pm25_alarm ? "ON" : "OFF") : "--",
                 weather->buzzer_output ? "ON" : "OFF");
        OLEDPage_WriteLine(6, text);

        snprintf(text, sizeof(text), "SOUND ENABLE:%s",
                 weather->buzzer_enable ? "ON" : "OFF");
        OLEDPage_WriteLine(7, text);
    }
    else
    {
        OLEDPage_WriteLine(5, "LIMIT:--- ug/m3");
        OLEDPage_WriteLine(6, "ALARM:-- BUZZ:--");
        OLEDPage_WriteLine(7, "SOUND ENABLE:--");
    }
}

void OLEDPage_ShowNetwork(const OLED_NetworkView_t *network)
{
    char text[40];
    const char *reply;

    if (network == NULL)
        return;

    OLEDPage_WriteLine(0, "    NETWORK STATUS");

    snprintf(text, sizeof(text), "UART DMA:%s",
             network->uart_status == HAL_OK ? "OK" : "WAIT/ERR");
    OLEDPage_WriteLine(1, text);

    snprintf(text, sizeof(text), "MQTT:%s", OLEDPage_MqttText(network));
    OLEDPage_WriteLine(2, text);

    snprintf(text, sizeof(text), "UP:%lu %s",
             (unsigned long)network->upload_id,
             OLEDPage_ResultText(network->upload_state));
    OLEDPage_WriteLine(3, text);

    snprintf(text, sizeof(text), "UP CODE:%lu",
             (unsigned long)network->upload_code);
    OLEDPage_WriteLine(4, text);

    snprintf(text, sizeof(text), "SET R:%lu A:%lu",
             (unsigned long)network->set_received,
             (unsigned long)network->set_applied);
    OLEDPage_WriteLine(5, text);

    if (network->set_applied == 0U)
        reply = "NONE";
    else if (network->set_reply_status == HAL_OK)
        reply = "AT OK";
    else if (network->set_reply_status == HAL_BUSY)
        reply = "WAIT";
    else
        reply = "AT ERR";

    snprintf(text, sizeof(text), "SET REPLY:%s", reply);
    OLEDPage_WriteLine(6, text);

    snprintf(text, sizeof(text), "EVENT:%s %lu",
             OLEDPage_ResultText(network->event_state),
             (unsigned long)network->event_code);
    OLEDPage_WriteLine(7, text);
}

void OLEDPage_ShowAbout(void)
{
    OLEDPage_WriteLine(0, "        ABOUT");
    OLEDPage_WriteLine(1, "VERSION: " WEATHER_VERSION);
    OLEDPage_WriteLine(2, "MCU: STM32F103C8T6");
    OLEDPage_WriteLine(3, "TEMP/HUM: DHT11");
    OLEDPage_WriteLine(4, "PRESS: BMP280");
    OLEDPage_WriteLine(5, "PM2.5: GP2Y1010");
    OLEDPage_WriteLine(6, "4G: AIR780E-B");
    OLEDPage_WriteLine(7, "LIGHT: ADC");
}
