#include "gp2y1010.h"
#include "adc.h"
#include "tim.h"

#include "FreeRTOS.h"
#include "task.h"

#define DUST_LED_ON_LEVEL   GPIO_PIN_RESET
#define DUST_LED_OFF_LEVEL  GPIO_PIN_SET

#define DUST_LED_ON() HAL_GPIO_WritePin(GPIOA, GPIO_PIN_2, DUST_LED_ON_LEVEL)
#define DUST_LED_OFF() HAL_GPIO_WritePin(GPIOA, GPIO_PIN_2, DUST_LED_OFF_LEVEL)

#define DUST_ELAPSED_US(start) ((uint16_t)((uint16_t)__HAL_TIM_GET_COUNTER(&htim2) - (start)))

static HAL_StatusTypeDef GP2Y1010_Prepare(void)
{
    ADC_ChannelConfTypeDef config = {0};
    HAL_StatusTypeDef status;

    DUST_LED_OFF();

    /* PA1对应ADC1通道1 */
    config.Channel = ADC_CHANNEL_1;
    config.Rank = ADC_REGULAR_RANK_1;
    config.SamplingTime = ADC_SAMPLETIME_13CYCLES_5;

    status = HAL_ADC_ConfigChannel(&hadc1, &config);

    if (status != HAL_OK)
    {
        return status;
    }

    /* 提前开启ADC，并完成一次不用的转换 */
    status = HAL_ADC_Start(&hadc1);

    if (status != HAL_OK)
    {
        return status;
    }

    status = HAL_ADC_PollForConversion(&hadc1, 10);

    if (status != HAL_OK)
    {
        HAL_ADC_Stop(&hadc1);
        return status;
    }

    (void)HAL_ADC_GetValue(&hadc1);

    /* 保持ADC开启，供后面的脉冲测量使用 */
    return HAL_OK;
}
static HAL_StatusTypeDef GP2Y1010_ReadPulse(uint16_t *value)
{
    HAL_StatusTypeDef status = HAL_TIMEOUT;
    uint16_t start;
    uint16_t sample = 0;

    if (value == NULL)
    {
        return HAL_ERROR;
    }

    /* 1. 开启LED，记录本次测量的起点 */
    DUST_LED_ON();
    start = (uint16_t)__HAL_TIM_GET_COUNTER(&htim2);

    /* 2. 等到开启后的约280us */
    while (DUST_ELAPSED_US(start) < 280U)
    {
    }

    /* 3. 清除旧标志，启动本次转换 */
    __HAL_ADC_CLEAR_FLAG(&hadc1, ADC_FLAG_EOC);

    SET_BIT(hadc1.Instance->CR2,
            ADC_CR2_SWSTART | ADC_CR2_EXTTRIG);

    /* 4. 等待转换完成，但不超过软件截止时间 */
    while (DUST_ELAPSED_US(start) < 310U)
    {
        if (__HAL_ADC_GET_FLAG(&hadc1,
                               ADC_FLAG_EOC) != RESET)
        {
            sample = (uint16_t)HAL_ADC_GetValue(&hadc1);
            status = HAL_OK;
            break;
        }
    }

    /* 5. 等到开启后的约320us */
    while (DUST_ELAPSED_US(start) < 320U)
    {
    }

    /* 6. 无论转换是否成功，都关闭LED */
    DUST_LED_OFF();

    if (status == HAL_OK)
    {
        *value = sample;
    }

    return status;
} 
HAL_StatusTypeDef GP2Y1010_Read(GP2Y1010_Data_t *data)
{
    const uint16_t warmup = 100;
    const uint16_t samples = 16;

    HAL_StatusTypeDef status;
    HAL_StatusTypeDef stop_status;
    TickType_t last_wake;

    uint32_t sum = 0;
    uint16_t raw;
    float voltage;

    if (data == NULL)
    {
        return HAL_ERROR;
    }

    /* 1. 准备ADC，提前完成开启操作 */
    status = GP2Y1010_Prepare();

    if (status != HAL_OK)
    {
        DUST_LED_OFF();
        HAL_ADC_Stop(&hadc1);
        return status;
    }

    last_wake = xTaskGetTickCount();

    /* 2. 按约10ms周期执行测量 */
    for (uint16_t i = 0; i < warmup + samples; i++)
    {
        /* 只保护单次约320us的脉冲过程 */
        taskENTER_CRITICAL();

        status = GP2Y1010_ReadPulse(&raw);

        taskEXIT_CRITICAL();

        if (status != HAL_OK)
        {
            break;
        }

        /* 前100次丢弃，后16次参与平均 */
        if (i >= warmup)
        {
            sum += raw;
        }

        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(10));
    }

    /* 3. 无论成功或失败，都结束硬件操作 */
    DUST_LED_OFF();
    stop_status = HAL_ADC_Stop(&hadc1);

    if (status != HAL_OK)
    {
        return status;
    }

    if (stop_status != HAL_OK)
    {
        return stop_status;
    }

    /* 4. 整批成功，才更新输出结果 */
    data->raw = (uint16_t)(sum / samples);

    /* 暂按VDDA=3.3V计算，原板没有分压，不乘2 */
    data->voltage_mv = (uint16_t)(
        (uint32_t)data->raw * 3300UL / 4095UL);

    /*
     * 沿用原工程的换算公式及0~500限幅，输出保留小数。
     * 乘2属于旧算法参数，不表示当前硬件有分压电路。
     * 该结果为未标定的粉尘估算值。
     */
    voltage = 3.3f * data->raw / 4096.0f * 2.0f;
    data->dust_ug = (float)(
        (0.17 * voltage - 0.1) * 1000.0 + 100.0);

    if (data->dust_ug < 0.0f)
    {
        data->dust_ug = 0.0f;
    }
    else if (data->dust_ug > 500.0f)
    {
        data->dust_ug = 500.0f;
    }

    return HAL_OK;
}
void GP2Y1010_Init(void)
{
    DUST_LED_OFF();
}
