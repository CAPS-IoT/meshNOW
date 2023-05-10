#include <esp_log.h>
#include <unity.h>

#include <cstdint>
#include <memory>
#include <numeric>
#include <vector>

#include "constants.hpp"
#include "networking.hpp"

const char* TAG = "test_packets";

/**
 * Serializes and deserializes the given payload, performs type checks and returns the deserialized payload for further
 * testing.
 */
std::unique_ptr<meshnow::packet::BasePayload> basic_check(const meshnow::packet::BasePayload& payload) {
    // serialize
    std::vector<uint8_t> buffer = meshnow::packet::Packet{payload}.serialize();
    ESP_LOG_BUFFER_HEXDUMP(TAG, buffer.data(), buffer.size(), ESP_LOG_INFO);
    // deserialize
    auto compare = meshnow::packet::Packet::deserialize(buffer);

    // check deserialization was successful and type matches
    TEST_ASSERT_NOT_NULL(compare);
    TEST_ASSERT_MESSAGE(compare->type() == payload.type(), "Type mismatch");

    return compare;
}

TEST_CASE("still_alive", "[serialization matches]") { basic_check(meshnow::packet::StillAlivePayload{}); }

TEST_CASE("anyone_there", "[serialization matches]") { basic_check(meshnow::packet::AnyoneTherePayload{}); }

TEST_CASE("i_am_here", "[serialization matches]") { basic_check(meshnow::packet::IAmHerePayload{}); }

TEST_CASE("pls_connect", "[serialization matches]") { basic_check(meshnow::packet::PlsConnectPayload{}); }

TEST_CASE("welcome", "[serialization matches]") { basic_check(meshnow::packet::WelcomePayload{}); }

TEST_CASE("node_connected", "[serialization matches]") {
    meshnow::MAC_ADDR mac_addr{1, 2, 3, 4, 5, 6};

    meshnow::packet::NodeConnectedPayload payload{mac_addr};
    auto compare = basic_check(payload);

    // TODO
}

TEST_CASE("node_disconnected", "[serialization matches]") {
    meshnow::MAC_ADDR mac_addr{1, 2, 3, 4, 5, 6};

    meshnow::packet::NodeDisconnectedPayload payload{mac_addr};
    auto compare = basic_check(payload);

    // TODO
}

TEST_CASE("data_ack", "[serialization matches]") {
    meshnow::MAC_ADDR mac_addr{1, 2, 3, 4, 5, 6};
    uint16_t seq_num = 1234;

    meshnow::packet::DataAckPayload payload{mac_addr, seq_num};
    auto compare = basic_check(payload);

    // TODO
}

TEST_CASE("data_nack", "[serialization matches]") {
    meshnow::MAC_ADDR mac_addr{1, 2, 3, 4, 5, 6};
    uint16_t seq_num = 1234;

    meshnow::packet::DataNackPayload payload{mac_addr, seq_num};
    auto compare = basic_check(payload);

    // TODO
}

TEST_CASE("data_first", "[serialization matches]") {
    // test both custom and lwip data
    bool custom = false;

    do {
        meshnow::MAC_ADDR mac_addr{1, 2, 3, 4, 5, 6};
        uint16_t seq_num = 1234;
        uint16_t payload_size = meshnow::MAX_DATA_FIRST_SIZE;
        // data vector consists of increasing numbers
        std::vector<uint8_t> data(payload_size);
        std::iota(data.begin(), data.end(), 0);

        meshnow::packet::DataFirstPayload payload{mac_addr, seq_num, meshnow::MAX_DATA_TOTAL_SIZE, custom, data};
        auto compare = basic_check(payload);

        // TODO

        custom = !custom;
    } while (custom);
}

TEST_CASE("data_next", "[serialization matches]") {
    // test both custom and lwip data
    bool custom = false;

    do {
        meshnow::MAC_ADDR mac_addr{1, 2, 3, 4, 5, 6};
        uint16_t seq_num = 1234;
        uint8_t frag_num = 5;
        // data vector consists of increasing numbers
        std::vector<uint8_t> data(meshnow::MAX_DATA_NEXT_SIZE);
        std::iota(data.begin(), data.end(), 0);

        meshnow::packet::DataNextPayload payload{mac_addr, seq_num, frag_num, custom, data};
        auto compare = basic_check(payload);

        // TODO

        custom = !custom;
    } while (custom);
}

TEST_CASE("mesh_unreachable", "[serialization matches]") { basic_check(meshnow::packet::MeshUnreachablePayload{}); }

TEST_CASE("mesh_reachable", "[serialization matches]") { basic_check(meshnow::packet::MeshReachablePayload{}); }
