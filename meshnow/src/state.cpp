#include "state.hpp"

#include <esp_log.h>

#include "event.hpp"
#include "layout.hpp"
#include "packets.hpp"
#include "send/queue.hpp"
#include "util/mac.hpp"
#include "util/util.hpp"

namespace meshnow::state {

namespace {

constexpr auto TAG = CREATE_TAG("State");

bool root{false};

util::MacAddr root_mac;
State state{State::DISCONNECTED_FROM_PARENT};

}  // namespace

void setState(State new_state) {
    ESP_LOGD(TAG, "Requested state change from %d to %d", static_cast<uint8_t>(state), static_cast<uint8_t>(new_state));
    if (new_state == state) return;

    event::StateChangedEvent data{
        .old_state = state,
        .new_state = new_state,
    };

    state = new_state;

    ESP_LOGI(TAG, "Firing event!");
    event::Internal::fire(event::InternalEvent::STATE_CHANGED, &data, sizeof(data));

    // don't send root reachable status events downstream if no children
    if (!layout::Layout::get().hasChildren()) return;

    // send to all children downstream
    if (new_state == State::REACHES_ROOT) {
        auto payload = packets::RootReachable{.root = getRootMac()};
        send::enqueuePayload(payload, send::DownstreamRetry());
    } else {
        assert(!isRoot());
        auto payload = packets::RootUnreachable{};
        send::enqueuePayload(payload, send::DownstreamRetry());
    }
}

State getState() { return state; }

void setRoot(bool is_root) { root = is_root; }

bool isRoot() { return root; }

void setRootMac(util::MacAddr mac) { root_mac = mac; }

util::MacAddr getRootMac() {
    // only if we can reach the root the saved mac will be valid
    assert(state == State::REACHES_ROOT);
    return root_mac;
}

util::MacAddr getThisMac() {
    // read once in the first call and cache the result
    static const auto mac = [] {
        util::MacAddr mac;
        esp_read_mac(mac.addr.data(), ESP_MAC_WIFI_STA);
        return mac;
    }();
    return mac;
}

}  // namespace meshnow::state