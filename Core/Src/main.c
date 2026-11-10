/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "adc.h"
#include "dma.h"
#include "tim.h"
#include "usart.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "bsp_delay.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "dht11.h"
#include "iic.h"
#include "oled.h"
#include "stdio.h"
#include "bmp280.h"
#include "semphr.h"
#include "gp2y1010.h"
#include "air780e.h"
#include "onenet_upload.h"
#include "onenet_connect.h"
#include "bsp_buzzer.h"
#include "alarm.h"
#include "onenet_control.h"
#include "onenet_config.h"
#include <string.h>
#include "oled_pages.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
static void Sensor(void *argument);      // 采集任务
static void Display(void *argument);     // 显示任务
static void NetworkTask(void *argument); /* 网络业务：连接、接收、周期上传 */

/*
 * Keil Watch 观察各步骤的最近返回结果。初始 HAL_BUSY 表示尚未完成该步骤。
 */
volatile HAL_StatusTypeDef g_air_init_status = HAL_BUSY;  /* 启动 DMA 接收 */
volatile HAL_StatusTypeDef g_air_tx_status = HAL_BUSY;    /* MCONFIG 参数配置及应答 */
volatile HAL_StatusTypeDef g_air_tcp_status = HAL_BUSY;   /* TCP 连接 */
volatile HAL_StatusTypeDef g_air_close_status = HAL_BUSY; /* 关闭旧 TCP 连接 */
volatile HAL_StatusTypeDef g_air_mqtt_status = HAL_BUSY;  /* MQTT 登录 */
volatile HAL_StatusTypeDef g_air_pub_status = HAL_BUSY;   /* 最近一次发布命令 */
volatile HAL_StatusTypeDef g_air_sub_status = HAL_BUSY;   /* 消息输出模式设置或订阅 */
/* 关闭回显的结果；g_air_init_status 只记录 DMA 启动结果 */
volatile HAL_StatusTypeDef g_air_at_status = HAL_BUSY;
/* 已经尝试连接的次数，方便在 Keil Watch 观察 */
volatile uint32_t g_air_connect_attempts = 0U;
/* 最近一次 MQTT 状态查询的结果 */
volatile HAL_StatusTypeDef g_air_query_status = HAL_BUSY;
/* MQTT 状态：0 离线，1 已登录，2 未认证，255 未知 */
volatile uint8_t g_air_mqtt_state = ONENET_MQTT_UNKNOWN;
/* 运行过程中触发重连的次数 */
volatile uint32_t g_air_reconnect_count = 0U;
volatile uint32_t g_air_recovery_attempts = 0U;
volatile uint32_t g_air_at_failures = 0U;
volatile uint32_t g_air_upload_timeouts = 0U;
volatile uint8_t g_air_recovery_requested = 0U;
volatile uint32_t g_pm25_event_queued = 0U;
volatile uint32_t g_pm25_event_dropped = 0U;
volatile uint32_t g_pm25_event_success = 0U;
volatile uint32_t g_pm25_event_id = 0U;
volatile uint32_t g_pm25_event_code = 0U;
volatile uint8_t g_pm25_event_state = UPLOAD_IDLE;
volatile HAL_StatusTypeDef g_pm25_event_at_status = HAL_BUSY;
typedef struct // 采集数据结构体
{
    uint8_t temperture;  // 温度
    uint8_t humidity;    // 湿度
    uint8_t dht11_valid; // dht11是否成功读取

    double bmp_temperature; // bmp280
    double press;
    uint8_t bmp_valid;

    uint16_t light_raw; // 光照强度
    uint8_t light_percent;
    uint8_t light_valid;

    GP2Y1010_Data_t dust; /* 粉尘采集结果 */
    uint8_t dust_valid;   /* 本轮采集是否成功 */

    float pm25_limit;
    uint8_t buzzer_enable;
    AlarmStatus_t alarm;
} WeatherData_t;
typedef struct
{
    float pm25_value;
    float limit_value;
} Pm25Event_t;
/* 任务间通信对象：数据快照、事件队列和 I2C 互斥锁。 */
static QueueHandle_t weatherqueue;
static QueueHandle_t uploadqueue; /* 长度 1：传递给网络任务的最新采集快照 */
static QueueHandle_t upload_result_queue;
static QueueHandle_t alarm_config_queue;
static QueueHandle_t pm25_event_queue;
static TaskHandle_t network_task_handle;
static TaskHandle_t sensor_task_handle;
static TaskHandle_t display_task_handle;
static SemaphoreHandle_t i2cMutex;

