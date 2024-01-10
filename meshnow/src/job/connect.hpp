#pragma once

#include <esp_event.h>
#include <esp_random.h>

#include <optional>
#include <variant>
#include <vector>

#include "event.hpp"
#include "job.hpp"
#include "util/event.hpp"
#include "util/mac.hpp"

namespace meshnow::job {

class ConnectJob : public Job {
   public:
    ConnectJob();

    TickType_t nextActionAt() const noexcept override;

    void performAction() override;

   private:
    struct ChannelConfig {
        uint8_t min_channel;
        uint8_t max_channel;
    };

    struct ParentInfo {
        util::MacAddr mac_addr;
        int rssi{0};
    };

    /**
     * Searches for potential parents by performing an all-channel scan
     */
    class SearchPhase {
       public:
        explicit SearchPhase(const ChannelConfig& channel_config)
            : current_channel_(readChannelFromNVS(channel_config)) {}

        TickType_t nextActionAt() const noexcept;
        void performAction(ConnectJob& job);

        void event_handler(ConnectJob& job, event::InternalEvent event, void* event_data);

       private:
        /**
         * Sends a beacon to search for nearby parents willing to accept this node as a child.
         */
        static void sendSearchProbe();

        static uint8_t readChannelFromNVS(const ChannelConfig& channel_config);

        static void writeChannelToNVS(uint8_t channel);

        /**
         * If this phase just been started.
         */
        bool started_{false};

        /**
         * Ticks since the first parent was found while searching.
         */
        TickType_t first_parent_found_time_{0};

        /**
         * Ticks since the last search probe was sent.
         */
        TickType_t last_search_probe_time_{0};

        /**
         * How many search probes have been sent on the current channel.
         */
        uint search_probes_sent_{0};

        /**
         * The current channel we are searching on.
         */
        uint8_t current_channel_;
    };

    /**
     * Tries to connect to the best available parent.
     */
    class ConnectPhase {
       public:
        TickType_t nextActionAt() const noexcept;
        void performAction(ConnectJob& job);

        void event_handler(ConnectJob& job, event::InternalEvent event, void* event_data);

       private:
        /**
         * Sends a connect request to a potential parent.
         * @param to_mac MAC address of the parent
         */
        static void sendConnectRequest(const util::MacAddr& to_mac);

        /**
         * If this phase just been started.
         */
        bool started_{false};

        /**
         * Ticks since the last connect request was sent.
         */
        TickType_t last_connect_request_time_{0};

        /**
         * If currently awaiting a connect response.
         */
        bool awaiting_connect_response_{false};

        /**
         * The MAC address of the parent we are currently trying to connect to.
         */
        util::MacAddr current_parent_mac_;
    };

    /**
     * Idle phase, only reacts to state change when disconnecting from the parent.
     */
    class DonePhase {
       public:
        TickType_t nextActionAt() const noexcept;
        void performAction(ConnectJob& job);

        void event_handler(ConnectJob& job, event::InternalEvent event, void* event_data);

       private:
        /**
         * If this phase just been started.
         */
        bool started_{false};
    };

    using Phase = std::variant<SearchPhase, ConnectPhase, DonePhase>;

    static void event_handler(void* event_handler_arg, esp_event_base_t event_base, int32_t event_id, void* event_data);

    util::EventHandlerInstance event_handler_instance_{event::Internal::handle, event::MESHNOW_INTERNAL,
                                                       ESP_EVENT_ANY_ID, &ConnectJob::event_handler, this};

    const ChannelConfig channel_config_;
    std::vector<ParentInfo> parent_infos_;
    // starts per default with SearchPhase
    Phase phase_{SearchPhase{channel_config_}};
};

}  // namespace meshnow::job