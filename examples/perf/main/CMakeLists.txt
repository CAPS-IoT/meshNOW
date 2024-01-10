idf_component_register(SRCS "main.c" "prepare.c" "meshnow_runner.c" "espmesh_runner.c" "espmesh_netif.c" INCLUDE_DIRS "." PRIV_REQUIRES iperf meshnow nvs_flash driver)