/* Keil Watch：最小剩余任务栈，单位为 StackType_t（本工程每个 4 字节）。 */
volatile uint32_t g_stack_start_min_words = 0U;
volatile uint32_t g_stack_sensor_min_words = 0U;
volatile uint32_t g_stack_display_min_words = 0U;
volatile uint32_t g_stack_network_min_words = 0U;
volatile uint32_t g_stack_idle_min_words = 0U;
volatile uint32_t g_heap_free_bytes = 0U;
volatile uint32_t g_heap_min_free_bytes = 0U;

/* 不创建额外任务；由显示任务低频采样，历史最小值由 FreeRTOS 维护。 */
static void Diagnostics_Update(void)
{
    if (sensor_task_handle != NULL)
        g_stack_sensor_min_words = uxTaskGetStackHighWaterMark(sensor_task_handle);
    if (display_task_handle != NULL)
        g_stack_display_min_words = uxTaskGetStackHighWaterMark(display_task_handle);
    if (network_task_handle != NULL)
        g_stack_network_min_words = uxTaskGetStackHighWaterMark(network_task_handle);
    g_stack_idle_min_words = uxTaskGetStackHighWaterMark(xTaskGetIdleTaskHandle());
    g_heap_free_bytes = (uint32_t)xPortGetFreeHeapSize();
    g_heap_min_free_bytes = (uint32_t)xPortGetMinimumEverFreeHeapSize();
}

/* 暂存两条云端属性设置消息 */
static QueueHandle_t cloud_set_queue;
static OneNET_PendingUpload_t property_pending;
static OneNET_PendingUpload_t event_pending;

/* Keil Watch 观察量 */
volatile uint32_t g_cloud_set_received = 0U;
volatile uint32_t g_cloud_set_overflow = 0U;
volatile uint32_t g_cloud_set_applied = 0U;
volatile uint32_t g_cloud_set_invalid = 0U;
volatile uint32_t g_cloud_set_discarded = 0U;
volatile HAL_StatusTypeDef g_cloud_reply_status = HAL_BUSY;
/* 将本设备的属性设置消息转交网络任务处理。 */
static void Network_OnMqttLine(const char *line)
{
    static const char topic[] = "" ONENET_TOPIC_PREFIX "/thing/property/set";

    const char *p;
    uint8_t quoted = 0U;

    if (line == NULL || cloud_set_queue == NULL || strncmp(line, "+MSUB:", 6U) != 0)
    {
        return;
    }

    /* 不论当前在等哪条 AT 回复，都先记录两个异步上报的云端回执。 */
    OneNET_PendingOnLine(&property_pending, line);
    OneNET_PendingOnLine(&event_pending, line);

    /* 找到模块输出的 MQTT 主题 */
    p = line + 6U;
    p += strspn(p, " \t");

    if (*p == '"')
    {
        quoted = 1U;
        p++;
    }

    /* 只接收本设备的属性设置主题 */
    if (strncmp(p, topic, sizeof(topic) - 1U) != 0)
    {
        return;
    }

    p += sizeof(topic) - 1U;

    if (quoted)
    {
        if (*p != '"')
        {
            return;
        }
        p++;
    }

    p += strspn(p, " \t");

    if (*p != ',')
    {
        return;
    }

    if (g_air_mqtt_status != HAL_OK || g_air_sub_status != HAL_OK)
    {
        /* 正在重连时不延后执行已经无法及时确认的旧设置。 */
        g_cloud_set_discarded++;
        return;
    }

    /*
     * line 指向驱动里的 256 字节接收数组。
     * 队列会复制内容，而不是保存这个指针。
     */
    if (xQueueSend(cloud_set_queue, line, 0) == pdPASS)
    {
        g_cloud_set_received++;
    }
    else
    {
        g_cloud_set_overflow++;
    }
}

