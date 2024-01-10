#include "meshnow_runner.h"

#include <esp_err.h>
#include <meshnow.h>

#include "prepare.h"

void run_meshnow() {
    // initialize meshnow
    meshnow_config_t config = {
        .root = is_root(),
        .router_config =
            {
                .should_connect = false,
            },
    };
    ESP_ERROR_CHECK(meshnow_init(&config));

    // start meshnow
    ESP_ERROR_CHECK(meshnow_start());
}