#pragma once

#include <esp_event.h>

namespace meshnow::util {

/**
 * A RAII wrapper for an ESP-IDF event handler instance.
 */
class EventHandlerInstance {
   public:
    EventHandlerInstance(esp_event_loop_handle_t event_loop, esp_event_base_t event_base, int32_t event_id,
                         esp_event_handler_t event_handler, void* event_handler_arg)
        :  // other params needed for the destructor
          event_loop_(event_loop),
          event_base_(event_base),
          event_id_(event_id) {
        ESP_ERROR_CHECK(esp_event_handler_instance_register_with(event_loop, event_base, event_id, event_handler,
                                                                 event_handler_arg, &instance_));
    }

    EventHandlerInstance(const EventHandlerInstance&) = delete;

    EventHandlerInstance& operator=(const EventHandlerInstance&) = delete;

    ~EventHandlerInstance() {
        ESP_ERROR_CHECK(esp_event_handler_instance_unregister_with(event_loop_, event_base_, event_id_, instance_));
    }

   private:
    esp_event_handler_instance_t instance_{nullptr};
    esp_event_loop_handle_t event_loop_;
    esp_event_base_t event_base_;
    int32_t event_id_;
};

}  // namespace meshnow::util