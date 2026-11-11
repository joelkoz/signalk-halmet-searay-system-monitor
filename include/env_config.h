#ifndef ENV_CONFIG_H_
#define ENV_CONFIG_H_

#include <stdint.h>

// Device settings for this build.
//
// Do not put values here. They come from the project's .env file (copy
// .env-example to .env), which scripts/load_dotenv.py injects into the build as
// CFG_* macros. The same .env drives the OTA upload target and password, so the
// password compiled into the firmware always matches the one used to upload.

#ifndef CFG_DEVICE_HOSTNAME
#error "Device settings missing: copy .env-example to .env and fill it in (see README)."
#endif

constexpr const char* kDeviceHostname = CFG_DEVICE_HOSTNAME;
constexpr const char* kBoatWifiSsid = CFG_WIFI_SSID;
constexpr const char* kBoatWifiPassword = CFG_WIFI_PASSWORD;
constexpr const char* kSignalKServerHost = CFG_SIGNALK_HOST;
constexpr uint16_t kSignalKServerPort = CFG_SIGNALK_PORT;
constexpr const char* kOtaPassword = CFG_OTA_PASSWORD;

// Time zone configuration for the boat
constexpr const char* kShipTimeZoneName = CFG_SHIP_TZ_NAME;
constexpr const char* kShipTimeZonePosix = CFG_SHIP_TZ_POSIX;

#endif  // ENV_CONFIG_H_
