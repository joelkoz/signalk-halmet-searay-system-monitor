#pragma once

// Application logging. LOG_* go through the ESP-IDF log system, which SensESP
// mirrors to the serial port and to the device's web log (/api/log).

#include "esp_log.h"

#ifndef TAG
#define TAG __FILENAME__
#endif

#define LOG_I(str, args...) ESP_LOGI(TAG, str, ##args)
#define LOG_W(str, args...) ESP_LOGW(TAG, str, ##args)
#define LOG_E(str, args...) ESP_LOGE(TAG, str, ##args)
#define LOG_D(str, args...) ESP_LOGD(TAG, str, ##args)

/**
 * @brief Sets the per-file log levels. Call once at the start of setup().
 */
void debug_setup();
