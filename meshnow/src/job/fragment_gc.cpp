#include "fragment_gc.hpp"

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include "fragments.hpp"
#include "util/util.hpp"

namespace meshnow::job {

static constexpr auto TAG = CREATE_TAG("FragmentGC");

// each reassembly process can have its last fragment received up to 3 seconds ago
static constexpr auto FRAGMENT_TIMEOUT = pdMS_TO_TICKS(CONFIG_FRAGMENT_TIMEOUT);

TickType_t FragmentGCJob::nextActionAt() const noexcept {
    auto youngestFragmentTime = fragments::youngestFragmentTime();

    if (youngestFragmentTime == portMAX_DELAY) {
        // no fragments, don't need to do anything
        return portMAX_DELAY;
    } else {
        return youngestFragmentTime + FRAGMENT_TIMEOUT;
    }
}

void FragmentGCJob::performAction() {
    auto now = xTaskGetTickCount();
    if (now < FRAGMENT_TIMEOUT) return;  // don't do anything if we haven't been running for long enough

    // remove all entries that have timed out
    fragments::removeOlderThan(now - FRAGMENT_TIMEOUT);
}

}  // namespace meshnow::job
