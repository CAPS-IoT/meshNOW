#pragma once

#include <esp_now.h>

#include <array>
#include <cstdint>

namespace meshnow {

// PACKETS
constexpr std::array<uint8_t, 3> MAGIC{0x55, 0x77, 0x55};
constexpr auto HEADER_SIZE{20};
constexpr auto MAX_FRAG_PAYLOAD_SIZE{ESP_NOW_MAX_DATA_LEN - HEADER_SIZE - 6};
constexpr auto MAX_CUSTOM_PAYLOAD_SIZE{ESP_NOW_MAX_DATA_LEN - HEADER_SIZE};

// TASKS
constexpr auto TASK_PRIORITY{23};

}  // namespace meshnow