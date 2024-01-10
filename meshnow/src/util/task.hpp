#pragma once

#include <esp_err.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <memory>
#include <tuple>

#include "waitbits.hpp"

namespace meshnow::util {

/**
 * Which CPU (core) to run the task on.
 */
enum class CPU {
    PRO_CPU = 0,
    APP_CPU = 1,
    ANY_CPU = 2,
};

/**
 * Settings for a task.
 */
struct TaskSettings {
    const char* name;
    configSTACK_DEPTH_TYPE stack_size;
    UBaseType_t priority;
    CPU cpu;
};

/**
 * Helper struct that is passed to the static task function which is invoked by FreeRTOS.
 * @tparam Fn The type of the task function
 * @tparam Args The types of the arguments of the task function
 */
template <typename Fn, typename... Args>
struct TaskFunctionParams {
    Fn&& task_function;
    std::tuple<Args&&...> args;
};

/**
 * Static task function that does the heavy lifting by calling the real user-supplied task function.
 * @tparam Fn  The type of the task function
 * @tparam Args The types of the arguments of the task function
 * @param arg Void pointer that carries the parameter helper struct
 */
template <typename Fn, typename... Args>
static void taskFunction(void* arg) {
    auto params = static_cast<TaskFunctionParams<Fn, Args...>*>(arg);

    // execute real task function
    std::apply(params->task_function, params->args);

    // free parameters
    delete params;

    // suspend so that parent task can delete this task -> this way we don't accidentally delete the task twice
    vTaskSuspend(nullptr);
}

/**
 * Wrapper around a FreeRTOS task.
 */
class Task {
   public:
    template <typename Fn, typename... Args>
    esp_err_t init(TaskSettings settings, Fn&& task_function, Args&&... args) {
        TaskHandle_t handle;

        // heap-allocate param helper struct
        auto params = new TaskFunctionParams<Fn, Args...>{std::forward<Fn>(task_function),
                                                          std::forward_as_tuple(std::forward<Args>(args)...)};

        BaseType_t affinity = tskNO_AFFINITY;
        switch (settings.cpu) {
            case CPU::PRO_CPU:
                affinity = PRO_CPU_NUM;
                break;
            case CPU::APP_CPU:
                affinity = APP_CPU_NUM;
                break;
            case CPU::ANY_CPU:
                affinity = tskNO_AFFINITY;
                break;
        }

        if (xTaskCreatePinnedToCore(&taskFunction<Fn, Args...>, settings.name, settings.stack_size, params,
                                    settings.priority, &handle, affinity) == pdPASS) {
            // save task handle
            task_handle_.reset(handle);
            return ESP_OK;
        } else {
            // free parameters
            delete params;
            return ESP_ERR_NO_MEM;
        }
    }

   private:
    struct Deleter {
        void operator()(TaskHandle_t task_handle) const { vTaskDelete(task_handle); }
    };

    std::unique_ptr<std::remove_pointer<TaskHandle_t>::type, Deleter> task_handle_;
};

}  // namespace meshnow::util