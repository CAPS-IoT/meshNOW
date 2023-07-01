#pragma once

#include "job.hpp"

namespace meshnow::job {

/**
 * Removes old incomplete fragment reassemblies.
 */
class FragmentGCJob : public meshnow::job::Job {
   public:
    TickType_t nextActionAt() const noexcept override;
    void performAction() override;
};

}  // namespace meshnow::job