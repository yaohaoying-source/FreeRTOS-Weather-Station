/*
 * Air780E AT 通信驱动。
 * 发送：网络任务 -> SendAndWait -> Send -> USART1 TX -> 模块。
 * 接收：模块 -> USART1 RX -> DMA 数组；中断通知任务，任务用 ReadLine 取行。
 * TCP/MQTT 连接顺序、JSON 内容和上传周期放在 main.c 的 NetworkTask 中。
 */
#include "air780e.h"
#include "usart.h"
#include "FreeRTOS.h"
#include "task.h"
#include <string.h>
/* DMA 循环写入原始字节；网络任务通过 ReadLine 读取。不要把整个数组当字符串。 */
uint8_t g_air_rx_dma[AIR780E_RX_SIZE];
volatile uint16_t g_air_rx_pos = 0;       /* 最近一次回调报告的位置，仅供观察 */
volatile uint32_t g_air_rx_events = 0;    /* 接收回调次数，不是完整消息数量 */
volatile uint32_t g_air_uart_error = 0;   /* 最近一次 HAL 串口错误码 */
volatile uint32_t g_air_uart_error_count = 0;
volatile uint32_t g_air_rx_restart_count = 0;
volatile uint32_t g_air_command_collision_count = 0;

/* ReadLine 的拼行区：只有函数返回 1 时，才表示这里是一行完整文本。 */
char g_air_reply[AIR780E_LINE_SIZE];
/* 最近一条完整 +MSUB 消息，包含主题、长度和消息体；新消息覆盖旧消息。 */
char g_air_mqtt_message[AIR780E_LINE_SIZE];
volatile uint32_t g_air_mqtt_rx_count = 0;
volatile uint32_t g_air_line_overflow = 0; /* 过长文本行的丢弃次数，不统计 DMA 覆盖 */

/* Init 保存网络任务句柄；接收中断向这个任务发送通知。 */
static TaskHandle_t s_air_task = NULL;
static volatile uint8_t s_rx_error = 0U;
static uint8_t s_command_active = 0U;

/* 以下状态只由网络任务修改；跨命令、跨接收事件保留，仅在 Init 中重置。 */
static uint16_t s_read_pos = 0;    /* 软件下一次从 DMA 数组读取的位置 */
static uint16_t s_line_length = 0; /* 当前已经拼接的字符数 */
static uint8_t s_drop_line = 0;    /* 本行过长后，丢弃至下一个换行符 */
/* 保存业务层注册的消息处理函数 */
static Air780E_MqttHandler_t s_mqtt_handler = NULL;

void Air780E_SetMqttHandler(Air780E_MqttHandler_t handler)
{
    s_mqtt_handler = handler;
}
/*
 * 1. 初始化接收。在调度器启动后的网络任务中调用一次。
 * 清空软件状态、保存当前任务句柄，再启动 USART1 的循环 DMA + IDLE 接收。
 * HAL_OK 仅表示接收启动成功，TCP/MQTT 连接由 main.c 继续完成。
 */
HAL_StatusTypeDef Air780E_Init(void)
{
    s_air_task = xTaskGetCurrentTaskHandle();
    s_read_pos = 0;
    s_line_length = 0;
    s_drop_line = 0;

    memset(g_air_rx_dma, 0, sizeof(g_air_rx_dma));
    g_air_reply[0] = '\0';
    g_air_mqtt_message[0] = '\0';
    g_air_rx_pos = 0;
    g_air_rx_events = 0;
    g_air_uart_error = 0;
    g_air_mqtt_rx_count = 0;
    g_air_line_overflow = 0;
    s_rx_error = 0U;
    s_command_active = 0U;

    return HAL_UARTEx_ReceiveToIdle_DMA(
        &huart1, g_air_rx_dma, sizeof(g_air_rx_dma));
}

uint8_t Air780E_HasRxError(void)
{
    return s_rx_error;
}

/* 串口错误可能让 HAL 停止 DMA；只在网络任务中结束旧接收并重新启动。 */
HAL_StatusTypeDef Air780E_RestartRx(void)
{
    HAL_StatusTypeDef status;
    if (s_air_task == NULL || xTaskGetCurrentTaskHandle() != s_air_task ||
        s_command_active)
        return HAL_BUSY;
    status = HAL_UART_AbortReceive(&huart1);
    if (status != HAL_OK)
        return status;
    __HAL_UART_CLEAR_OREFLAG(&huart1);
    s_read_pos = 0U;
    s_line_length = 0U;
    s_drop_line = 0U;
    g_air_reply[0] = '\0';
    g_air_rx_pos = 0U;
    s_rx_error = 0U;
    status = HAL_UARTEx_ReceiveToIdle_DMA(&huart1, g_air_rx_dma, sizeof(g_air_rx_dma));
    if (status == HAL_OK)
        g_air_rx_restart_count++;
    return status;
}

