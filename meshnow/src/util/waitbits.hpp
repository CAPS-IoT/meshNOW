#pragma once

#include <esp_err.h>
#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>

#include <memory>

namespace meshnow::util {

/**
 * Wraps a FreeRTOS Event Group for WaitBits.
 */
class WaitBits {
   public:
    WaitBits() = default;

    WaitBits(const WaitBits&) = delete;

    WaitBits& operator=(const WaitBits&) = delete;

    WaitBits(WaitBits&& other) noexcept : event_group_handle_{std::move(other.event_group_handle_)} {}

    WaitBits& operator=(WaitBits&& other) noexcept {
        event_group_handle_ = std::move(other.event_group_handle_);
        return *this;
    }

    esp_err_t init() {
        auto handle = xEventGroupCreate();
        if (handle != nullptr) {
            event_group_handle_.reset(handle);
            return ESP_OK;
        } else {
            return ESP_ERR_NO_MEM;
        }
    }

    /**
     * Wait for the given bits to be set.
     * @param bits the bits to wait for (bitwise OR)
     * @param clearOnExit clear the set bits when function returns normally (no timeout)
     * @param waitForAllBits if true waits for all bits to be set, otherwise waits for any bit to be set
     * @param ticksToWait how long to wait for the bits to be set
     * @return the bits that were set during the timeout period
     */
    EventBits_t wait(EventBits_t bits, bool clearOnExit, bool waitForAllBits, TickType_t ticksToWait) {
        return xEventGroupWaitBits(event_group_handle_.get(), bits, clearOnExit, waitForAllBits, ticksToWait);
    }

    /**
     * Set the given bits.
     * @param bits the bits to set (bitwise OR)
     * @return the bits that were set at the time that this call returns
     */
    EventBits_t set(EventBits_t bits) { return xEventGroupSetBits(event_group_handle_.get(), bits); }

    /**
     * Clear the given bits.
     * @param bits the bits to clear (bitwise OR)
     * @return the bits that were cleared before this call
     */
    EventBits_t clear(EventBits_t bits) { return xEventGroupClearBits(event_group_handle_.get(), bits); }

    /**
     * Get the current bits.
     * @return the current bits
     */
    EventBits_t get() { return xEventGroupGetBits(event_group_handle_.get()); }

   private:
    struct Deleter {
        void operator()(EventGroupHandle_t event_group_handle) { vEventGroupDelete(event_group_handle); }
    };

    std::unique_ptr<std::remove_pointer<EventGroupHandle_t>::type, Deleter> event_group_handle_;
};
}  // namespace meshnow::util