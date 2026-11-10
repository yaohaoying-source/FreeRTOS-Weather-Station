#include <stdint.h>
#include <string.h>
#include "stm32f1xx_hal.h"
#include "onenet_upload.h"
#include "onenet_config.h"
#define AIR780E_LINE_SIZE 256U
#define pdPASS 1
typedef void *QueueHandle_t;
static QueueHandle_t cloud_set_queue=(QueueHandle_t)1;
static OneNET_PendingUpload_t property_pending, event_pending;
volatile HAL_StatusTypeDef g_air_mqtt_status=HAL_OK, g_air_sub_status=HAL_OK;
volatile uint32_t g_cloud_set_received, g_cloud_set_overflow, g_cloud_set_discarded;
static uint32_t queue_count;
char g_air_reply[256];
uint32_t HAL_GetTick(void) { return 0; }
uint32_t ulTaskNotifyTake(int c, uint32_t n) { (void)c; (void)n; return 0; }
HAL_StatusTypeDef Air780E_BeginCommand(void) { return HAL_OK; }
void Air780E_EndCommand(void) {}
HAL_StatusTypeDef Air780E_Send(const char *c) { (void)c; return HAL_OK; }
uint8_t Air780E_ReadLine(void) { return 0; }
uint8_t Air780E_HasRxError(void) { return 0; }
int xQueueSend(QueueHandle_t q, const void *item, int ticks)
{ (void)q; (void)item; (void)ticks; if(queue_count>=2) return 0; queue_count++; return pdPASS; }
#include "../../../Core/Src/onenet_upload.c"
#include "network_callback.inc"
void HostReset(void)
{
    memset(&property_pending,0,sizeof(property_pending));
    memset(&event_pending,0,sizeof(event_pending));
    queue_count=0;g_cloud_set_received=0;g_cloud_set_overflow=0;g_cloud_set_discarded=0;
    g_air_mqtt_status=HAL_OK;g_air_sub_status=HAL_OK;
}
void HostLine(const char *line) { Network_OnMqttLine(line); }
uint8_t HostStart(uint32_t id)
{ return OneNET_PendingStart(&property_pending,ONENET_UPLOAD_PROPERTY,id,0,5000); }
uint8_t HostTake(uint32_t now, UploadResult_t *result)
{ return OneNET_PendingTake(&property_pending,now,result); }
uint32_t HostReceived(void) { return g_cloud_set_received; }
uint32_t HostOverflow(void) { return g_cloud_set_overflow; }
uint32_t HostDiscarded(void) { return g_cloud_set_discarded; }
void HostOffline(void) { g_air_mqtt_status=HAL_BUSY; }
