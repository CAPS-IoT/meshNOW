#include "networking.hpp"

#include <esp_check.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "constants.hpp"
#include "fragments.hpp"
#include "job/runner.hpp"
#include "netif.hpp"
#include "receive/queue.hpp"
#include "send/queue.hpp"
#include "send/worker.hpp"
#include "util/util.hpp"
#include "util/waitbits.hpp"

static constexpr auto TAG = CREATE_TAG("Networking");

namespace meshnow {

static constexpr auto JOB_RUNNER_FINISHED_BIT = BIT0;
static constexpr auto SEND_WORKER_FINISHED_BIT = BIT1;

esp_err_t Networking::init() {
    ESP_LOGI(TAG, "Initializing");

    ESP_RETURN_ON_ERROR(send::init(), TAG, "Failed to initialize send queue");
    ESP_RETURN_ON_ERROR(receive::init(), TAG, "Failed to initialize receive queue");
    ESP_RETURN_ON_ERROR(task_waitbits_.init(), TAG, "Failed to initialize task waitbits");
    ESP_RETURN_ON_ERROR(fragments::init(), TAG, "Failed to initialize fragment reassembly");
    ESP_RETURN_ON_ERROR(netif_.init(), TAG, "Failed to initialize custom netif");

    // init receiver
    receiver_ = std::make_shared<receive::Receiver>();
    espnow_multi::EspnowMulti::getInstance()->addReceiver(receiver_);

    return ESP_OK;
}

void Networking::deinit() {
    ESP_LOGI(TAG, "Deinitializing");

    // reverse order of init
    netif_.deinit();
    fragments::deinit();
    receive::deinit();
    send::deinit();
}

esp_err_t Networking::start() {
    ESP_LOGI(TAG, "Starting");

    stop_tasks_ = false;

    constexpr auto priority = TASK_PRIORITY;
    constexpr auto cpu = util::CPU::APP_CPU;

    // TODO adapt stack size, right now it is ridiculously high but works

    // start job runner
    {
        auto settings = util::TaskSettings{"job_runner", 5000, priority, cpu};
        ESP_RETURN_ON_ERROR(job_runner_task_.init(settings, &job::runner_task, std::ref(stop_tasks_),
                                                  std::ref(task_waitbits_), JOB_RUNNER_FINISHED_BIT),
                            TAG, "Failed to create job runner task");
    }

    // start send worker
    {
        auto settings = util::TaskSettings{"send_worker", 5000, priority, cpu};
        ESP_RETURN_ON_ERROR(send_worker_task_.init(settings, &send::worker_task, std::ref(stop_tasks_),
                                                   std::ref(task_waitbits_), SEND_WORKER_FINISHED_BIT),
                            TAG, "Failed to create send worker task");
    }

    //    // init netif
    //    netif_->init();
    //
    //    // start both workers
    //    main_worker_->start();
    //    send_worker_->start();
    //
    //    // if we are root, we can also start it already
    //    if (state_->isRoot()) {
    //        netif_->start();
    //    }

    // TODO turn into event
    netif_.start();

    return ESP_OK;
}

void Networking::stop() {
    ESP_LOGI(TAG, "Stopping");

    // TODO turn into event
    netif_.stop();

    stop_tasks_ = true;

    // wait until both tasks are finished
    task_waitbits_.wait(JOB_RUNNER_FINISHED_BIT | SEND_WORKER_FINISHED_BIT, true, true, portMAX_DELAY);

    // reset both tasks
    job_runner_task_ = util::Task();
    send_worker_task_ = util::Task();
}

}  // namespace meshnow