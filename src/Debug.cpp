#include "Debug.h"

#ifdef USE_REMOTE_DEBUG
#include "utils/ExpiryTimer.h"
RemoteDebug Debug;

static boolean remoteDebugEnabled = false;
static boolean enableRequired = false;
ExpiryTimer enableDelay;

static void onWifiConnected() {
   enableRequired = true;
   enableDelay.start(3000);
}

void debug_setup() {

    remoteDebugEnabled = false;
    enableRequired = false;

    // Echo debug info to serial port also...
    Debug.setSerialEnabled(true);

    // Can NOT call Debug.begin() without an active Wifi connection, so delay
    // this until we get notification of a station connection...
    WiFi.onEvent([](WiFiEvent_t event, WiFiEventInfo_t info) { onWifiConnected(); }, WiFiEvent_t::ARDUINO_EVENT_WIFI_STA_GOT_IP);
    WiFi.onEvent([](WiFiEvent_t event, WiFiEventInfo_t info) { onWifiConnected(); }, WiFiEvent_t::ARDUINO_EVENT_WIFI_AP_STACONNECTED);

    esp_log_level_set("*", ESP_LOG_INFO);
    esp_log_level_set("main.cpp", ESP_LOG_DEBUG);
}

void debug_loop() {
    if (enableRequired && enableDelay.expired()) {
      if (!remoteDebugEnabled) {
          Debug.begin(String("pp"), RemoteDebug::INFO);
          Debug.setResetCmdEnabled(true);
          remoteDebugEnabled = true;
          debugI("RemoteDebug is now enabled via Telnet");
      }
      enableRequired = false;
    }
    else {
       Debug.handle();
    }
}

#else

void debug_setup() {
  esp_log_level_set("*", ESP_LOG_INFO);
  esp_log_level_set("main.cpp", ESP_LOG_DEBUG);
}

void debug_loop() {   
}

#endif