/* 创建任务和所需的队列、互斥锁。 */
static void StartTask(void *argument)
{
    UploadResult_t initial_result = {0, 0, UPLOAD_IDLE};
    /* 默认阈值 10.0，蜂鸣器静音 */
    AlarmConfig_t initial_alarm_config = {10.0f, 0U};
    (void)argument;
    i2cMutex = xSemaphoreCreateMutex();
    weatherqueue = xQueueCreate(1, sizeof(WeatherData_t));
    uploadqueue = xQueueCreate(1, sizeof(WeatherData_t));
    upload_result_queue = xQueueCreate(8, sizeof(UploadResult_t));
    alarm_config_queue = xQueueCreate(1, sizeof(AlarmConfig_t));
    cloud_set_queue = xQueueCreate(2, AIR780E_LINE_SIZE);
    pm25_event_queue = xQueueCreate(4, sizeof(Pm25Event_t));
    if (i2cMutex == NULL)
        Error_Handler();
    if (weatherqueue == NULL)
        Error_Handler();
    if (uploadqueue == NULL)
        Error_Handler();
    if (upload_result_queue == NULL)
        Error_Handler();
    if (alarm_config_queue == NULL)
        Error_Handler();
    if (cloud_set_queue == NULL)
        Error_Handler();
    if (pm25_event_queue == NULL)
        Error_Handler();
    if (xQueueOverwrite(alarm_config_queue, &initial_alarm_config) != pdPASS)
        Error_Handler();
    xQueueSend(upload_result_queue, &initial_result, 0);
    if (xTaskCreate(Sensor, "Sensor", 384, NULL, 2, &sensor_task_handle) != pdPASS)
        Error_Handler();
    if (xTaskCreate(Display, "Display", 512, NULL, 1, &display_task_handle) != pdPASS)
        Error_Handler();
    if (xTaskCreate(NetworkTask, "NetworkTask", 512, NULL, 1, &network_task_handle) != pdPASS)
        Error_Handler();
    /* 启动任务随后删除，提前保存它的历史最小剩余栈。 */
    g_stack_start_min_words = uxTaskGetStackHighWaterMark(NULL);
    vTaskDelete(NULL);
}

