#pragma once

#if IDF_VERSION_MAJOR >= 5 && IDF_VERSION_MINOR < 1
#include <freertos/portmacro.h>
#else
#include "freertos/FreeRTOS.h"
#endif

#include <esp_err.h>

#include <memory>
#include <optional>

#include "def.hpp"
#include "packets.hpp"
#include "util/mac.hpp"
#include "util/util.hpp"

namespace meshnow::send {

struct Item {
    packets::Payload payload;
    SendBehavior behavior;
    uint32_t id;
};

/**
 * Initializes the send queue.
 */
esp_err_t init();

/**
 * Deinitializes the send queue.
 */
void deinit();

/**
 * Enqueues a new payload to be sent.
 * @param packet The payload to send
 * @param behavior The behavior to use for sending
 * @param id The id of the packet
 */
void enqueuePayload(const packets::Payload& payload, SendBehavior behavior, uint32_t id);

void enqueuePayload(const packets::Payload& payload, SendBehavior behavior);

std::optional<Item> popItem(TickType_t timeout);

}  // namespace meshnow::send
