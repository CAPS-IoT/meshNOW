#pragma once

#include "meshnow.h"

namespace meshnow::custom {

struct ActualCBHandle {
    ActualCBHandle* prev;
    ActualCBHandle* next;
    meshnow_data_cb_t cb;
};

ActualCBHandle* createCBHandle(meshnow_data_cb_t cb);

void destroyCBHandle(ActualCBHandle* handle);

ActualCBHandle* getFirstCBHandle();

void init();

void deinit();

}  // namespace meshnow::custom