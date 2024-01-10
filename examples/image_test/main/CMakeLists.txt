idf_component_register(SRCS "main.c" "prepare.c" "demo_mqtt.c" INCLUDE_DIRS "." PRIV_REQUIRES meshnow nvs_flash driver mqtt esp_timer)
