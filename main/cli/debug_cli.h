#ifndef __DEBUG_CLI_H__
#define __DEBUG_CLI_H__


#include "FreeRTOS_CLI.h"


BaseType_t prvDebugformationCommand( char *pcWriteBuffer, size_t xWriteBufferLen, const char *pcCommandString );
typedef struct {
    uint32_t DBG_EN      : 1;  // 1비트 사용 (0 ~ 1)
    uint32_t TOF         : 1;  // 1비트 사용
    uint32_t HX711       : 1;  // 1비트 사용
    uint32_t moter       : 1;  // 1비트 사용
    uint32_t adc         : 1;  // 4비트 사용 (0 ~ 15)
    uint32_t button      : 1; // 12비트 사용 (0 ~ 4095)
    uint32_t ble         : 1; // 12비트 사용 (0 ~ 4095)
    uint32_t wifi        : 1; // 12비트 사용 (0 ~ 4095)    
    uint32_t led         : 1; // 12비트 사용 (0 ~ 4095)       
    uint32_t reserved    : 23; // 남은 12비트 (패딩) -> 총합 32비트
} DBG_Resister_t;

DBG_Resister_t* Debug_Get(void);

#endif
