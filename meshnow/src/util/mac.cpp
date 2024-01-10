#include "mac.hpp"

namespace meshnow::util {

MacAddr MacAddr::broadcast() {
    static const MacAddr BROADCAST{{0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}};
    return BROADCAST;
}

MacAddr MacAddr::root() {
    static const MacAddr ROOT{{0x00, 0x00, 0x00, 0x00, 0x00, 0x00}};
    return ROOT;
}

MacAddr::MacAddr(std::array<uint8_t, 6> addr) : addr(addr) {}

MacAddr::MacAddr(const uint8_t* addr_ptr) { std::copy(addr_ptr, addr_ptr + 6, addr.begin()); }

bool MacAddr::isBroadcast() const { return addr == broadcast().addr; }
bool MacAddr::isRoot() const { return addr == root().addr; }

bool MacAddr::operator==(const MacAddr& rhs) const { return addr == rhs.addr; }
bool MacAddr::operator!=(const MacAddr& rhs) const { return !(rhs == *this); }

bool MacAddr::operator<(const MacAddr& rhs) const { return addr < rhs.addr; }
bool MacAddr::operator>(const MacAddr& rhs) const { return rhs < *this; }
bool MacAddr::operator<=(const MacAddr& rhs) const { return !(rhs < *this); }
bool MacAddr::operator>=(const MacAddr& rhs) const { return !(*this < rhs); }

uint8_t MacAddr::operator[](std::size_t index) const { return addr[index]; }

}  // namespace meshnow::util