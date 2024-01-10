#pragma once

#include <freertos/portmacro.h>

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