#include "Debug.h"

void debug_setup() {
  esp_log_level_set("*", ESP_LOG_INFO);
  esp_log_level_set("main.cpp", ESP_LOG_DEBUG);
  // SensESP's Saveable::load() logs an INFO line whenever a component is
  // constructed with an empty config_path (e.g. transient SK listeners that
  // don't persist state). This is expected/benign; suppress it.
  esp_log_level_set("saveable.cpp", ESP_LOG_WARN);
}
