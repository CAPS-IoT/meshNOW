#pragma once

#if IDF_VERSION_MAJOR >= 5 && IDF_VERSION_MINOR < 1
#include <freertos/portmacro.h>
#else
#include "freertos/FreeRTOS.h"
#endif

#include <cstdint>
#include <optional>

#include "util/mac.hpp"
#include "util/util.hpp"

namespace meshnow::fragments {

/**
 * Initializes fragment handling.
 */
esp_err_t init();

/**
 * Deinitializes fragment handling.
 */
void deinit();

/**
 * Add a fragment for processing.
 *
 * @param src_mac MAC of the node the fragment originated from
 * @param fragment_id Random ID to identify which fragments belong together
 * @param fragment_number Number of this fragment in the sequence of fragments [0, 7]
 * @param total_size Total size of the data over all fragments in bytes [0, 1500]
 * @param data Data of this fragment
 */
void addFragment(const util::MacAddr& src_mac, uint16_t fragment_id, uint16_t fragment_number, uint16_t total_size,
                 util::Buffer data);

/**
 * Return the next reassembled data.
 * @param timeout Timeout to wait for data
 * @return Reassembled data or std::nullopt if no data was received
 */
std::optional<util::Buffer> popReassembledData(TickType_t timeout);

/**
 * Return the time of the youngest fragment.
 * @return Time of the youngest fragment, or portMAX_DELAY if no fragments exist
 */
TickType_t youngestFragmentTime();

/**
 * Remove all fragments that are older than the given time.
 */
void removeOlderThan(TickType_t time);

}  // namespace meshnow::fragments
