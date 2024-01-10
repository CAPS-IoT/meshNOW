#pragma once

#include "util/waitbits.hpp"

namespace meshnow::send {

void worker_task(bool& should_stop, util::WaitBits& task_waitbits, int send_worker_finished_bit);

}