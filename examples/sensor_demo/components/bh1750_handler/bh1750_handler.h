#pragma once

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#ifdef __cplusplus
extern "C" {
#endif

void caps_bh1750_i2c_init(void);

void caps_bh1750_init(void);

uint16_t caps_bh1750_measure(void);

#ifdef __cplusplus
}
#endif