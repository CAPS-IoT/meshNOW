#include "queue.hpp"

#include <esp_random.h>

#include <utility>

#include "util/queue.hpp"

static constexpr auto QUEUE_SIZE{32};
// TODO QUEUE_SIZE has to be higher so not to get deadlocks! FIND A REAL SOLUTION!

namespace meshnow::send {

static util::Queue<Item> queue;

esp_err_t init() { return queue.init(QUEUE_SIZE); }

void deinit() { queue = util::Queue<Item>{}; }

void enqueuePayload(const packets::Payload& payload, SendBehavior behavior, uint32_t id) {
    queue.push_back(Item{payload, std::move(behavior), id}, portMAX_DELAY);
}

void enqueuePayload(const packets::Payload& payload, SendBehavior behavior) {
    enqueuePayload(payload, std::move(behavior), esp_random());
}

std::optional<Item> popItem(TickType_t timeout) { return queue.pop(timeout); }

}  // namespace meshnow::send