HAL_StatusTypeDef Air780E_BeginCommand(void)
{
    if (s_air_task == NULL || xTaskGetCurrentTaskHandle() != s_air_task ||
        s_command_active)
    {
        g_air_command_collision_count++;
        return HAL_BUSY;
    }
    if (s_rx_error)
        return HAL_ERROR;
    s_command_active = 1U;
    return HAL_OK;
}

void Air780E_EndCommand(void)
{
    if (s_air_task != NULL && xTaskGetCurrentTaskHandle() == s_air_task)
        s_command_active = 0U;
}

/*
 * 2. 发送一条以 '\0' 结尾的 AT 字符串。这里只发送，不判断模块回复。
 * 当前使用普通串口轮询发送，100ms 是发送超时；接收仍由 DMA 独立完成。
 */
HAL_StatusTypeDef Air780E_Send(const char *command)
{
    if (command == NULL)
    {
        return HAL_ERROR;
    }
    if (s_air_task == NULL || xTaskGetCurrentTaskHandle() != s_air_task ||
        !s_command_active || s_rx_error)
        return HAL_BUSY;

    return HAL_UART_Transmit(
        &huart1, (uint8_t *)command,
        (uint16_t)strlen(command), 100);
}

/*
 * 3. 非阻塞地读取下一行：完整一行返回 1，否则返回 0，半行留到下次续接。
 * 用法：while (Air780E_ReadLine()) { 使用 g_air_reply 中的完整行; }
 * 忽略 CR，以 LF 结束一行，跳过空行；同时保存完整的 +MSUB 主动消息。
 * 仅由网络任务调用，处理短文本行，不是任意二进制 MQTT 消息解析器。
 */
uint8_t Air780E_ReadLine(void)
{
    uint16_t write_pos;
    char ch;

    if (huart1.hdmarx == NULL || s_air_task == NULL ||
        xTaskGetCurrentTaskHandle() != s_air_task)
    {
        return 0;
    }

    /* 剩余传输数递减，因此用数组大小减去它得到本圈写位置；取模处理回绕。 */
    write_pos = (uint16_t)(
        (AIR780E_RX_SIZE -
         __HAL_DMA_GET_COUNTER(huart1.hdmarx)) % AIR780E_RX_SIZE);

    /* 只处理当前已经到达的字节；读位置追上写位置就结束，不等待新字节。 */
    while (s_read_pos != write_pos)
    {
        ch = (char)((volatile uint8_t *)g_air_rx_dma)[s_read_pos];
        s_read_pos = (uint16_t)((s_read_pos + 1U) % AIR780E_RX_SIZE);

        if (ch == '\r')
        {
            continue;
        }

        /* 收到换行符，才判断是否形成一行完整且未溢出的文本。 */
        if (ch == '\n')
        {
            if (s_drop_line || s_line_length == 0U)
            {
                s_drop_line = 0;
                s_line_length = 0;
                continue;
            }

            g_air_reply[s_line_length] = '\0';

            /* 即使当前正在等 AT 回复，也保存夹在其中的 MQTT 主动消息。 */
            if (strncmp(g_air_reply, "+MSUB:", 6U) == 0)
            {
                memcpy(g_air_mqtt_message, g_air_reply,
                       (size_t)s_line_length + 1U);
                g_air_mqtt_rx_count++;
                if (s_mqtt_handler != NULL)
                {
                    s_mqtt_handler(g_air_reply);
                }
            }

            s_line_length = 0;
            return 1;
        }

        if (!s_drop_line)
        {
            if (s_line_length < AIR780E_LINE_SIZE - 1U)
            {
                g_air_reply[s_line_length++] = ch;
                g_air_reply[s_line_length] = '\0';
            }
            else
            {
                /* 留一个位置给字符串结尾；超过容量后整行丢弃，直到遇到 LF。 */
                s_drop_line = 1;
                g_air_line_overflow++;
            }
        }
    }

    return 0;
}

/*
 * 4. 发送并等待：command 是指令，expected 是完整的目标回复，timeout_ms 是等待时限。
 * 返回 HAL_OK：匹配目标行；HAL_ERROR：错误回复；HAL_TIMEOUT：等待超时。
 * 等 CONNECT OK 时收到 ALREADY CONNECT 返回 HAL_BUSY，由网络任务决定关闭和重试。
 * 等待过程中继续用同一个 ReadLine 处理接收；没有完整行时通过通知阻塞让出 CPU。
 */
