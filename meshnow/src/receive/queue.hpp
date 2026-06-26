#pragma once

#if IDF_VERSION_MAJOR >= 5 && IDF_VERSION_MINOR < 1
#include <freertos/portmacro.h>
#else
#include "freertos/FreeRTOS.h"
#endif
#include <esp_err.h>

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
 * A type defintion to represent priority level
 *
 * HIGHER is BETTER
 */
typedef std::size_t p_level;

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
 *
 * @warning the pushed the item with the highest
 * priority
 */
void push(Item&& item);

/**
 * Pushes a new item at a certain priority level
 *
 * @param item Item to push
 * @param level Priority level of the item
 */
void push(Item&& item, p_level level);
/**
 * Pops an item from the receive queue.
 *
 * @param item Item to pop.
 * @param timeout Timeout in ticks.
 *
 * @note When the queue has some non-trivial
 * priorities, this function fetches the 
 * highest-priority element. 
 * To prevent items with low priority to stay
 * in the queue for ever, a mechanism is set so
 * that once in a while low priority elements get
 * fetched.
 */
std::optional<Item> pop(TickType_t timeout);
}  // namespace meshnow::receive
