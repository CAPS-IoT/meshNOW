#pragma once

#include <esp_err.h>

#include <memory>

#include "netif.hpp"
#include "receive/receiver.hpp"
#include "util/task.hpp"
#include "util/waitbits.hpp"

namespace meshnow {

class Networking {
   public:
    Networking() = default;

    Networking(const Networking&) = delete;
    Networking& operator=(const Networking&) = delete;

    esp_err_t init();

    void deinit();

    /**
     * Starts the networking stack.
     */
    esp_err_t start();

    /**
     * Stops the networking stack.
     */
    void stop();

   private:
    // receiver is initialized here, which sets up everything ESP-NOW related automatically
    std::shared_ptr<receive::Receiver> receiver_;

    // tasks
    util::WaitBits task_waitbits_;
    bool stop_tasks_{false};
    util::Task job_runner_task_;
    util::Task send_worker_task_;

    NowNetif netif_;
};

}  // namespace meshnow
