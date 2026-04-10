#include "sensors/analog_threshold_input.h"

#include "Debug.h"
#include "system/halmet_ads1115.h"
#include "system/halmet_const.h"

namespace app {

AnalogThresholdInput::AnalogThresholdInput(uint8_t channel,
                                           const String& input_name,
                                           unsigned int read_interval_ms,
                                           float active_threshold_v,
                                           float inactive_threshold_v,
                                           bool initial_state)
    : sensesp::BoolSensor(""),
      channel_(channel),
      input_name_(input_name),
      read_interval_ms_(read_interval_ms),
      active_threshold_v_(active_threshold_v),
      inactive_threshold_v_(inactive_threshold_v) {
  this->emit(initial_state);
  repeat_event_ = sensesp::event_loop()->onRepeat(
      read_interval_ms_, [this]() { this->sample(); });
  sensesp::event_loop()->onDelay(0, [this]() { this->sample(); });
}

AnalogThresholdInput::~AnalogThresholdInput() {
  if (repeat_event_ != nullptr) {
    repeat_event_->remove(sensesp::event_loop());
  }
}

void AnalogThresholdInput::sample() {
  if (channel_ > 3) {
    LOG_E("%s: invalid ADS1115 channel %u", input_name_.c_str(), channel_);
    return;
  }

  auto* ads = halmet::HalmetADS1115::instance();
  const int16_t raw = ads->readADC_SingleEnded(channel_);
  last_voltage_v_ = ads->computeVolts(raw) * halmet::kVoltageDividerScale;

  if (last_voltage_v_ > active_threshold_v_) {
    in_deadband_ = false;
    if (!this->get()) {
      this->emit(true);
    }
  } else if (last_voltage_v_ < inactive_threshold_v_) {
    in_deadband_ = false;
    if (this->get()) {
      this->emit(false);
    }
  } else if (!in_deadband_) {
    in_deadband_ = true;
    LOG_W("%s: analog input %.2f V is indeterminate; holding %s",
          input_name_.c_str(), last_voltage_v_,
          this->get() ? "active" : "inactive");
  }
}

}  // namespace app
