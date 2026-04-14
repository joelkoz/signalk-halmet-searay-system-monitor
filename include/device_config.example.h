#pragma once

#include <stdint.h>

// Setup:
// 1. Copy this file to include/device_config.h
// 2. Edit the values for your boat
// 3. Build again

constexpr const char* kDeviceHostname = "sk-monitor";
constexpr const char* kBoatWifiSsid = "your-wifi-ssid";
constexpr const char* kBoatWifiPassword = "your-wifi-password";
constexpr const char* kSignalKServerHost = "signalk-server.local";
constexpr uint16_t kSignalKServerPort = 3000;
constexpr const char* kShipTimeZoneName = "US Eastern";
constexpr const char* kShipTimeZonePosix = "EST5EDT,M3.2.0/2,M11.1.0/2";