static HAL_StatusTypeDef Air780E_SendAndWaitOwned(
    const char *command,
    const char *expected,
    uint32_t timeout_ms)
{
    uint32_t start;
    uint32_t elapsed;
    uint32_t drained = 0U;
    HAL_StatusTypeDef status;

    if (command == NULL || expected == NULL ||
        huart1.hdmarx == NULL)
    {
        return HAL_ERROR;
    }

    /*
     * 先消费已经到达的数据，MQTT消息会被ReadLine记录。
     * 不清空DMA、不重置读位置，也不清掉尚未完成的行。
     */
    while (Air780E_ReadLine())
    {
        /* 串口持续洪泛时不在发送前无限清理旧数据。 */
        if (++drained >= 32U || s_rx_error)
            return HAL_BUSY;
    }

    status = Air780E_Send(command);
    if (status != HAL_OK)
    {
        return status;
    }

    start = HAL_GetTick();

    while ((uint32_t)(HAL_GetTick() - start) < timeout_ms)
    {
        if (s_rx_error)
            return HAL_ERROR;
        while (Air780E_ReadLine())
        {
            if (s_rx_error)
                return HAL_ERROR;
            if ((uint32_t)(HAL_GetTick() - start) >= timeout_ms)
                return HAL_TIMEOUT;
            /* 按整行精确匹配，避免把其他文本中的 OK 当成本指令成功。 */
            if (strcmp(g_air_reply, expected) == 0)
            {
                return HAL_OK;
            }

            if (strcmp(expected, "CONNECT OK") == 0 &&
                strcmp(g_air_reply, "ALREADY CONNECT") == 0)
            {
                return HAL_BUSY;
            }

            if (strcmp(g_air_reply, "ERROR") == 0 ||
                strncmp(g_air_reply, "+CME ERROR", 10U) == 0 ||
                strcmp(g_air_reply, "CONNECT FAIL") == 0 ||
                strcmp(g_air_reply, "CLOSED") == 0)
            {
                return HAL_ERROR;
            }
        }

        elapsed = (uint32_t)(HAL_GetTick() - start);
        if (elapsed >= timeout_ms)
        {
            break;
        }

        /* 一直沿用本次指令的起点计时；其他消息不会重新延长等待时限。 */
        (void)ulTaskNotifyTake(
            pdTRUE, pdMS_TO_TICKS(timeout_ms - elapsed));
    }

    return HAL_TIMEOUT;
}

HAL_StatusTypeDef Air780E_SendAndWait(
    const char *command, const char *expected, uint32_t timeout_ms)
{
    HAL_StatusTypeDef status = Air780E_BeginCommand();
    if (status != HAL_OK)
        return status;
    status = Air780E_SendAndWaitOwned(command, expected, timeout_ms);
    Air780E_EndCommand();
    return status;
}

/*
 * 5. 接收事件回调：由 HAL 在中断上下文调用，不需要业务代码手动调用。
 * IDLE、DMA 半满和 DMA 满都可能触发。Size 是回调报告的位置，不是完整帧长度。
 * 中断只记录事件并通知任务；通知不携带文本，文本已经由 DMA 写入数组。
 */
void HAL_UARTEx_RxEventCallback(
    UART_HandleTypeDef *huart, uint16_t Size)
{
    BaseType_t higher_priority_task_woken = pdFALSE;

    if (huart->Instance == USART1)
    {
        g_air_rx_pos = Size;
        g_air_rx_events++;

        if (s_air_task != NULL)
        {
            vTaskNotifyGiveFromISR(
                s_air_task, &higher_priority_task_woken);
        }

        /* 唤醒了更高优先级的任务时，请求退出中断后调度。 */
        portYIELD_FROM_ISR(higher_priority_task_woken);
    }
}

/* 最近一次串口错误记录，供 Watch 观察；这里不执行接收恢复或网络重连。 */
void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    BaseType_t higher_priority_task_woken = pdFALSE;
    if (huart->Instance == USART1)
    {
        g_air_uart_error = huart->ErrorCode;
        g_air_uart_error_count++;
        s_rx_error = 1U;
        if (s_air_task != NULL)
            vTaskNotifyGiveFromISR(s_air_task, &higher_priority_task_woken);
        portYIELD_FROM_ISR(higher_priority_task_woken);
    }
}
