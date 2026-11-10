#include <stdint.h>
#include <string.h>
#include "stm32f1xx_hal.h"
#include "task.h"

static DMA_HandleTypeDef dma = {512};
UART_HandleTypeDef huart1 = { USART1, &dma, 0 };
static uint32_t tick, writes, mode, fired, callbacks;
static uint8_t *rxbuf;
static uint16_t rxsize, write_pos;
static TaskHandle_t current_task = (TaskHandle_t)1;
static int nested_status = -1;
void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size);
void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart);
HAL_StatusTypeDef Air780E_SendAndWait(const char *, const char *, uint32_t);

TaskHandle_t xTaskGetCurrentTaskHandle(void) { return current_task; }
void vTaskNotifyGiveFromISR(TaskHandle_t task, BaseType_t *woken) { (void)task; *woken = 0; }
uint32_t HAL_GetTick(void) { return tick; }
HAL_StatusTypeDef HAL_UART_AbortReceive(UART_HandleTypeDef *h) { (void)h; return HAL_OK; }
HAL_StatusTypeDef HAL_UARTEx_ReceiveToIdle_DMA(UART_HandleTypeDef *h, uint8_t *buf, uint16_t n)
{ (void)h; rxbuf=buf; rxsize=n; write_pos=0; dma.counter=n; return HAL_OK; }
HAL_StatusTypeDef HAL_UART_Transmit(UART_HandleTypeDef *h, uint8_t *data, uint16_t n, uint32_t t)
{ (void)h; (void)data; (void)n; (void)t; writes++; fired=0; return HAL_OK; }

static void inject(const char *data)
{
    while (*data)
    {
        rxbuf[write_pos] = (uint8_t)*data++;
        write_pos=(uint16_t)((write_pos+1U)%rxsize);
        dma.counter = rxsize - write_pos;
    }
    HAL_UARTEx_RxEventCallback(&huart1, write_pos);
}
static void handler(const char *line)
{
    (void)line;
    callbacks++;
    if (mode==4)
        nested_status = Air780E_SendAndWait("AT\r\n", "OK", 10);
}
uint32_t ulTaskNotifyTake(BaseType_t clear, TickType_t ticks)
{
    (void)clear;
    if (!fired)
    {
        fired=1;
        if (mode==0) { inject("\r\nOK\r\n"); tick++; return 1; }
        if (mode==2) { huart1.ErrorCode=8; HAL_UART_ErrorCallback(&huart1); tick++; return 1; }
        if (mode==3 || mode==4)
        { inject("+MSUB: \"test\",1,{}\r\nOK\r\n"); tick++; return 1; }
        if (mode==5)
        {
            static char long_line[280];
            memset(long_line,'A',270); long_line[270]='\n'; long_line[271]='O';
            long_line[272]='K'; long_line[273]='\n'; long_line[274]=0;
            inject(long_line); tick++; return 1;
        }
    }
    tick += ticks ? ticks : 1U;
    return 0;
}
#include "../../../Core/Src/air780e.c"
void HostReset(uint32_t new_mode)
{
    tick=0; writes=0; mode=new_mode; fired=0; callbacks=0; nested_status=-1;
    current_task=(TaskHandle_t)1;
    (void)Air780E_Init();
    Air780E_SetMqttHandler(handler);
}
void HostSetTask(uint32_t value) { current_task=(TaskHandle_t)(uintptr_t)value; }
void HostInject(const char *s) { inject(s); }
uint32_t HostWrites(void) { return writes; }
uint32_t HostCallbacks(void) { return callbacks; }
uint32_t HostTick(void) { return tick; }
int HostNestedStatus(void) { return nested_status; }
uint32_t HostLineOverflow(void) { return g_air_line_overflow; }
uint32_t HostErrorCount(void) { return g_air_uart_error_count; }
uint32_t HostRestartCount(void) { return g_air_rx_restart_count; }
uint32_t HostCollisionCount(void) { return g_air_command_collision_count; }
