#pragma once

#include <Arduino.h>

#include "sensesp/system/observablevalue.h"
#include "sensesp/system/saveable.h"

namespace app {

/**
 * @brief Configurable Signal K heartbeat component to report device life to
 * Signal K.
 *
 * HeartbeatReporter is a complex component that wires a
 * SensESP producer chain that emits device uptime (in seconds) to a
 * Signal K path. Both the heartbeat interval and the Signal K path are
 * user configurable. The first value is published at startup, and
 * onSKServerConnect() republishes the current uptime so the server sees the
 * device as alive as soon as the connection is (re)established.
 */
class HeartbeatReporter : public sensesp::FileSystemSaveable {
 public:
  /**
   * @brief Constructs and wires the heartbeat producer chain.
   *
   * @param config_path SensESP configuration path for the heartbeat settings.
   * @param default_sk_path Default Signal K path for the heartbeat value.
   * @param default_interval_s Default heartbeat interval in seconds.
   */
  HeartbeatReporter(const String& config_path, const String& default_sk_path,
                    float default_interval_s = 10.0f);

  /**
   * @brief Publishes the current uptime immediately. Call this when the
   * Signal K websocket connects or reconnects.
   */
  void onSKServerConnect();

  bool to_json(JsonObject& root) override;
  bool from_json(const JsonObject& config) override;

 private:
  String sk_path_;
  float heartbeat_interval_s_;
  sensesp::ObservableValue<float>* heartbeat_value_ = nullptr;
};

const String ConfigSchema(const HeartbeatReporter& obj);

inline bool ConfigRequiresRestart(const HeartbeatReporter& obj) {
  return true;
}

}  // namespace app
