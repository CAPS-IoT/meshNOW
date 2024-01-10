#pragma once

#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>

namespace meshnow {

class Lock {
   public:
    Lock();
    ~Lock();

    Lock(const Lock&) = delete;

    Lock& operator=(const Lock&) = delete;

    Lock(Lock&& other) = delete;

    Lock& operator=(Lock&& other) = delete;

   private:
    static SemaphoreHandle_t handle_;
};

}  // namespace meshnow