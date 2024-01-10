#include "event.hpp"

#include <esp_task.h>

#include "meshnow.h"

namespace meshnow::event {

ESP_EVENT_DEFINE_BASE(MESHNOW_INTERNAL);

esp_err_t Internal::init() {
    // TODO all these values should be config values
    esp_event_loop_args_t args{
        .queue_size = 16,
        .task_name = "meshnow_internal",
        .task_priority = ESP_TASKD_EVENT_PRIO,
        .task_stack_size = ESP_TASKD_EVENT_STACK,
        .task_core_id = 0,
    };
    return esp_event_loop_create(&args, &handle);
}

void Internal::deinit() {
    assert(handle != nullptr);
    ESP_ERROR_CHECK(esp_event_loop_delete(handle));
    handle = nullptr;
}

void Internal::fire(meshnow::event::InternalEvent event, void* data, size_t data_size) {
    ESP_ERROR_CHECK(
        esp_event_post_to(handle, MESHNOW_INTERNAL, static_cast<int32_t>(event), data, data_size, portMAX_DELAY));
}

esp_event_loop_handle_t Internal::handle{nullptr};

}  // namespace meshnow::event