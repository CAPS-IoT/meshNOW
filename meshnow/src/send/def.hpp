#pragma once

#include <memory>
#include <variant>

#include "packets.hpp"
#include "util/mac.hpp"

namespace meshnow::send {

/**
 * A sink accepts packets to be sent and does the actual underlying sending.
 */
class SendSink {
   public:
    virtual ~SendSink() = default;

    /**
     * Accepts a packet to be sent.
     * @param next_hop the address of the very next hop to actually send to
     * @param from the address written as the from field
     * @return to the address written as the to field
     */
    virtual bool accept(const util::MacAddr& next_hop, const util::MacAddr& from, const util::MacAddr& to) = 0;

    /**
     * Retries later.
     */
    virtual void requeue() = 0;
};

class DirectOnce {
   public:
    explicit DirectOnce(const util::MacAddr& dest_addr) : dest_addr_(dest_addr) {}

    void send(SendSink& sink);

   private:
    util::MacAddr dest_addr_;
};

class NeighborsOnce {
   public:
    void send(SendSink& sink);
};

class UpstreamRetry {
   public:
    void send(SendSink& sink);
};

class DownstreamRetry {
   public:
    void send(SendSink& sink);

   private:
    std::vector<util::MacAddr> failed_;
};

class FullyResolve {
   public:
    FullyResolve(const util::MacAddr& from, const util::MacAddr& to, const util::MacAddr& prev_hop)
        : from(from), to(to), prev_hop(prev_hop) {}

    void send(SendSink& sink);

   private:
    void broadcast(SendSink& sink);

    void root(SendSink& sink);

    void parent(SendSink& sink);

    void child(SendSink& sink);

    util::MacAddr from;
    util::MacAddr to;
    util::MacAddr prev_hop;
    std::vector<util::MacAddr> broadcast_failed_;
};

using SendBehavior = std::variant<DirectOnce, NeighborsOnce, UpstreamRetry, DownstreamRetry, FullyResolve>;

}  // namespace meshnow::send