#ifndef ONENET_CONFIG_H
#define ONENET_CONFIG_H

/* 本地认证信息：从 onenet_config.local.example.h 复制并填写。 */
#include "onenet_config.local.h"

/* 三个模块使用同一套设备身份，避免主题和登录参数不一致。 */
#define ONENET_TOPIC_PREFIX \
    "$sys/" ONENET_PRODUCT_ID "/" ONENET_DEVICE_NAME

#define ONENET_MQTT_CONFIG_COMMAND \
    "AT+MCONFIG=\"" ONENET_DEVICE_NAME "\",\"" ONENET_PRODUCT_ID \
    "\",\"" ONENET_TOKEN "\"\r\n"

#endif
