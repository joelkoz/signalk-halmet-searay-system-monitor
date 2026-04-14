#pragma once

#include <Arduino.h>
#include <time.h>

#include "signalk/sk_time_listener.h"
#include "sensesp/system/saveable.h"

namespace app {

/**
 * @brief Complex component that applies Signal K time updates to the system clock.
 *
 * This component owns an `SKTimeListener`, wires its emitted epoch values to a
 * lambda consumer, and updates the ESP32 wall clock via `settimeofday()`.
 * Once the clock is valid, code that relies on `time()`, `localtime_r()`, and
 * related functions can use ship-local time after the runtime timezone has
 * been configured.
 */
class SignalKTimeSync : public sensesp::FileSystemSaveable {
 public:
  /**
   * @brief Construct a Signal K time synchronization component.
   *
   * @param sk_path Signal K path that provides an ISO-8601 datetime string.
   * @param listen_delay_ms Requested minimum interval between listener updates.
   * @param config_path SensESP config path for the component.
   */
  SignalKTimeSync(const String& sk_path = "navigation.datetime",
                  int listen_delay_ms = 60000,
                  const String& config_path = "/system/signalk_time_sync");

  /**
   * @brief Start listening for Signal K datetime updates.
   */
  void begin();

  /**
   * @brief Access the underlying parsed time producer.
   *
   * This allows other transforms or consumers in `main.cpp` to subscribe to
   * the parsed epoch time in the future.
   *
   * @return SKTimeListener* Underlying parsed time listener.
   */
  SKTimeListener* input() const { return input_; }

  /**
   * @brief Serialize the component configuration.
   */
  bool to_json(JsonObject& root) override;

  /**
   * @brief Load the component configuration.
   */
  bool from_json(const JsonObject& config) override;

 private:
  /**
   * @brief Apply one parsed epoch value to the system wall clock.
   *
   * @param epoch_seconds Parsed UTC epoch received from the time listener.
   */
  void handle_time_update(time_t epoch_seconds);

  /// Signal K path that provides the UTC datetime string.
  String sk_path_;
  /// Requested minimum interval between Signal K time updates.
  int listen_delay_ms_;
  /// Underlying parsed time producer sourced from Signal K.
  SKTimeListener* input_ = nullptr;
  /// Tracks whether the lambda wiring has already been installed.
  bool started_ = false;
  /// Most recent epoch value successfully applied to the system clock.
  time_t last_synced_epoch_ = 0;
};

const String ConfigSchema(const SignalKTimeSync& obj);

inline bool ConfigRequiresRestart(const SignalKTimeSync& obj) { return true; }

}  // namespace app
