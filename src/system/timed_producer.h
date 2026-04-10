#pragma once

#include <Arduino.h>

#include <functional>

#include "sensesp/system/saveable.h"
#include "sensesp/system/valueproducer.h"
#include "sensesp_base_app.h"

namespace app {

/**
 * @brief Produces a value by calling a lambda function at a specific interval.
 *
 * TimedProducer calls the provided labmda function on each interval tick and 
 * emits the returned value. The repeat interval is stored in milliseconds and
 * can optionally be user configured and loaded from a SensESP filesystem 
 * configuration path.
 *
 * @tparam T Type produced by the callback function.
 */
template <typename T>
class TimedProducer : public sensesp::ValueProducer<T>,
                      public sensesp::FileSystemSaveable {
 public:
  /**
   * @brief Constructs and starts a timed producer.
   *
   * @param fn Function called on each interval to produce the next value.
   * @param config_path Optional SensESP configuration path for interval_ms.
   * @param msDefaultInterval Default repeat interval in milliseconds.
   */
  TimedProducer(std::function<T()> fn, const String& config_path = "",
                unsigned long msDefaultInterval = 1000)
      : sensesp::FileSystemSaveable(config_path),
        fn_(fn),
        interval_ms_(msDefaultInterval) {
    this->load();
    if (interval_ms_ == 0) {
      interval_ms_ = 1;
    }
    repeat_event_ = sensesp::event_loop()->onRepeat(interval_ms_, [this]() {
      this->emit(fn_());
    });
  }

  ~TimedProducer() {
    if (repeat_event_ != nullptr) {
      repeat_event_->remove(sensesp::event_loop());
    }
  }

  /**
   * @brief Returns the active repeat interval in milliseconds.
   */
  unsigned long interval_ms() const { return interval_ms_; }

  bool to_json(JsonObject& root) override {
    root["interval_ms"] = interval_ms_;
    return true;
  }

  bool from_json(const JsonObject& config) override {
    if (config["interval_ms"].is<unsigned long>()) {
      interval_ms_ = config["interval_ms"].as<unsigned long>();
    }
    if (interval_ms_ == 0) {
      interval_ms_ = 1;
    }
    return true;
  }

 private:
  std::function<T()> fn_;
  unsigned long interval_ms_;
  reactesp::RepeatEvent* repeat_event_ = nullptr;
};

template <typename T>
const String ConfigSchema(const TimedProducer<T>& obj) {
  return R"json({"type":"object","properties":{"interval_ms":{"title":"Repeat Interval (ms)","description":"How often this timed producer emits a new value, in milliseconds. Changes require a device restart.","type":"integer"}}})json";
}

template <typename T>
inline bool ConfigRequiresRestart(const TimedProducer<T>& obj) {
  return true;
}

}  // namespace app
