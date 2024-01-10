#pragma once

#include <esp_err.h>
#include <esp_event.h>

#include <optional>

#include "state.hpp"

namespace meshnow::event {

ESP_EVENT_DECLARE_BASE(MESHNOW_INTERNAL);

enum class InternalEvent : int32_t {
    STATE_CHANGED,
    PARENT_FOUND,
    GOT_CONNECT_RESPONSE,
};

struct StateChangedEvent {
    const state::State old_state;
    const state::State new_state;
};

struct ParentFoundData {
    const util::MacAddr parent;
    const int rssi;
};

struct GotConnectResponseData {
    const util::MacAddr parent;
    const util::MacAddr root;
};

class Internal {
   public:
    /**
     * Initializes internal event loop.
     */
    static esp_err_t init();

    /**
     * Deinitializes everything.
     */
    static void deinit();

    static void fire(InternalEvent event, void* data, size_t data_size);

    static esp_event_loop_handle_t handle;
};

}  // namespace meshnow::event