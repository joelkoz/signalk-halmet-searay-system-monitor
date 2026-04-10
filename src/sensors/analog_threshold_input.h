#pragma once

#include <Arduino.h>

#include "ReactESP.h"
#include "sensesp/sensors/sensor.h"

namespace app {

class AnalogThresholdInput : public sensesp::BoolSensor {
 public:
  AnalogThresholdInput(uint8_t channel, const String& input_name,
                       unsigned int read_interval_ms = 100,
                       float active_threshold_v = 8.0f,
                       float inactive_threshold_v = 2.0f,
                       bool initial_state = false);
  ~AnalogThresholdInput();

  float last_voltage() const { return last_voltage_v_; }
  uint8_t channel() const { return channel_; }

  void sample();

 private:
  uint8_t channel_;
  String input_name_;
  unsigned int read_interval_ms_;
  float active_threshold_v_;
  float inactive_threshold_v_;
  float last_voltage_v_ = 0.0f;
  bool in_deadband_ = false;
  reactesp::RepeatEvent* repeat_event_ = nullptr;
};

}  // namespace app
