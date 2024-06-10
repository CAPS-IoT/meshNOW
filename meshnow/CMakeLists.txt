file(GLOB_RECURSE SRC_FILES src/*.cpp)

idf_component_register(
  SRCS
  "${SRC_FILES}"
  INCLUDE_DIRS
  "src/include"
  PRIV_INCLUDE_DIRS
  "src"
  REQUIRES
  esp_wifi
  esp_system
  freertos
  PRIV_REQUIRES
  nvs_flash
  lwip
  bitsery
  espnow_multi)
