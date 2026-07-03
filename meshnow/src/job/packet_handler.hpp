#pragma once

#include "packets.hpp"

namespace meshnow::job {

struct MetaData {
    const util::MacAddr last_hop;
    const util::MacAddr from;
    const int rssi;
};

/**
 * Handles incoming packets.
 */
class PacketHandler {
   public:
    /**
     * Handle a packet. Calls the corresponding private methods.
     * @param from the mac address of the sender
     * @param packet the packet to handle
     */
    static void handlePacket(const util::MacAddr& from, int rssi, const packets::Packet& packet);

   private:
    // HANDLERS for each payload type //

    static void handle(const MetaData& meta, const packets::Status& p);
    static void handle(const MetaData& meta, const packets::SearchProbe& p);
    static void handle(const MetaData& meta, const packets::SearchReply& p);
    static void handle(const MetaData& meta, const packets::ConnectRequest& p);
    static void handle(const MetaData& meta, const packets::ConnectOk& p);
    #if CONFIG_USE_CONNECT_OK_ACK_MESSAGE
    static void handle(const MetaData& meta, const packets::ConnectOkAck& p);
    #endif
    static void handle(const MetaData& meta, const packets::RoutingTableAdd& p);
    static void handle(const MetaData& meta, const packets::RoutingTableRemove& p);
    static void handle(const MetaData& meta, const packets::RootUnreachable& p);
    static void handle(const MetaData& meta, const packets::RootReachable& p);
    #if CONFIG_USE_CONNECT_END_MESSAGE
    static void handle(const MetaData& meta, const packets::ConnectEnd& p);
    #endif
    static void handle(const MetaData& meta, const packets::DataFragment& p);
    static void handle(const MetaData& meta, const packets::CustomData& p);
};

}  // namespace meshnow::job