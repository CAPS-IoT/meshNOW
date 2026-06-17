#include "queue.hpp"
#include <optional>

#include "util/queue.hpp"

static constexpr auto QUEUE_TIMEOUT{pdMS_TO_TICKS(100)};
static constexpr std::size_t PRIORITY_LEVELS{3};
static constexpr std::size_t PEOPLE_PER_LEVEL{2};
static constexpr auto QUEUE_SIZE{128};
// TODO QUEUE_SIZE has to be higher so not to get deadlocks! FIND A REAL SOLUTION!

namespace meshnow::receive {

static std::array<util::Queue<Item>, PRIORITY_LEVELS> priority_queues;
//this array helps to insure some amount of fairness between priority levels 
static std::array<std::size_t, PRIORITY_LEVELS> crowded_timeouts;

esp_err_t init() { 
    for (auto& queue : priority_queues) {
        auto err = queue.init(QUEUE_SIZE);
        if (err != ESP_OK) return err;
    }
    for (auto i = 0; i < PRIORITY_LEVELS; i++) {
        crowded_timeouts[i] = (PRIORITY_LEVELS - i)*PEOPLE_PER_LEVEL;
    }
    return ESP_OK;
}

void deinit() { 
    for (auto& queue : priority_queues) {
        queue = util::Queue<Item>{};
    }
}

void push(Item&& item) { priority_queues[0].push_back(std::move(item), QUEUE_TIMEOUT); }

void push(Item&& item, p_level level) {
    if (level < PRIORITY_LEVELS) {
        priority_queues[level].push_back(std::move(item), QUEUE_TIMEOUT);
    } else {
        //the given priority level is higher than the max priority defined in PRIORITY_LEVELS
        //hence we push this element at the front of the biggest priority subqueue
        priority_queues[PRIORITY_LEVELS-1].push_front(std::move(item), QUEUE_TIMEOUT);
    }
}
//std::optional<Item> pop(TickType_t timeout) { return priority_queue[0].pop(timeout); }

std::optional<Item> pop(TickType_t timeout) {    
    for (int i = PRIORITY_LEVELS - 1; i >= 0; --i) {
        //we first go to the high priority queues that still have some
        //messages to be poped
        if (priority_queues[i].items_waiting() && crowded_timeouts[i]-- > 0) {
            //if the crowded_timeouts[i] counter is >= 0, then fetches the elements from the queue
            return priority_queues[i].pop(timeout);
        } else {
            //Here, it could happen that 
            //  1) we're here because priority_queues[i] was empty. Hence we allow it to have a init value counter
            //  2) we're here because crowded_timouts[i] has reached 0. This means that, over the last 
            //      (PRIORITY_LEVELS-i)*PEOPLE_PER_LEVEL pop() calls, the poped item was coming from 
            //      the queue i. This is too much and i needs to let lower priorities be popped too.
            //Note that, the higher the priority, the lower the crowded_timouts threshold. This allows to have a 
            //fairer popping system.
            crowded_timeouts[i] = (PRIORITY_LEVELS - i)*PEOPLE_PER_LEVEL;
        }
    }
    //just a fallback if very queue is empty
    return priority_queues[PRIORITY_LEVELS-1].pop(timeout);
}

}  // namespace meshnow::receive

