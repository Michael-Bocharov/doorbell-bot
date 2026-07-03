#ifndef MOCK_FREERTOS_H
#define MOCK_FREERTOS_H

#include <stdint.h>
#include <stddef.h>

typedef uint32_t TickType_t;
typedef int32_t BaseType_t;
typedef uint32_t UBaseType_t;

#define pdFALSE 0
#define pdTRUE  1

#define portTICK_PERIOD_MS 1
#define pdMS_TO_TICKS(xInMs) ((TickType_t)(xInMs))

#endif // MOCK_FREERTOS_H
