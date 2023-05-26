#pragma once

#include <cstdint>
#include <future>
#include <queue.hpp>
#include <thread>
#include <waitbits.hpp>

#include "constants.hpp"
#include "packets.hpp"

namespace meshnow {

class Networking;

class SendResult {
   public:
    explicit SendResult(bool successful) : successful_{successful} {}

    bool isOk() const { return successful_; }

   private:
    bool successful_;
};

using SendPromise = std::promise<SendResult>;

enum class QoS : uint8_t {
    FIRE_AND_FORGET,   // Resolves the promise with a result immediately after the send callback is called
    WAIT_ACK_TIMEOUT,  // Waits a certain amount of time for an ACK matching the packet's sequence number
};

/**
 * Thread that handles sending payloads by queueing them and working them off one by one.
 */
class SendWorker {
   public:
    SendWorker() = default;

    SendWorker(const SendWorker&) = delete;
    SendWorker& operator=(const SendWorker&) = delete;

    /**
     * Starts the SendWorker.
     */
    void start();

    /**
     * Stops the SendWorker.
     */
    void stop();

    /**
     * Add packet to the send queue.
     *
     * @note Blocks if send queue is full.
     *
     * @param dest_addr MAC address of the immediate node to send the packet to
     * @param packet packet to send
     * @param result_promise promise to resolve according to the chosen QoS
     * @param priority whether the packet should be put in front of the queue
     * @param qos quality of service
     */
    void enqueuePacket(const MAC_ADDR& dest_addr, packets::Packet packet, SendPromise&& result_promise, bool priority,
                       QoS qos);

    /**
     * Notify the SendWorker that the previous payload was sent.
     */
    void sendFinished(bool successful);

   private:
    struct SendQueueItem {
        packets::Packet packet;
        SendPromise result_promise;
        MAC_ADDR dest_addr;
        QoS qos;
    };

    /**
     * Loop of the SendWorker thread.
     * @param stoken stop token to interrupt the loop
     */
    void runLoop(std::stop_token stoken);

    /**
     * Communicates a successful/failed payload from the send callback to the thread.
     */
    util::WaitBits waitbits_{};

    // TODO extract max_items to constants.hpp
    util::Queue<SendQueueItem> send_queue_{10};

    std::jthread run_thread_{};
};

}  // namespace meshnow