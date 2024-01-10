#pragma once

#include "util/waitbits.hpp"

namespace meshnow::job {

void runner_task(bool& should_stop, util::WaitBits& task_waitbits, int job_runner_finished_bit);

}  // namespace meshnow::job