#include "lock.hpp"

namespace meshnow {

SemaphoreHandle_t Lock::handle_{nullptr};

Lock::Lock() {
    if (handle_ == nullptr) {
        handle_ = xSemaphoreCreateMutex();
        assert(handle_ && "Failed to create global mutex!");
    }
    xSemaphoreTake(handle_, portMAX_DELAY);
}

Lock::~Lock() { xSemaphoreGive(handle_); }

}  // namespace meshnow