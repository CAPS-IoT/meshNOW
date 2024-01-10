#include "connect.hpp"

#include <esp_log.h>
#include <esp_wifi.h>
#include <nvs_flash.h>
#include <sdkconfig.h>

#include "layout.hpp"
#include "lock.hpp"
#include "meshnow.h"
#include "packets.hpp"
#include "send/queue.hpp"
#include "state.hpp"
#include "util/util.hpp"

namespace {

constexpr auto TAG = CREATE_TAG("Connect");

// How long to wait before sending a new search probe
constexpr auto SEARCH_PROBE_INTERVAL = pdMS_TO_TICKS(CONFIG_SEARCH_PROBE_INTERVAL);

// How many probes to send per channel before switching
constexpr auto PROBES_PER_CHANNEL = CONFIG_PROBES_PER_CHANNEL;

// Min time to wait for potential other parents after the first parent was found
constexpr auto FIRST_PARENT_WAIT = pdMS_TO_TICKS(CONFIG_FIRST_PARENT_WAIT);

constexpr auto MAX_PARENTS_TO_CONSIDER = CONFIG_MAX_PARENTS_TO_CONSIDER;

// Time to wait for a connection reply
constexpr auto CONNECT_TIMEOUT = pdMS_TO_TICKS(CONFIG_CONNECT_TIMEOUT);

}  // namespace

