#pragma once

#include "freertos/FreeRTOS.h"
#include "job.hpp"

namespace meshnow::job {

/**
 * Removes old incomplete fragment reassemblies.
 */
class FragmentGCJob : public Job {
   public:
    TickType_t nextActionAt() const noexcept override;
    void performAction() override;
};

}  // namespace meshnow::job
