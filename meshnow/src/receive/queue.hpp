#pragma once

#include <esp_err.h>
#include <freertos/portmacro.h>

#include <optional>
#include <utility>

#include "packets.hpp"
#include "util/mac.hpp"
#include "util/util.hpp"

namespace meshnow::receive {

struct Item {
    Item(util::MacAddr from, int rssi, packets::Packet packet) : from(from), rssi(rssi), packet(std::move(packet)) {}

    util::MacAddr from;
    int rssi;
    packets::Packet packet;
};

/**
 * Initializes receive queue.
 */
esp_err_t init();

/**
 * Deinitializes receive queue.
 */
void deinit();

/**
 * Pushes a new item to the receive queue.
 *
 * @param item Item to push.
 */
void push(Item&& item);

/**
 * Pops an item from the receive queue.
 *
 * @param item Item to pop.
 * @param timeout Timeout in ticks.
 */
std::optional<Item> pop(TickType_t timeout);

}  // namespace meshnow::receive