#pragma once

#include <Arduino.h>
#include <time.h>

#include "sensesp/signalk/signalk_value_listener.h"
#include "sensesp/system/saveable.h"
#include "sensesp/system/valueproducer.h"

namespace app {

/**
 * @brief Signal K listener that parses an ISO-8601 datetime string to epoch.
 *
 * This class owns a raw `StringSKListener`, parses a Signal K datetime path
 * such as `navigation.datetime`, and emits Unix epoch seconds whenever a valid
 * datetime value is received.
 */
class SKTimeListener : public sensesp::FileSystemSaveable,
                       public sensesp::ValueProducer<time_t> {
 public:
  /**
   * @brief Construct a time listener for a Signal K datetime path.
   *
   * @param sk_path Signal K path that provides an ISO-8601 datetime string.
   * @param listen_delay_ms Requested minimum interval between listener updates.
   * @param config_path SensESP config path for this listener.
   */
  SKTimeListener(const String& sk_path = "navigation.datetime",
                 int listen_delay_ms = 60000,
                 const String& config_path = "/system/signalk_time_listener");

  /**
   * @brief Start the underlying raw Signal K listener.
   */
  void begin();

  /**
   * @brief Serialize this listener's configuration.
   *
   * @param root JSON object to populate.
   * @return true if serialization succeeded.
   */
  bool to_json(JsonObject& root) override;

  /**
   * @brief Load this listener's configuration from JSON.
   *
   * @param config JSON object containing configuration.
   * @return true if parsing succeeded.
   */
  bool from_json(const JsonObject& config) override;

  /**
   * @brief Return the configured Signal K datetime path.
   */
  const String& sk_path() const { return sk_path_; }

  /**
   * @brief Return the configured listen delay in milliseconds.
   */
  int listen_delay_ms() const { return listen_delay_ms_; }

 private:
  /**
   * @brief Process one incoming Signal K datetime string and emit epoch time.
   *
   * @param datetime_string ISO-8601 datetime string from Signal K.
   */
  void handle_datetime_string(const String& datetime_string);

  /**
   * @brief Parse an ISO-8601 datetime string into Unix epoch seconds.
   *
   * @param datetime_string ISO-8601 datetime string from Signal K.
   * @param epoch_seconds Parsed output epoch in UTC seconds.
   * @return true if parsing succeeded.
   * @return false if parsing failed.
   */
  bool parse_datetime_to_epoch(const String& datetime_string,
                               time_t* epoch_seconds) const;

  /// Signal K path that provides the UTC datetime string.
  String sk_path_;
  /// Requested minimum interval between raw listener updates.
  int listen_delay_ms_;
  /// Underlying raw string listener from SensESP.
  sensesp::StringSKListener* raw_listener_ = nullptr;
  /// Tracks whether the raw listener wiring has already been installed.
  bool started_ = false;
};

const String ConfigSchema(const SKTimeListener& obj);

inline bool ConfigRequiresRestart(const SKTimeListener& obj) { return true; }

}  // namespace app
