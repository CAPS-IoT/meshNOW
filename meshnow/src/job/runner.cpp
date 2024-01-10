#include "runner.hpp"

#include <esp_log.h>

#include "connect.hpp"
#include "fragment_gc.hpp"
#include "freertos/portmacro.h"
#include "job.hpp"
#include "keep_alive.hpp"
#include "lock.hpp"
#include "packet_handler.hpp"
#include "receive/queue.hpp"
#include "util/util.hpp"
#include "util/waitbits.hpp"

namespace meshnow::job {

static constexpr auto TAG = CREATE_TAG("JobRunner");
static constexpr auto MIN_TIMEOUT = pdMS_TO_TICKS(5000);

using JobList = std::initializer_list<std::reference_wrapper<Job>>;

static TickType_t calculateTimeout(JobList jobs) {
    // we want to at least check every 500ms, if not for the stop request
    auto timeout = MIN_TIMEOUT;

    auto now = xTaskGetTickCount();
    // go through every task and check if it has a sooner timeout
    for (auto job : jobs) {
        TickType_t next_action;
        {
            Lock lock;
            next_action = job.get().nextActionAt();
        }
        TickType_t this_timeout;
        if (next_action == portMAX_DELAY) {
            // in case of the maximum delay, we don't want to subtract now, as it is assumed the task never wants to run
            this_timeout = portMAX_DELAY;
        } else if (next_action > now) {
            this_timeout = next_action - now;
        } else {
            // run immediately
            this_timeout = 0;
        }

        // update timeout with potentially sooner timeout
        if (this_timeout < timeout) {
            timeout = this_timeout;
        }

        // break early
        if (timeout == 0) {
            break;
        }
    }

    return timeout;
}

void runner_task(bool& should_stop, util::WaitBits& task_waitbits, int job_runner_finished_bit) {
    ESP_LOGI(TAG, "Starting!");

    ConnectJob hand_shaker;
    FragmentGCJob fragment_gc;
    StatusSendJob status_send;
    UnreachableTimeoutJob unreachable_timeout;
    NeighborCheckJob neighbor_check;

    JobList jobs{hand_shaker, fragment_gc, status_send, unreachable_timeout, neighbor_check};

    auto lastLoopRun = xTaskGetTickCount();

    while (!should_stop) {
        // calculate timeout
        auto timeout = calculateTimeout(jobs);

        ESP_LOGV(TAG, "Next action in at most %lu ticks", timeout);

        // get next packet from receive queue
        auto receive_item = receive::pop(timeout);
        if (receive_item) {
            // handle packet
            PacketHandler::handlePacket(receive_item->from, receive_item->rssi, receive_item->packet);
        }

        // perform tasks
        for (auto now = xTaskGetTickCount(); auto job : jobs) {
            // only perform the action if it is due
            if (job.get().nextActionAt() <= now) {
                Lock lock;
                job.get().performAction();
            }
        }

        // wait at least one tick to avoid triggering the watchdog
        xTaskDelayUntil(&lastLoopRun, 1);
    }

    ESP_LOGI(TAG, "Stopping!");

    // finished
    task_waitbits.set(job_runner_finished_bit);
}

}  // namespace meshnow::job