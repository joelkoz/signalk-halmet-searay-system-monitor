#pragma once

#define USE_REMOTE_DEBUG

#ifdef USE_REMOTE_DEBUG
#include "RemoteDebug.h"

extern RemoteDebug Debug;

#define LOG_I(fmt, args...) debugI(fmt, ##args)
#define LOG_W(fmt, args...) debugW(fmt, ##args)
#define LOG_E(fmt, args...) debugE(fmt, ##args)
#define LOG_D(fmt, args...) debugD(fmt, ##args)
#else
#include "esp_log.h"
#ifndef TAG
#define TAG __FILENAME__
#endif
#define LOG_I(str, args...) ESP_LOGI(TAG, str, ##args) 
#define LOG_W(str, args...) ESP_LOGW(TAG, str, ##args)
#define LOG_E(str, args...) ESP_LOGE(TAG, str, ##args)
#define LOG_D(str, args...) ESP_LOGD(TAG, str, ##args)

#endif

void debug_setup();

void debug_loop();
