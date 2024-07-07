#include "keep_alive.hpp"

#include <esp_log.h>

#include "layout.hpp"
#include "meshnow.h"
#include "send/queue.hpp"
#include "state.hpp"
#include "util/util.hpp"

namespace meshnow::job {

static constexpr auto TAG = CREATE_TAG("KeepAlive");

// send a keep alive status every 1s
static constexpr auto STATUS_SEND_INTERVAL = pdMS_TO_TICKS(CONFIG_STATUS_SEND_INTERVAL);

// consider a neighbor dead if no beacon was received for 3s
static constexpr auto KEEP_ALIVE_TIMEOUT = pdMS_TO_TICKS(CONFIG_KEEP_ALIVE_TIMEOUT);

// disconnect from parent if the root was unreachable for 10s
static constexpr auto ROOT_UNREACHABLE_TIMEOUT = pdMS_TO_TICKS(CONFIG_ROOT_UNREACHABLE_TIMEOUT);

// StatusSendJob //

TickType_t StatusSendJob::nextActionAt() const noexcept {
    if (!layout::Layout::get().isEmpty()) {
        return last_status_sent_ + STATUS_SEND_INTERVAL;
    } else {
        return portMAX_DELAY;
    }
}

void StatusSendJob::performAction() {
    auto now = xTaskGetTickCount();
    if (now - last_status_sent_ < STATUS_SEND_INTERVAL) return;

    if (layout::Layout::get().isEmpty()) return;

    sendStatus();

    last_status_sent_ = now;
}

void StatusSendJob::sendStatus() {
    ESP_LOGD(TAG, "Sending status beacons to neighbors");
    auto state = state::getState();

    packets::Status payload{
        .state = state,
        .root = state == state::State::REACHES_ROOT ? std::make_optional(state::getRootMac()) : std::nullopt,
    };

    send::enqueuePayload(payload, send::NeighborsOnce{});
}

// UnreachableTimeoutJob //

TickType_t UnreachableTimeoutJob::nextActionAt() const noexcept {
    return awaiting_reachable ? mesh_unreachable_since_ + ROOT_UNREACHABLE_TIMEOUT : portMAX_DELAY;
}

void UnreachableTimeoutJob::performAction() {
    auto now = xTaskGetTickCount();

    if (awaiting_reachable && now - mesh_unreachable_since_ > ROOT_UNREACHABLE_TIMEOUT) {
        // timeout from waiting for a path to the root

        ESP_LOGI(TAG, "Timeout from waiting for a path to the root");

        awaiting_reachable = false;

        // if we haven't lost the parent by now because of a Keep Alive timeout, remove it
        auto& layout = layout::Layout::get();
        if (layout.hasParent()) {
            // fire disconnect event
            {
                meshnow_event_parent_disconnected_t parent_disconnected_event;
                util::MacAddr& parent_mac = layout::Layout::get().getParent().mac;
                std::copy(parent_mac.addr.begin(), parent_mac.addr.end(), parent_disconnected_event.parent_mac);
                esp_event_post(MESHNOW_EVENT, meshnow_event_t::MESHNOW_EVENT_PARENT_DISCONNECTED,
                               &parent_disconnected_event, sizeof(parent_disconnected_event), portMAX_DELAY);
            }

            layout.removeParent();
            state::setState(state::State::DISCONNECTED_FROM_PARENT);  // set state to disconnected
        }
    }
}

void UnreachableTimeoutJob::event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    assert(event_base == event::MESHNOW_INTERNAL);
    assert(event_id == static_cast<int32_t>(event::InternalEvent::STATE_CHANGED));

    auto& job = *static_cast<UnreachableTimeoutJob*>(arg);
    auto data = *static_cast<event::StateChangedEvent*>(event_data);
    auto new_state = data.new_state;
    auto old_state = data.old_state;

