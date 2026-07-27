#ifndef __MQTT_CLI_H__
#define __MQTT_CLI_H__


#include "FreeRTOS_CLI.h"
BaseType_t prvMQTTformationCommand( char *pcWriteBuffer, size_t xWriteBufferLen, const char *pcCommandString );

#endif