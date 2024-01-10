#pragma once

#include <esp_err.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

#include <memory>
#include <optional>

namespace meshnow::util {

/**
 * Wraps a FreeRTOS thread-safe queue.
 * Lord forgive me for what I have done...
 */
template <typename T>
class Queue {
   public:
    Queue() = default;

    Queue(const Queue&) = delete;

    Queue& operator=(const Queue&) = delete;

    Queue(Queue&& other) noexcept : queue_handle_{std::move(other.queue_handle_)} {}

    Queue& operator=(Queue&& other) noexcept {
        queue_handle_ = std::move(other.queue_handle_);
        return *this;
    }

    esp_err_t init(size_t num_items) {
        auto handle = xQueueCreate(num_items, sizeof(T));
        if (handle != nullptr) {
            queue_handle_.reset(handle);
            return ESP_OK;
        } else {
            return ESP_ERR_NO_MEM;
        }
    }

    bool push_back(T&& item, TickType_t ticksToWait) const {
        alignas(T) uint8_t buffer[sizeof(T)];
        new (buffer) T{std::move(item)};
        return xQueueSendToBack(queue_handle_.get(), static_cast<const void*>(buffer), ticksToWait);
    }

    bool push_front(T&& item, TickType_t ticksToWait) const {
        alignas(T) uint8_t buffer[sizeof(T)];
        new (buffer) T{std::move(item)};
        return xQueueSendToFront(queue_handle_.get(), static_cast<const void*>(buffer), ticksToWait);
    }

    std::optional<T> pop(TickType_t ticksToWait) const {
        alignas(T) uint8_t buffer[sizeof(T)];
        if (xQueueReceive(queue_handle_.get(), static_cast<void*>(buffer), ticksToWait)) {
            return std::make_optional(std::move(*reinterpret_cast<T*>(buffer)));
        } else {
            return std::nullopt;
        }
    }

    void clear() const { xQueueReset(queue_handle_.get()); }

    size_t spaces_available() const { return uxQueueSpacesAvailable(queue_handle_.get()); }

    size_t items_waiting() const { return uxQueueMessagesWaiting(queue_handle_.get()); }

   private:
    struct Deleter {
        void operator()(QueueHandle_t queue_handle) { vQueueDelete(queue_handle); }
    };

    std::unique_ptr<std::remove_pointer<QueueHandle_t>::type, Deleter> queue_handle_;
};

}  // namespace meshnow::util