    if (job.awaiting_reachable) {
        if (old_state == state::State::CONNECTED_TO_PARENT && new_state == state::State::REACHES_ROOT) {
            // root is reachable again
            ESP_LOGI(TAG, "Root is reachable again");
        }
        job.awaiting_reachable = false;
        job.mesh_unreachable_since_ = 0;
    } else {
        if (old_state == state::State::REACHES_ROOT && new_state == state::State::CONNECTED_TO_PARENT) {
            // root became unreachable
            ESP_LOGI(TAG, "Root became unreachable");
            job.awaiting_reachable = true;
            job.mesh_unreachable_since_ = xTaskGetTickCount();
        }
    }
}

// NeighborsCheckJob //

TickType_t NeighborCheckJob::nextActionAt() const noexcept {
    // get the neighbor with the lowest last_seen timestamp
    auto min_last_seen = portMAX_DELAY;

    auto& layout = layout::Layout::get();

    for (const auto& child : layout.getChildren()) {
        min_last_seen = std::min(min_last_seen, child.last_seen);
    }

    if (layout.hasParent()) {
        min_last_seen = std::min(min_last_seen, layout.getParent().last_seen);
    }

    if (min_last_seen == portMAX_DELAY) {
        // no neighbors
        return portMAX_DELAY;
    } else {
        return min_last_seen + KEEP_ALIVE_TIMEOUT;
    }
}

void NeighborCheckJob::performAction() {
    auto& layout = layout::Layout::get();
    auto now = xTaskGetTickCount();

    // check for timeouts in all neighbors and remove from the layout if necessary

    // direct children
    for (auto it = layout.getChildren().begin(); it != layout.getChildren().end();) {
        if (now - it->last_seen > KEEP_ALIVE_TIMEOUT) {
            auto mac = it->mac;
            ESP_LOGW(TAG, "Direct child " MACSTR " timed out", MAC2STR(mac));

            // fire disconnect event
            {
                meshnow_event_child_disconnected_t child_disconnected_event;
                util::MacAddr& parent_mac = layout::Layout::get().getParent().mac;
                std::copy(parent_mac.addr.begin(), parent_mac.addr.end(), child_disconnected_event.child_mac);
                esp_event_post(MESHNOW_EVENT, meshnow_event_t::MESHNOW_EVENT_CHILD_DISCONNECTED,
                               &child_disconnected_event, sizeof(child_disconnected_event), portMAX_DELAY);
            }

            layout.removeChild(it->mac);
            // send event upstream
            sendChildDisconnected(mac);
        } else {
            ++it;
        }
    }

    // parent
    if (layout.hasParent()) {
        auto& parent = layout.getParent();
        if (now - parent.last_seen > KEEP_ALIVE_TIMEOUT) {
            ESP_LOGW(TAG, "Parent " MACSTR " timed out", MAC2STR(parent.mac));

            // fire disconnect event
            {
                meshnow_event_parent_disconnected_t parent_disconnected_event;
                util::MacAddr& parent_mac = layout::Layout::get().getParent().mac;
                std::copy(parent_mac.addr.begin(), parent_mac.addr.end(), parent_disconnected_event.parent_mac);
                esp_event_post(MESHNOW_EVENT, meshnow_event_t::MESHNOW_EVENT_PARENT_DISCONNECTED,
                               &parent_disconnected_event, sizeof(parent_disconnected_event), portMAX_DELAY);
            }

            layout.removeParent();
            state::setState(state::State::DISCONNECTED_FROM_PARENT);
        }
    }
}

void NeighborCheckJob::sendChildDisconnected(const util::MacAddr& mac) {
    if (state::isRoot()) return;
    if (!layout::Layout::get().hasParent()) return;

    ESP_LOGI(TAG, "Sending child disconnected event upstream");

    // send to parent
    auto payload = packets::RoutingTableRemove{mac};
    send::enqueuePayload(payload, send::UpstreamRetry{});
}

}  // namespace meshnow::job