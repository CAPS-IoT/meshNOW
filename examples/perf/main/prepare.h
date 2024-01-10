#pragma once

#include <stdbool.h>

typedef enum {
    MESHNOW,
    ESPMESH,
} impl_t;

typedef struct {
    impl_t impl;
    bool long_range;
} setup_config_t;

void prepare();

setup_config_t get_setup_config();

bool is_root();