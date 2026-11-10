#include <stdint.h>
#include <string.h>
#include "stm32f1xx_hal.h"
#include "FreeRTOS.h"
#include "onenet_upload.h"
#include "onenet_config.h"
#define AIR780E_LINE_SIZE 256U
#define pdPASS 1
typedef void *QueueHandle_t;
typedef struct { float pm25_value, limit_value; } Pm25Event_t;
static QueueHandle_t cloud_set_queue=(void *)1, pm25_event_queue=(void *)2;
static void *network_task_handle=(void *)1;
static OneNET_PendingUpload_t property_pending, event_pending;
static Pm25Event_t events[4];
static uint32_t event_count, head, cloud_count, tick, sends;
volatile HAL_StatusTypeDef g_air_mqtt_status=HAL_OK, g_air_sub_status=HAL_OK;
volatile HAL_StatusTypeDef g_pm25_event_at_status=HAL_BUSY;
volatile uint8_t g_air_recovery_requested=0, g_pm25_event_state=UPLOAD_IDLE;
volatile uint32_t g_cloud_set_received, g_cloud_set_overflow, g_cloud_set_discarded;
volatile uint32_t g_pm25_event_queued, g_pm25_event_dropped, g_pm25_event_success;
volatile uint32_t g_pm25_event_id, g_pm25_event_code;
char g_air_reply[256];
uint32_t HAL_GetTick(void) { return tick; }
uint32_t xTaskGetTickCount(void) { return tick; }
uint32_t ulTaskNotifyTake(int c, uint32_t n) { (void)c; tick+=n; return 0; }
void xTaskNotifyGive(void *h) { (void)h; }
HAL_StatusTypeDef Air780E_BeginCommand(void) { return HAL_OK; }
void Air780E_EndCommand(void) {}
HAL_StatusTypeDef Air780E_Send(const char *c) { (void)c; return HAL_OK; }
uint8_t Air780E_ReadLine(void) { return 0; }
uint8_t Air780E_HasRxError(void) { return 0; }
HAL_StatusTypeDef Air780E_SendAndWait(const char *c,const char *e,uint32_t t)
{ (void)c;(void)e;(void)t;sends++;return HAL_OK; }
void Network_RecordAtResult(HAL_StatusTypeDef s) { (void)s; }
int xQueueSend(QueueHandle_t q, const void *item, int timeout)
{
    (void)timeout;
    if(q==pm25_event_queue) {
        if(event_count==4) return 0;
        events[(head+event_count)%4]=*(const Pm25Event_t *)item;
        event_count++;return pdPASS;
    }
    if(q==cloud_set_queue && cloud_count<2) { cloud_count++;return pdPASS; }
    return 0;
}
int xQueuePeek(QueueHandle_t q, void *item, int timeout)
{ (void)timeout; if(q!=pm25_event_queue||!event_count)return 0;*(Pm25Event_t *)item=events[head];return pdPASS; }
int xQueueReceive(QueueHandle_t q, void *item, int timeout)
{ if(!xQueuePeek(q,item,timeout))return 0;head=(head+1)%4;event_count--;return pdPASS; }
#include "../../../Core/Src/onenet_upload.c"
#include "network_callback.inc"
#include "event_function.inc"
void HostPush(float pm25,float limit)
{
    Pm25Event_t event;
    event.pm25_value=pm25;event.limit_value=limit;
#include "event_enqueue.inc"
}
void HostStep(void) { Network_ProcessPm25Event(); }
void HostAdvance(uint32_t n) { tick+=n; }
void HostLine(const char *line) { Network_OnMqttLine(line); }
uint32_t HostCount(void) { return event_count; }
uint32_t HostDropped(void) { return g_pm25_event_dropped; }
uint32_t HostQueued(void) { return g_pm25_event_queued; }
uint32_t HostSuccess(void) { return g_pm25_event_success; }
uint32_t HostSends(void) { return sends; }
uint32_t HostState(void) { return g_pm25_event_state; }
uint32_t HostSets(void) { return g_cloud_set_received; }
