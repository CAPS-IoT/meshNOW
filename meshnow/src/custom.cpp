#include "custom.hpp"

namespace meshnow::custom {

namespace {

ActualCBHandle* first_cb_handle{nullptr};

}

void init() {
    // NOP
}

void deinit() {
    ActualCBHandle* handle = first_cb_handle;

    while (handle != nullptr) {
        ActualCBHandle* next = handle->next;
        destroyCBHandle(handle);
        handle = next;
    }
}

ActualCBHandle* createCBHandle(meshnow_data_cb_t cb) {
    auto* handle = new ActualCBHandle();
    handle->prev = nullptr;
    handle->next = first_cb_handle;
    handle->cb = cb;

    if (first_cb_handle != nullptr) {
        first_cb_handle->prev = handle;
    }

    first_cb_handle = handle;

    return handle;
}

void destroyCBHandle(ActualCBHandle* handle) {
    if (handle->prev != nullptr) {
        handle->prev->next = handle->next;
    }

    if (handle->next != nullptr) {
        handle->next->prev = handle->prev;
    }

    if (first_cb_handle == handle) {
        first_cb_handle = handle->next;
    }

    delete handle;
}

ActualCBHandle* getFirstCBHandle() { return first_cb_handle; }

}  // namespace meshnow::custom