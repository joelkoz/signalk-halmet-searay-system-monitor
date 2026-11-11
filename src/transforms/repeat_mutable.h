#ifndef APP_REPEAT_MUTABLE_H_
#define APP_REPEAT_MUTABLE_H_

#include "sensesp/transforms/transform.h"
#include "sensesp_base_app.h"

namespace app {

/**
 * @brief Repeat transform whose interval can be updated at runtime.
 *
 * RepeatMutable behaves like a repeat/report transform that forwards changed
 * input values immediately and then re-emits the most recent value on a repeat
 * timer. Unlike the fixed SensESP Repeat transform, its interval can be
 * changed after construction, which makes it useful for UI-configurable repeat
 * reporting.
 *
 * @tparam T Value type repeated by the transform.
 */
template <typename T>
class RepeatMutable : public sensesp::Transform<T, T> {
 public:
  /**
   * @brief Constructs a repeat transform with an initial interval.
   *
   * @param interval_ms Repeat interval in milliseconds.
   */
  explicit RepeatMutable(unsigned long interval_ms)
      : sensesp::Transform<T, T>(""), interval_ms_(interval_ms) {}

  ~RepeatMutable() { remove_repeat(); }

  /**
   * @brief Stores a new value and emits it if it changed.
   *
   * The repeat timer is also started or refreshed so the stored value will be
   * re-emitted on the configured cadence.
   *
   * @param input Latest input value to store and potentially emit.
   */
  void set(const T& input) override {
    const bool changed = !has_value_ || this->output_ != input;
    this->output_ = input;
    has_value_ = true;
    if (changed) {
      this->notify();
      schedule_repeat();
    } else if (repeat_event_ == nullptr) {
      schedule_repeat();
    }
  }

  /**
   * @brief Updates the repeat interval for future re-emissions.
   *
   * @param interval_ms New repeat interval in milliseconds.
   */
  void set_interval(unsigned long interval_ms) {
    interval_ms_ = interval_ms;
    schedule_repeat();
  }

  /**
   * @brief Returns the current repeat interval in milliseconds.
   */
  unsigned long interval_ms() const { return interval_ms_; }

 private:
  void remove_repeat() {
    if (repeat_event_ != nullptr) {
      repeat_event_->remove(sensesp::event_loop());
      repeat_event_ = nullptr;
    }
  }

  void schedule_repeat() {
    remove_repeat();

    if (!has_value_ || interval_ms_ == 0) {
      return;
    }

    repeat_event_ =
        sensesp::event_loop()->onRepeat(interval_ms_,
                                        [this]() { this->notify(); });
  }

  unsigned long interval_ms_;
  bool has_value_ = false;
  reactesp::RepeatEvent* repeat_event_ = nullptr;
};

}  // namespace app

#endif  // APP_REPEAT_MUTABLE_H_
