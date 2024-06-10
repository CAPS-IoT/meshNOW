#include "fragments.hpp"

#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#if IDF_VERSION_MAJOR >= 5 && IDF_VERSION_MINOR < 1
#include <freertos/portmacro.h>
#endif
#include <freertos/task.h>

#include <algorithm>
#include <map>

#include "constants.hpp"
#include "util/queue.hpp"

namespace meshnow::fragments {

static constexpr auto TAG = CREATE_TAG("Fragments");

static constexpr auto QUEUE_SIZE{32};
// TODO QUEUE_SIZE has to be higher so not to get deadlocks! FIND A REAL SOLUTION!

static util::Queue<util::Buffer> finished_queue;

/**
 * Data that is being reassembled.
 */
class ReassemblyData {
   public:
    explicit ReassemblyData(uint16_t total_size)
        : data_(total_size),
          // rounds up to the next integer
          num_fragments((total_size + MAX_FRAG_PAYLOAD_SIZE - 1) / MAX_FRAG_PAYLOAD_SIZE) {
        // reserve buffer beforehand
        ESP_LOGV(TAG, "Reserving %d bytes for reassembly", total_size);
        data_.reserve(total_size);
    }

    void insert(uint8_t frag_num, const util::Buffer& data) {
        ESP_LOG_BUFFER_HEXDUMP(TAG, data.data(), data.size(), ESP_LOG_VERBOSE);
        // set bit to indicate this fragment was received
        fragment_mask |= 1 << frag_num;
        // copy to the correct position
        std::copy(data.begin(), data.end(), data_.begin() + MAX_FRAG_PAYLOAD_SIZE * frag_num);
        // update time
        last_fragment_received_ = xTaskGetTickCount();
    }

    bool isComplete() const noexcept { return fragment_mask == (1 << num_fragments) - 1; }

    util::Buffer getData() noexcept { return data_; }

    TickType_t lastFragmentReceived() const noexcept { return last_fragment_received_; }

   private:
    // Reassembled data
    util::Buffer data_;

    // Number of fragments that are expected.
    uint8_t num_fragments;

    // Each bit in the mask corresponds to a fragment. If the bit is set, the fragment was received.
    uint8_t fragment_mask{0};

    // When the last fragment was received in ticks since boot.
    TickType_t last_fragment_received_{0};
};

/**
 * "Uniquely" identifies a data entry with a source MAC address and a fragment ID.
 */
using ReassemblyKey = std::pair<util::MacAddr, uint16_t>;

static std::map<ReassemblyKey, ReassemblyData> reassembly_map;

esp_err_t init() { return finished_queue.init(QUEUE_SIZE); }

void deinit() {
    reassembly_map.clear();
    finished_queue = util::Queue<util::Buffer>{};
}

void addFragment(const util::MacAddr& src_mac, uint16_t fragment_id, uint16_t fragment_number, uint16_t total_size,
                 util::Buffer data) {
    ESP_LOGV(TAG, "Received fragment %d from message %d with size %d/%d", fragment_number, fragment_id, data.size(),
             total_size);

    // short-circuit logic if it is the first and only fragment
    if (fragment_number == 0 && total_size == data.size()) {
        finished_queue.push_back(std::move(data), portMAX_DELAY);
        return;
    }

    auto key = ReassemblyKey{src_mac, fragment_id};

    // check if we already have an entry for this fragment
    auto it = reassembly_map.find(key);
    if (it == reassembly_map.end()) {
        // no entry yet, create one
        auto entry = ReassemblyData{total_size};
        entry.insert(fragment_number, data);
        reassembly_map.emplace(key, std::move(entry));
        return;
    }

    // entry already exists, add the fragment
    it->second.insert(fragment_number, data);

    // check if the data is complete
    if (it->second.isComplete()) {
        // data is complete, move it to the finished queue
        finished_queue.push_back(it->second.getData(), portMAX_DELAY);
        reassembly_map.erase(it);
    }
}

std::optional<util::Buffer> popReassembledData(TickType_t timeout) { return finished_queue.pop(timeout); }

TickType_t youngestFragmentTime() {
    // if empty return max time
    if (reassembly_map.empty()) {
        return portMAX_DELAY;
    }

    // find the youngest fragment and return its time
    auto youngest = std::min_element(reassembly_map.begin(), reassembly_map.end(), [](const auto& a, const auto& b) {
        return a.second.lastFragmentReceived() < b.second.lastFragmentReceived();
    });
    return youngest->second.lastFragmentReceived();
}

void removeOlderThan(TickType_t time) {
    for (auto it = reassembly_map.begin(); it != reassembly_map.end();) {
        if (it->second.lastFragmentReceived() < time) {
            it = reassembly_map.erase(it);
        } else {
            ++it;
        }
    }
}

}  // namespace meshnow::fragments
