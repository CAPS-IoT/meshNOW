#pragma once

#include <cstdint>
#include <optional>
#include <variant>

#include "state.hpp"
#include "util/mac.hpp"
#include "util/util.hpp"

namespace meshnow::packets {

struct Status {
    state::State state;
    std::optional<util::MacAddr> root;
#if CONFIG_USE_RTT_FOR_TIMEOUT
    uint32_t seq;
#endif
};

struct SearchProbe {};

struct SearchReply {};

struct ConnectRequest {};

struct ConnectOk {
    util::MacAddr root;
};

#if CONFIG_USE_CONNECT_OK_ACK_MESSAGE
struct ConnectOkAck {};
#endif

#if CONFIG_USE_CONNECT_END_MESSAGE
struct ConnectEnd {};
#endif


struct RoutingTableAdd {
    util::MacAddr entry;
};

struct RoutingTableRemove {
    util::MacAddr entry;
};

struct RootUnreachable {};

struct RootReachable {
    util::MacAddr root;
};

struct DataFragment {
    uint32_t frag_id;
    union {
        struct {
            uint16_t frag_num : 3;
            uint16_t total_size : 11;
            uint16_t : 2;  // unused
        } unpacked;
        uint16_t packed;
    } options;
    util::Buffer data;
};

struct CustomData {
    util::Buffer data;
};


using Payload = std::variant<Status, SearchProbe, SearchReply, ConnectRequest, ConnectOk, 
                            RoutingTableAdd, RoutingTableRemove, RootUnreachable, RootReachable, 
                            DataFragment,
                            #if CONFIG_USE_CONNECT_OK_ACK_MESSAGE
                            ConnectOkAck,
                            #endif
                            #if CONFIG_USE_CONNECT_END_MESSAGE
                            ConnectEnd,
                            #endif
                            CustomData>;

struct Packet {
    uint32_t id;
    util::MacAddr from;
    util::MacAddr to;
    Payload payload;
};

/**
 * Serialize the given packet into a byte buffer
 * @param packet The packet to serialize
 * @return The serialized packet as a byte buffer
 */
util::Buffer serialize(const Packet& packet);

/**
 * Deserialize the given byte buffer into a packet
 * @param buffer The byte buffer to deserialize
 * @return The deserialized packet. If the buffer is invalid,
 * std::nullopt is returned
 */
std::optional<Packet> deserialize(const util::Buffer& buffer);

}  // namespace meshnow::packets
