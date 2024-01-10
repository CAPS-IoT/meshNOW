#include "queue.hpp"

#include "util/queue.hpp"

static constexpr auto QUEUE_SIZE{32};
// TODO QUEUE_SIZE has to be higher so not to get deadlocks! FIND A REAL SOLUTION!

namespace meshnow::receive {

static util::Queue<Item> queue;

esp_err_t init() { return queue.init(QUEUE_SIZE); }

void deinit() { queue = util::Queue<Item>{}; }

void push(Item&& item) { queue.push_back(std::move(item), portMAX_DELAY); }

std::optional<Item> pop(TickType_t timeout) { return queue.pop(timeout); }

}  // namespace meshnow::receive