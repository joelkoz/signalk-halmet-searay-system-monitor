#pragma once

#include <Arduino.h>

#include "system/timed_producer.h"

namespace app {

/**
 * @brief Timed producer that emits the current device uptime in milliseconds.
 *
 * Timer is a convenience wrapper around TimedProducer<unsigned long> whose
 * callback returns millis(). It can be connected to transforms or outputs just
 * like any other SensESP ValueProducer.
 */
class Timer : public TimedProducer<unsigned long> {
 public:
  /**
   * @brief Constructs a timer producer.
   *
   * @param config_path Optional SensESP configuration path for interval_ms.
   * @param msDefaultInterval Default repeat interval in milliseconds.
   */
  explicit Timer(const String& config_path = "",
                 unsigned long msDefaultInterval = 1000);
};

}  // namespace app