/* 采集传感器、执行本地报警并发布数据快照。 */
static void Sensor(void *argument)
{
    BMP_OVERSAMPLE_MODE BMP_OVERSAMPLE_MODEStructure;
    BMP_OVERSAMPLE_MODEStructure.P_Osample = BMP280_P_MODE_3;
    BMP_OVERSAMPLE_MODEStructure.T_Osample = BMP280_T_MODE_1;
    BMP_OVERSAMPLE_MODEStructure.WORKMODE = BMP280_FORCED_MODE;
    AlarmConfig_t alarm_config;
    AlarmStatus_t alarm_status = {0U, 0U, 0U};
    uint8_t previous_pm25_alarm = 0U;
    (void)argument;

    for (;;)
    {
        WeatherData_t data = {0};
        uint8_t triggered = 0;
        double temperature;
        double pressure_pa;
        data.dht11_valid = (DHT11_Read_Data(&data.temperture, &data.humidity) == 0);
        /* 触发 BMP280 本轮测量 */
        if (xSemaphoreTake(i2cMutex, portMAX_DELAY) == pdTRUE)
        {
            if (BMP280_ReadID() == 0x58)
            {
                BMP280_Set_TemOversamp(&BMP_OVERSAMPLE_MODEStructure);
                triggered = 1;
            }
            xSemaphoreGive(i2cMutex);
        }

        if (triggered)
        {
            /*
             * 当前温度×1、压力×4的最大测量时间约13.3ms。
             * 等待期间释放总线，允许显示任务运行。
             */
            vTaskDelay(pdMS_TO_TICKS(20));

            if (xSemaphoreTake(i2cMutex, portMAX_DELAY) == pdTRUE)
            {
                /* 必须先进行温度补偿，再计算压力 */
                temperature = BMP280_Get_Temperature();
                pressure_pa = BMP280_Get_Pressure();

                xSemaphoreGive(i2cMutex);

                /* 对结果做基本范围检查 */
                if (temperature >= -40.0 && temperature <= 85.0 && pressure_pa >= 30000.0 &&
                    pressure_pa <= 110000.0)
                {
                    data.bmp_temperature = temperature;
                    data.press = pressure_pa / 100.0;
                    data.bmp_valid = 1;
                }
            }
        }
        /* 光照：PA0，对应ADC1通道0，连续采样16次求平均 */
        if (ADC_ReadAverage(ADC_CHANNEL_0, 16, &data.light_raw) == HAL_OK)
        {
            /* ADC值越小，表示越亮。*/
            data.light_percent = (uint8_t)((4095UL - data.light_raw) * 100UL / 4095UL);
            data.light_valid = 1;
        }
        else
        {
            data.light_valid = 0;
        }
        /* 粉尘：模块内部完成脉冲采样、平均和换算 */
        data.dust_valid = (GP2Y1010_Read(&data.dust) == HAL_OK);
        /* 读取最新的报警配置，不移除队列中的配置 */
        if (xQueuePeek(alarm_config_queue, &alarm_config, 0) == pdPASS)
        {
            Alarm_UpdatePm25(&alarm_status, &alarm_config, data.dust.dust_ug, data.dust_valid);
            /* 本轮有效，且从正常变为超限时，生成一次事件 */
            if (data.dust_valid)
            {
                if (previous_pm25_alarm == 0U && alarm_status.pm25_alarm != 0U)
                {
                    Pm25Event_t event;

                    event.pm25_value = data.dust.dust_ug;
                    event.limit_value = alarm_config.pm25_limit;

                    if (xQueueSend(pm25_event_queue, &event, 0) == pdPASS)
                    {
                        g_pm25_event_queued++;

                        /* 告诉网络任务有新事件 */
                        if (network_task_handle != NULL)
                        {
                            xTaskNotifyGive(network_task_handle);
                        }
                    }
                    else
                    {
                        /* 队列已满 */
                        g_pm25_event_dropped++;
                    }
                }

                previous_pm25_alarm = alarm_status.pm25_alarm;
            }
            /* 根据判断结果控制蜂鸣器 */
            if (alarm_status.buzzer_output != 0U)
            {
                BSP_Buzzer_On();
            }
            else
            {
                BSP_Buzzer_Off();
            }

            /* 与本轮传感器数据一起发给显示任务 */
            data.pm25_limit = alarm_config.pm25_limit;
            data.buzzer_enable = alarm_config.buzzer_enable;
            data.alarm = alarm_status;
        }
        /* 所有传感器采集结束，发送本轮结果 */
        xQueueOverwrite(weatherqueue, &data);
        xQueueOverwrite(uploadqueue, &data);
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
}
/* 渲染本地页面；OLED 和 BMP280 共用 I2C 互斥锁。 */
static void Display(void *argument)
{
    WeatherData_t data = {0};
    OLED_WeatherView_t weather = {0};
    OLED_NetworkView_t network = {0};
    UploadResult_t upload_result = {0, 0, UPLOAD_IDLE};

    uint8_t have_data = 0U;
    uint8_t page = 0U;
    uint8_t redraw = 1U;

    TickType_t now;
    TickType_t page_started = xTaskGetTickCount();
    TickType_t last_refresh = page_started;

    (void)argument;

    for (;;)
    {
        /* 取传感器快照；没有新数据最多等 100 ms。 */
        if (xQueueReceive(weatherqueue, &data, pdMS_TO_TICKS(100)) == pdPASS)
            have_data = 1U;

        /* 保留最后一条上传结果，供联网状态页使用。 */
        while (xQueueReceive(upload_result_queue, &upload_result, 0) == pdPASS)
        {
        }

        now = xTaskGetTickCount();

        /* 三个页面每 10 秒轮换一次。 */
        if ((TickType_t)(now - page_started) >= pdMS_TO_TICKS(10000U))
        {
            page = (uint8_t)((page + 1U) % 3U);
            page_started = now;
            redraw = 1U;
        }

        /* 每秒检查一次内容；页面模块只重写变化的行。 */
        if (redraw || (TickType_t)(now - last_refresh) >= pdMS_TO_TICKS(1000U))
        {
            weather.have_data = have_data;
            weather.temperature = data.temperture;
            weather.humidity = data.humidity;
            weather.dht_valid = data.dht11_valid;
            weather.bmp_valid = data.bmp_valid;
            weather.pressure_hpa = data.press;
            weather.light_percent = data.light_percent;
            weather.light_valid = data.light_valid;
            weather.dust_valid = data.dust_valid;
            weather.pm25 = data.dust.dust_ug;
            weather.pm25_limit = data.pm25_limit;
            weather.pm25_alarm = data.alarm.pm25_alarm;
            weather.buzzer_enable = data.buzzer_enable;
            weather.buzzer_output = data.alarm.buzzer_output;

            network.uart_status = (uint8_t)g_air_init_status;
            network.mqtt_status = (uint8_t)g_air_mqtt_status;
            network.mqtt_state = g_air_mqtt_state;
            network.upload_id = upload_result.id;
            network.upload_code = upload_result.code;
            network.upload_state = upload_result.state;
            network.set_received = g_cloud_set_received;
            network.set_applied = g_cloud_set_applied;
            network.set_reply_status = (uint8_t)g_cloud_reply_status;
            network.event_state = g_pm25_event_state;
            network.event_code = g_pm25_event_code;

            /* OLED 和 BMP280 共享 I2C。只在写屏时持锁。 */
            if (xSemaphoreTake(i2cMutex, portMAX_DELAY) == pdTRUE)
            {
                if (page == 0U)
                    OLEDPage_ShowData(&weather);
                else if (page == 1U)
                    OLEDPage_ShowNetwork(&network);
                else
                    OLEDPage_ShowAbout();

                xSemaphoreGive(i2cMutex);
            }

            last_refresh = xTaskGetTickCount();
            redraw = 0U;
            Diagnostics_Update();
        }
    }
}

static void UploadResultPost(const UploadResult_t *result)
{
    UploadResult_t discarded;

    /* 网络任务不阻塞等待显示；满了就丢掉最旧事件。 */
    if (xQueueSend(upload_result_queue, result, 0) != pdPASS)
    {
        xQueueReceive(upload_result_queue, &discarded, 0);
        xQueueSend(upload_result_queue, result, 0);
    }
}
/* 由网络任务调用，处理队列中的云端属性设置 */
static uint8_t s_at_consecutive_failures = 0U;

/* 超时后的命令状态不确定，先离开正常业务，再进行连接恢复。 */
static void Network_RecordAtResult(HAL_StatusTypeDef status)
{
    if (status == HAL_OK)
    {
        s_at_consecutive_failures = 0U;
        return;
    }
    g_air_at_failures++;
    if (s_at_consecutive_failures < 255U)
        s_at_consecutive_failures++;
    if (status == HAL_TIMEOUT || Air780E_HasRxError() ||
        s_at_consecutive_failures >= 3U)
        g_air_recovery_requested = 1U;
}

static void Network_ProcessSet(void)
{
    static char line[AIR780E_LINE_SIZE];
    static char reply_command[192];

    OneNET_SetCommand_t command;
    AlarmConfig_t config;
    int length;
    uint8_t processed = 0U;

    if (g_air_recovery_requested || Air780E_HasRxError())
        return;
    while (processed < 2U && xQueueReceive(cloud_set_queue, line, 0) == pdPASS)
    {
        processed++;
        /* 解析消息，并读取当前报警配置 */
        if (!OneNET_ParseSet(line, &command) ||
            xQueuePeek(alarm_config_queue, &config, 0) != pdPASS)
        {
            g_cloud_set_invalid++;
            continue;
        }

        /* 云端只设置了一项时，另一项保留原值 */
        if (command.has_pm25_limit)
        {
            config.pm25_limit = command.pm25_limit;
        }

        if (command.has_buzzer_enable)
        {
            config.buzzer_enable = command.buzzer_enable;
        }

        /* 更新后，采集任务会读取新配置 */
        if (xQueueOverwrite(alarm_config_queue, &config) != pdPASS)
        {
            g_cloud_set_invalid++;
            continue;
        }

        g_cloud_set_applied++;

        /* 使用下发消息的 ID 回复平台 */
        length = snprintf(reply_command, sizeof(reply_command),

                          "AT+MPUB=\"" ONENET_TOPIC_PREFIX "/thing/property/set_reply\",0,0,"
                          "\"{\\22id\\22:\\22%s\\22,\\22code\\22:200,"
                          "\\22msg\\22:\\22success\\22}\"\r\n",

                          command.id);

        if (length <= 0 || length >= (int)sizeof(reply_command))
        {
            g_cloud_reply_status = HAL_ERROR;
            continue;
        }

        g_cloud_reply_status = Air780E_SendAndWait(reply_command, "OK", 1000U);
        Network_RecordAtResult(g_cloud_reply_status);
        if (g_air_recovery_requested)
            break;
    }
}
/*
 * 发生超时后先接收迟到数据，空闲 250 ms 再查询带状态行的 MQTT 状态。
 * 这能减少旧 OK 被误用；无模块复位线时无法证明任意迟到回复都被排除。
 */
static HAL_StatusTypeDef Network_WaitRxQuiet(void)
{
    uint32_t started = HAL_GetTick();
    uint32_t last_data = started;
    uint32_t events = g_air_rx_events;
    while ((uint32_t)(HAL_GetTick() - started) < 2000U)
    {
        if (Air780E_HasRxError())
            return HAL_ERROR;
        while (Air780E_ReadLine())
        {
            if ((uint32_t)(HAL_GetTick() - started) >= 2000U)
                return HAL_TIMEOUT;
        }
        if (g_air_rx_events != events)
        {
            events = g_air_rx_events;
            last_data = HAL_GetTick();
        }
        if ((uint32_t)(HAL_GetTick() - last_data) >= 250U)
            return HAL_OK;
        (void)ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(50U));
    }
    return HAL_TIMEOUT;
}

