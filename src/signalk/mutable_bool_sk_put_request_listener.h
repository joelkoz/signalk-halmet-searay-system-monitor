#pragma once

#include <Arduino.h>

#include "sensesp/signalk/signalk_put_request_listener.h"

namespace app {

/**
 * @brief Writable bool PUT listener with mutable path and tolerant parsing.
 *
 * This app-specific extension of SensESP's BoolSKPutRequestListener allows the
 * Signal K path to be updated after construction and accepts both JSON boolean
 * values and integer-style `0` / `1` inputs when parsing PUT requests.
 */
class MutableBoolSKPutRequestListener
    : public sensesp::BoolSKPutRequestListener {
 public:
  /**
   * @brief Constructs a bool PUT listener for a Signal K path.
   *
   * @param sk_path Signal K path this listener should accept PUT requests for.
   */
  explicit MutableBoolSKPutRequestListener(const String& sk_path);

  /**
   * @brief Updates the Signal K path this listener matches.
   *
   * @param sk_path New writable Signal K path.
   */
  void set_sk_path(const String& sk_path);

  /**
   * @brief Parses a Signal K PUT value into a bool and forwards it downstream.
   *
   * @param put Signal K PUT object containing the requested value.
   */
  void parse_value(const JsonObject& put) override;
};

}  // namespace app
