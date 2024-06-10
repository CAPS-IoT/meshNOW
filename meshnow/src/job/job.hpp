#pragma once

#if IDF_VERSION_MAJOR >= 5 && IDF_VERSION_MINOR < 1
#include <freertos/portmacro.h>
#else
#include "freertos/FreeRTOS.h"
#endif

namespace meshnow::job {

class Job {
   public:
    virtual ~Job() = default;

    /**
     * @return the time at which the next action should be performed
     */
    virtual TickType_t nextActionAt() const noexcept = 0;

    /**
     * Perform the next action.
     */
    virtual void performAction() = 0;
};

}  // namespace meshnow::job