/* 只尝试一次；失败后的等待和再次尝试由 NetworkTask 的主循环管理。 */
static HAL_StatusTypeDef Network_ConnectOnce(const char *config_command, uint8_t resync)
{
    HAL_StatusTypeDef status;
    OneNET_ConnectInfo_t info;
    uint8_t state;

    if (g_air_init_status != HAL_OK || Air780E_HasRxError())
    {
        g_air_init_status = Air780E_RestartRx();
        if (g_air_init_status != HAL_OK)
            return g_air_init_status;
        resync = 1U;
    }
    if (resync)
    {
        status = Network_WaitRxQuiet();
        if (status != HAL_OK)
            return status;
        /* 不能只用一行通用 OK，必须先收到明确的状态行。 */
        status = OneNET_QueryMqttState(&state, 3000U);
        if (status != HAL_OK)
            return status;
        if (state == 1U)
        {
            status = OneNET_Disconnect();
            if (status != HAL_OK)
                return status;
        }
    }
    g_air_connect_attempts++;
    status = OneNET_Connect(config_command, &info);
    g_air_at_status = info.at_status;
    g_air_tx_status = info.config_status;
    g_air_tcp_status = info.tcp_status;
    g_air_close_status = info.close_status;
    g_air_mqtt_status = (status == HAL_OK) ? info.mqtt_status : HAL_BUSY;
    g_air_sub_status = info.sub_status;
    return status;
}
/* 只由网络任务调用；失败后保留事件并等待重试 */
static void Network_ProcessPm25Event(void)
{
    static uint32_t next_id = 1U;
    static uint8_t retry_waiting = 0U;
    static TickType_t last_attempt_finished = 0;
    static char command[256];

    Pm25Event_t event;
    UploadResult_t result;
    int length;

    if (OneNET_PendingTake(&event_pending, HAL_GetTick(), &result))
    {
        g_pm25_event_state = result.state;
        g_pm25_event_code = result.code;
        if (result.state == UPLOAD_SUCCESS)
        {
            (void)xQueueReceive(pm25_event_queue, &event, 0);
            g_pm25_event_success++;
            next_id++;
            retry_waiting = 0U;
        }
        else
        {
            last_attempt_finished = xTaskGetTickCount();
            retry_waiting = 1U;
        }
    }

    if (event_pending.active || g_air_recovery_requested ||
        g_air_mqtt_status != HAL_OK || xQueuePeek(pm25_event_queue, &event, 0) != pdPASS)
    {
        return;
    }

    /* 上次失败后，至少等待 10 秒再重试 */
    if (retry_waiting &&
        (TickType_t)(xTaskGetTickCount() - last_attempt_finished) < pdMS_TO_TICKS(10000U))
    {
        return;
    }

    g_pm25_event_id = next_id;
    g_pm25_event_code = 0U;
    g_pm25_event_state = UPLOAD_WAITING;

    length = OneNET_FormatPm25Event(command, sizeof(command), next_id,
                                     event.pm25_value, event.limit_value);
    if (length <= 0 ||
        !OneNET_PendingStart(&event_pending, ONENET_UPLOAD_EVENT,
                             next_id, HAL_GetTick(), 3000U))
    {
        g_pm25_event_at_status = HAL_ERROR;
        g_pm25_event_state = UPLOAD_FAILED;
        last_attempt_finished = xTaskGetTickCount();
        retry_waiting = 1U;
        return;
    }
    /* 只等待模块的 OK；云端回执由 Network_OnMqttLine 异步记录。 */
    g_pm25_event_at_status = Air780E_SendAndWait(command, "OK", 2000U);
    Network_RecordAtResult(g_pm25_event_at_status);
    if (g_pm25_event_at_status != HAL_OK)
        OneNET_PendingFail(&event_pending,
                           g_pm25_event_at_status == HAL_TIMEOUT ? UPLOAD_TIMEOUT : UPLOAD_FAILED);
}
/*
 * 网络业务放在本任务中：连接准备 -> 订阅结果 -> 周期上传和持续接收。
 * 串口收发与取行交给 air780e.c；传感器数值从 uploadqueue 复制。
 */
