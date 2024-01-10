#pragma once

#include <stdbool.h>

#define LIGHT_GPIO GPIO_NUM_19

void prepare();

bool is_root();

bool is_target();