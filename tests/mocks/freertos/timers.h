#ifndef MOCK_FREERTOS_TIMERS_H
#define MOCK_FREERTOS_TIMERS_H

#include "FreeRTOS.h"

typedef void *TimerHandle_t;
typedef void (*TimerCallbackFunction_t)(TimerHandle_t xTimer);

TimerHandle_t xTimerCreate(const char * const pcTimerName,
                           const TickType_t xTimerPeriodInTicks,
                           const UBaseType_t uxAutoReload,
                           void * const pvTimerID,
                           TimerCallbackFunction_t pxCallbackFunction);

BaseType_t xTimerChangePeriod(TimerHandle_t xTimer,
                              const TickType_t xNewPeriod,
                              const TickType_t xTicksToWait);

BaseType_t xTimerStart(TimerHandle_t xTimer, const TickType_t xTicksToWait);
BaseType_t xTimerStop(TimerHandle_t xTimer, const TickType_t xTicksToWait);

#endif // MOCK_FREERTOS_TIMERS_H