/* 属性回执由串口回调记录，这里只取最终结果，不等待平台。 */
static void Network_PollProperty(void)
{
    static uint8_t consecutive_timeouts = 0U;
    UploadResult_t result;
    if (!OneNET_PendingTake(&property_pending, HAL_GetTick(), &result))
        return;
    UploadResultPost(&result);
    if (result.state == UPLOAD_TIMEOUT)
    {
        g_air_upload_timeouts++;
        if (consecutive_timeouts < 255U)
            consecutive_timeouts++;
        if (consecutive_timeouts >= 3U)
        {
            g_air_recovery_requested = 1U;
            consecutive_timeouts = 0U;
        }
    }
    else
    {
        consecutive_timeouts = 0U;
    }
}

/*
 * 唯一 AT 使用者：命令逐条发送；平台回复和下发通过同一个接收回调分流。
 * 每次连接和命令都有时限；失败后让任务休眠再重试。
 */
static void NetworkTask(void *argument)
{
    static char pub_command[512];
    static const char mqtt_config_command[] = ONENET_MQTT_CONFIG_COMMAND;
    WeatherData_t upload_data;
    OneNET_WeatherReport_t report;
    UploadResult_t upload_result;
    uint32_t message_id = 1U;
    TickType_t last_upload, last_mqtt_check, last_attempt;
    TickType_t now, remaining;
    uint8_t mqtt_state, connected = 0U, resync = 0U, retry_waiting = 0U;
    int command_length;
    HAL_StatusTypeDef status;
    (void)argument;

    Air780E_SetMqttHandler(Network_OnMqttLine);
    g_air_init_status = Air780E_Init();
    if (g_air_init_status != HAL_OK)
        resync = 1U;
    last_upload = xTaskGetTickCount();
    last_mqtt_check = last_upload;
    last_attempt = last_upload;

    for (;;)
    {
        uint8_t received_lines = 0U;
        while (received_lines < 32U && Air780E_ReadLine())
        {
            received_lines++;
        }

        if (connected && (g_air_recovery_requested || Air780E_HasRxError()))
        {
            if (Air780E_HasRxError())
                g_air_init_status = HAL_ERROR;
            connected = 0U;
            resync = 1U;
            retry_waiting = 1U;
            last_attempt = xTaskGetTickCount();
            g_air_reconnect_count++;
            g_air_mqtt_status = HAL_BUSY;
            g_air_sub_status = HAL_BUSY;
            g_air_mqtt_state = ONENET_MQTT_UNKNOWN;
            OneNET_PendingFail(&property_pending, UPLOAD_FAILED);
            OneNET_PendingFail(&event_pending, UPLOAD_FAILED);
            Network_PollProperty();
            Network_ProcessPm25Event(); /* 失败事件留在队列，重连后再试。 */
            /* 连接中断时无法及时确认的旧下发不能在重连后悄悄执行。 */
            g_cloud_set_discarded += (uint32_t)uxQueueMessagesWaiting(cloud_set_queue);
            (void)xQueueReset(cloud_set_queue);
            g_air_recovery_requested = 0U;
            continue;
        }

        if (!connected)
        {
            now = xTaskGetTickCount();
            if (retry_waiting &&
                (TickType_t)(now - last_attempt) < pdMS_TO_TICKS(5000U))
            {
                (void)ulTaskNotifyTake(pdTRUE, pdMS_TO_TICKS(200U));
                continue;
            }
            g_air_recovery_attempts++;
            status = Network_ConnectOnce(mqtt_config_command, resync);
            if (status == HAL_OK)
            {
                connected = 1U;
                resync = 0U;
                retry_waiting = 0U;
                s_at_consecutive_failures = 0U;
                g_air_recovery_requested = 0U;
                g_air_mqtt_state = ONENET_MQTT_UNKNOWN;
                last_upload = xTaskGetTickCount();
                last_mqtt_check = last_upload;
            }
            else
            {
                /* 不把未知状态当作已连接；下次先检查串口并同步状态。 */
                resync = 1U;
                retry_waiting = 1U;
                last_attempt = xTaskGetTickCount();
                g_air_mqtt_status = HAL_BUSY;
                g_air_mqtt_state = ONENET_MQTT_UNKNOWN;
            }
            continue;
        }

        Network_ProcessSet();        /* 云端下发优先于发起新的上报。 */
        Network_PollProperty();
        if (g_air_recovery_requested)
            continue;
        Network_ProcessPm25Event();
        Network_ProcessSet();
        if (g_air_recovery_requested)
            continue;

        now = xTaskGetTickCount();
        if ((TickType_t)(now - last_mqtt_check) >= pdMS_TO_TICKS(30000U))
        {
            last_mqtt_check = now;
            g_air_query_status = OneNET_QueryMqttState(&mqtt_state, 3000U);
            Network_RecordAtResult(g_air_query_status);
            if (g_air_query_status == HAL_OK)
            {
                g_air_mqtt_state = mqtt_state;
                if (mqtt_state != 1U)
                    g_air_recovery_requested = 1U;
            }
            else
                g_air_mqtt_state = ONENET_MQTT_UNKNOWN;
            Network_ProcessSet();
            if (g_air_recovery_requested)
                continue;
        }

        now = xTaskGetTickCount();
        if ((TickType_t)(now - last_upload) >= pdMS_TO_TICKS(10000U))
        {
            last_upload = now;
            if (!property_pending.active &&
                xQueuePeek(uploadqueue, &upload_data, 0) == pdPASS)
            {
                memset(&report, 0, sizeof(report));
                report.dht_valid = upload_data.dht11_valid;
                report.temperature = upload_data.temperture;
                report.humidity = upload_data.humidity;
                report.bmp_valid = upload_data.bmp_valid;
                report.pressure = upload_data.press;
                report.light_valid = upload_data.light_valid;
                report.light_percent = upload_data.light_percent;
                report.dust_valid = upload_data.dust_valid;
                report.pm25 = (double)upload_data.dust.dust_ug;
                report.pm25_limit = (double)upload_data.pm25_limit;
                report.buzzer_enable = upload_data.buzzer_enable;
                report.pm25_alarm = upload_data.alarm.pm25_alarm;
                report.buzzer_output = upload_data.alarm.buzzer_output;
                command_length = OneNET_FormatWeather(pub_command, sizeof(pub_command),
                                                       message_id, &report);
                if (command_length > 0 &&
                    OneNET_PendingStart(&property_pending, ONENET_UPLOAD_PROPERTY,
                                        message_id, HAL_GetTick(), 5000U))
                {
                    upload_result.id = message_id;
                    upload_result.code = 0U;
                    upload_result.state = UPLOAD_WAITING;
                    UploadResultPost(&upload_result);
                    g_air_pub_status = Air780E_SendAndWait(pub_command, "OK", 2000U);
                    Network_RecordAtResult(g_air_pub_status);
                    if (g_air_pub_status != HAL_OK)
                        OneNET_PendingFail(&property_pending,
                            g_air_pub_status == HAL_TIMEOUT ? UPLOAD_TIMEOUT : UPLOAD_FAILED);
                    message_id++;
                }
                else
                {
                    g_air_pub_status = HAL_ERROR;
                    upload_result.id = message_id++;
                    upload_result.code = 0U;
                    upload_result.state = UPLOAD_FAILED;
                    UploadResultPost(&upload_result);
                }
                if (!g_air_recovery_requested)
                    Network_ProcessSet();
            }
        }

        /* 最多 200 ms 检查一次异步结果和到期时间，不忙等。 */
        now = xTaskGetTickCount();
        remaining = pdMS_TO_TICKS(10000U) - (TickType_t)(now - last_upload);
        if ((TickType_t)(now - last_upload) >= pdMS_TO_TICKS(10000U) ||
            remaining > pdMS_TO_TICKS(200U))
            remaining = pdMS_TO_TICKS(200U);
        (void)ulTaskNotifyTake(pdTRUE, remaining);
    }
}

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_ADC1_Init();
  MX_TIM2_Init();
  MX_USART1_UART_Init();
  /* USER CODE BEGIN 2 */
  if (BSP_Delay_Init() != HAL_OK) Error_Handler();
  GP2Y1010_Init();
  DHT11_Init();

  /* OLED初始化同时准备共用的软件I2C，再访问BMP280。 */
  OLED_Init();
  OLED_CLS();
  OLEDPage_ShowBoot();

  /* 保留上电稳定等待，移除原来的测试页面和ID显示。 */
  HAL_Delay(5000);
  if (BMP280_ReadID() == 0x58)
  {
    Bmp_Init();
  }

  if (HAL_ADCEx_Calibration_Start(&hadc1) != HAL_OK) Error_Handler();
  if (xTaskCreate(StartTask, "StartTask", 128, NULL, 3, NULL) != pdPASS)
    Error_Handler();
  vTaskStartScheduler();
  Error_Handler();
  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
  RCC_PeriphCLKInitTypeDef PeriphClkInit = {0};

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.HSEPredivValue = RCC_HSE_PREDIV_DIV1;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLMUL = RCC_PLL_MUL9;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
  PeriphClkInit.PeriphClockSelection = RCC_PERIPHCLK_ADC;
  PeriphClkInit.AdcClockSelection = RCC_ADCPCLK2_DIV6;
  if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInit) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  Period elapsed callback in non blocking mode
  * @note   This function is called  when TIM4 interrupt took place, inside
  * HAL_TIM_IRQHandler(). It makes a direct call to HAL_IncTick() to increment
  * a global variable "uwTick" used as application time base.
  * @param  htim : TIM handle
  * @retval None
  */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  /* USER CODE BEGIN Callback 0 */

  /* USER CODE END Callback 0 */
  if (htim->Instance == TIM4)
  {
    HAL_IncTick();
  }
  /* USER CODE BEGIN Callback 1 */

  /* USER CODE END Callback 1 */
}

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* User can add his own implementation to report the HAL error return state */
  __disable_irq();
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}
#ifdef USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
