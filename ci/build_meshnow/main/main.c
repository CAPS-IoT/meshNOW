#include "meshnow.h"

void app_main(void) {
    meshnow_config_t config = {
        .root = false,
        .router_config = {.should_connect = false, .sta_config = NULL},
    };
    meshnow_init(&config);
}
