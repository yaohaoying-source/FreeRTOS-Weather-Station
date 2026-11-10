#include <stdint.h>
#include <string.h>
#include "stm32f1xx_hal.h"
char g_air_reply[256];
static uint32_t mode, calls, tcp_calls, tick, step, active, begins, ends, sends;
static HAL_StatusTypeDef begin_status;
static int saw_set_sub;
HAL_StatusTypeDef Air780E_BeginCommand(void) { begins++; return begin_status; }
void Air780E_EndCommand(void) { ends++; }
uint8_t Air780E_HasRxError(void) { return mode==6 && tick>0; }
uint32_t HAL_GetTick(void) { return tick; }
uint32_t ulTaskNotifyTake(int clear, uint32_t delay)
{ (void)clear; tick += delay ? delay : 1; return 0; }
HAL_StatusTypeDef Air780E_Send(const char *command)
{ (void)command; sends++; active=1; step=0; return HAL_OK; }
uint8_t Air780E_ReadLine(void)
{
    const char *line=0;
    if (!active) return 0;
    if (mode==3) {
        if(step==0) line="+MQTTSTATU: 1";
        if(step==1) line="OK";
    } else if (mode==4 && step==0) line="OK";
    step++;
    if(!line) return 0;
    strcpy(g_air_reply,line);
    return 1;
}
HAL_StatusTypeDef Air780E_SendAndWait(const char *cmd, const char *expected, uint32_t ms)
{
    (void)expected; (void)ms; calls++;
    if (strstr(cmd,"/thing/property/set")!=0) saw_set_sub=1;
    if (mode==1 && strstr(cmd,"/thing/event/post/reply")!=0) return HAL_ERROR;
    if (mode==2 && strstr(cmd,"AT+MIPSTART")!=0 && tcp_calls++==0) return HAL_BUSY;
    return HAL_OK;
}
#include "../../../Core/Src/onenet_connect.c"
void HostReset(uint32_t m)
{
    mode=m;calls=0;tcp_calls=0;tick=0;step=0;active=0;begins=0;ends=0;sends=0;
    begin_status=HAL_OK;saw_set_sub=0;
}
void HostBeginStatus(uint32_t s) { begin_status=(HAL_StatusTypeDef)s; }
uint32_t HostCalls(void) { return calls; }
uint32_t HostSawSet(void) { return (uint32_t)saw_set_sub; }
uint32_t HostBegins(void) { return begins; }
uint32_t HostEnds(void) { return ends; }
uint32_t HostSends(void) { return sends; }
uint32_t HostTick(void) { return tick; }
