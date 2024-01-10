#pragma once

#include <esp_mac.h>

#include <array>
#include <cstdint>

namespace meshnow::util {

struct MacAddr {
    static MacAddr broadcast();

    static MacAddr root();

    MacAddr() = default;

    explicit MacAddr(std::array<uint8_t, 6> addr);

    explicit MacAddr(const uint8_t* addr_ptr);

    bool isBroadcast() const;

    bool isRoot() const;

    bool operator==(const MacAddr& rhs) const;
    bool operator!=(const MacAddr& rhs) const;

    bool operator<(const MacAddr& rhs) const;
    bool operator>(const MacAddr& rhs) const;
    bool operator<=(const MacAddr& rhs) const;
    bool operator>=(const MacAddr& rhs) const;

    uint8_t operator[](std::size_t index) const;

    std::array<uint8_t, 6> addr{};
};

}  // namespace meshnow::util