namespace meshnow::job {

ConnectJob::ConnectJob()
    :  // use lambda to initialize channel config
      channel_config_([] {
          // get channel config via wifi country
          wifi_country_t country;
          ESP_ERROR_CHECK(esp_wifi_get_country(&country));
          uint8_t min_channel = country.schan;
          uint8_t max_channel = min_channel + country.nchan - 1;
          return ChannelConfig{min_channel, max_channel};
      }()) {
    parent_infos_.reserve(MAX_PARENTS_TO_CONSIDER);
}

TickType_t ConnectJob::nextActionAt() const noexcept {
    // root never performs any connecting process
    if (state::isRoot()) return portMAX_DELAY;
    // forward to current phase
    return std::visit([](auto &phase) { return phase.nextActionAt(); }, phase_);
}

void ConnectJob::performAction() {
    // root never performs any connecting process
    if (state::isRoot()) return;
    // forward to current phase
    std::visit([&](auto &phase) { phase.performAction(*this); }, phase_);
}

void ConnectJob::event_handler(void *event_handler_arg, esp_event_base_t event_base, int32_t event_id,
                               void *event_data) {
    assert(event_base == event::MESHNOW_INTERNAL);

    // root never performs any connecting process
    if (state::isRoot()) return;

    auto &job = *static_cast<ConnectJob *>(event_handler_arg);
    // forward to current phase
    Lock lock;
    std::visit([&](auto &phase) { phase.event_handler(job, static_cast<event::InternalEvent>(event_id), event_data); },
               job.phase_);
}

// SEARCH PHASE //

TickType_t ConnectJob::SearchPhase::nextActionAt() const noexcept {
    if (!started_) return 0;
    // we always want to send a search probe
    return last_search_probe_time_ + SEARCH_PROBE_INTERVAL;
}

void ConnectJob::SearchPhase::performAction(ConnectJob &job) {
    if (!started_) {
        ESP_LOGI(TAG, "Starting search for potential parents");
        started_ = true;
    }

    if (job.parent_infos_.empty()) {
        // if we haven't found a parent yet, we switch the channel after a set amount of probes
        if (search_probes_sent_ >= PROBES_PER_CHANNEL) {
            // switch to next channel
            current_channel_++;
            // potentially wrap around
            if (current_channel_ > job.channel_config_.max_channel) {
                current_channel_ = job.channel_config_.min_channel;
            }
            ESP_LOGD(TAG, "Switching to channel %d", current_channel_);
            ESP_ERROR_CHECK(esp_wifi_set_channel(current_channel_, WIFI_SECOND_CHAN_NONE));
            search_probes_sent_ = 0;
        }
    } else {
        // have found at least one parent
        // keep searching for new ones, but after a while start the connecting phase
        if (xTaskGetTickCount() - first_parent_found_time_ > FIRST_PARENT_WAIT) {
            job.phase_ = ConnectPhase();
            return;
        }
    }

    // send a probe
    last_search_probe_time_ = xTaskGetTickCount();
    search_probes_sent_++;
    sendSearchProbe();
}

void ConnectJob::SearchPhase::event_handler(ConnectJob &job, event::InternalEvent event, void *event_data) {
    if (event != event::InternalEvent::PARENT_FOUND) return;

    auto &parent_data = *static_cast<event::ParentFoundData *>(event_data);
    auto parent_mac = parent_data.parent;
    auto parent_rssi = parent_data.rssi;

    // check if the advertised parent is already in the Layout, if so, ignore
    if (layout::Layout::get().has(parent_mac)) return;

    auto &parent_infos = job.parent_infos_;

    // if we have found the first parent, remember the time and save the channel
    if (parent_infos.empty()) {
        first_parent_found_time_ = xTaskGetTickCount();
        writeChannelToNVS(current_channel_);
    }

    // check if we already know this parent
    auto it = std::find_if(parent_infos.begin(), parent_infos.end(),
                           [&parent_mac](const ParentInfo &info) { return info.mac_addr == parent_mac; });

    if (it != parent_infos.end()) {
        ESP_LOGV(TAG, "Updating parent " MACSTR ". RSSI %d(old) -> %d(new)", MAC2STR(parent_mac), it->rssi,
                 parent_rssi);

        // update the RSSI
        it->rssi = parent_rssi;
    } else {
        ESP_LOGI(TAG, "Found new parent " MACSTR ". RSSI %d", MAC2STR(parent_mac), parent_rssi);

        // add the parent to the list, replacing the weakest parent if the list is full
        // but only replace if the new parent has a better RSSI
        if (parent_infos.size() >= MAX_PARENTS_TO_CONSIDER) {
            // find the weakest parent
            auto weakest_it =
                std::min_element(parent_infos.begin(), parent_infos.end(),
                                 [](const ParentInfo &a, const ParentInfo &b) { return a.rssi < b.rssi; });

            if (parent_rssi < weakest_it->rssi) return;

            // replace it with the new parent
            ESP_LOGI(TAG, "Replacing parent " MACSTR " with " MACSTR, MAC2STR(weakest_it->mac_addr),
                     MAC2STR(parent_mac));
            *weakest_it = ParentInfo{parent_mac, parent_rssi};
        } else {
            // just add the new parent
            parent_infos.push_back(ParentInfo{parent_mac, parent_rssi});
        }
    }
}

void ConnectJob::SearchPhase::sendSearchProbe() {
    ESP_LOGV(TAG, "Broadcasting search probe");
    send::enqueuePayload(packets::SearchProbe{}, send::DirectOnce{util::MacAddr::broadcast()});
}
uint8_t ConnectJob::SearchPhase::readChannelFromNVS(const ChannelConfig &channel_config) {
    nvs_handle_t nvs_handle;
    ESP_ERROR_CHECK(nvs_open("meshnow", NVS_READONLY, &nvs_handle));

    uint8_t channel;
    esp_err_t ret = nvs_get_u8(nvs_handle, "last_channel", &channel);

    nvs_close(nvs_handle);

    switch (ret) {
        case ESP_OK:
            if (channel >= channel_config.min_channel && channel <= channel_config.max_channel) {
                break;
            }
            [[fallthrough]];
        case ESP_ERR_NVS_NOT_FOUND:
            channel = channel_config.min_channel;
            break;
        default:
            ESP_ERROR_CHECK(ret);
    }

    return channel;
}

void ConnectJob::SearchPhase::writeChannelToNVS(uint8_t channel) {
    nvs_handle_t nvs_handle;
    ESP_ERROR_CHECK(nvs_open("meshnow", NVS_READWRITE, &nvs_handle));

    ESP_ERROR_CHECK(nvs_set_u8(nvs_handle, "last_channel", channel));

    ESP_ERROR_CHECK(nvs_commit(nvs_handle));

    nvs_close(nvs_handle);
}

// CONNECT PHASE //

TickType_t ConnectJob::ConnectPhase::nextActionAt() const noexcept {
    if (!started_) return 0;

    if (!awaiting_connect_response_) {
        return 0;
    } else {
        return last_connect_request_time_ + CONNECT_TIMEOUT;
    }
}

void ConnectJob::ConnectPhase::performAction(ConnectJob &job) {
    if (!started_) {
        ESP_LOGI(TAG, "Starting connect phase");
        started_ = true;
    }

    if (awaiting_connect_response_) {
        ESP_LOGI(TAG, "Connect request timed out");
        awaiting_connect_response_ = false;
    }

    // send a connect request to the best potential parent

    // get best parent
    auto it = std::max_element(job.parent_infos_.begin(), job.parent_infos_.end(),
                               [](const ParentInfo &a, const ParentInfo &b) { return a.rssi < b.rssi; });
    if (it == job.parent_infos_.end()) {
        ESP_LOGI(TAG, "All parents exhausted");
        job.phase_ = SearchPhase{job.channel_config_};
        return;
    }

    current_parent_mac_ = it->mac_addr;
    job.parent_infos_.erase(it);

    awaiting_connect_response_ = true;
    last_connect_request_time_ = xTaskGetTickCount();
    sendConnectRequest(current_parent_mac_);
}

void ConnectJob::ConnectPhase::event_handler(ConnectJob &job, event::InternalEvent event, void *event_data) {
    if (event != event::InternalEvent::GOT_CONNECT_RESPONSE) return;

    auto &response_data = *static_cast<event::GotConnectResponseData *>(event_data);
    auto parent_mac = response_data.parent;

    // got a wrong connection response
    if (parent_mac != current_parent_mac_) return;

    // got a correct connection response
    ESP_LOGI(TAG, "Got accepted by " MACSTR, MAC2STR(parent_mac));
    awaiting_connect_response_ = false;

    // we are now connected to the parent
    // set parent info
    auto &layout = layout::Layout::get();
    layout.setParent(parent_mac);

    // set root mac
    state::setRootMac(response_data.root);

    // update the state
    // we can assume to immediately reach the root since the parent also has to reach the root
    state::setState(state::State::REACHES_ROOT);

    // fire connect event
    {
        meshnow_event_parent_connected_t parent_connected_event;
        std::copy(parent_mac.addr.begin(), parent_mac.addr.end(), parent_connected_event.parent_mac);
        esp_event_post(MESHNOW_EVENT, meshnow_event_t::MESHNOW_EVENT_PARENT_CONNECTED, &parent_connected_event,
                       sizeof(parent_connected_event), portMAX_DELAY);
    }

    // we now want to perform the reset
    job.phase_ = DonePhase{};
}

void ConnectJob::ConnectPhase::sendConnectRequest(const util::MacAddr &to_mac) {
    ESP_LOGI(TAG, "Sending connect request to " MACSTR, MAC2STR(to_mac));
    send::enqueuePayload(packets::ConnectRequest{}, send::DirectOnce(to_mac));
}

// DonePhase //

TickType_t ConnectJob::DonePhase::nextActionAt() const noexcept {
    if (!started_) {
        return 0;
    } else {
        return portMAX_DELAY;
    }
}

// does nothing really
void ConnectJob::DonePhase::performAction(meshnow::job::ConnectJob &job) {
    if (!started_) {
        ESP_LOGI(TAG, "Connect job done!");
        started_ = true;
    }
}

void ConnectJob::DonePhase::event_handler(meshnow::job::ConnectJob &job, event::InternalEvent event, void *event_data) {
    if (event != event::InternalEvent::STATE_CHANGED) return;

    ESP_LOGI(TAG, "Got called!");

    auto state_change = *static_cast<event::StateChangedEvent *>(event_data);

    ESP_LOGI(TAG, "new State: %d", static_cast<uint8_t>(state_change.new_state));

    if (state_change.new_state == state::State::DISCONNECTED_FROM_PARENT) {
        job.phase_ = SearchPhase{job.channel_config_};
    }
}

}  // namespace meshnow::job
