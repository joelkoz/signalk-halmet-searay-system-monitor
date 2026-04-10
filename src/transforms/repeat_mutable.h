#ifndef APP_REPEAT_MUTABLE_H_
#define APP_REPEAT_MUTABLE_H_

#include "sensesp/transforms/transform.h"
#include "sensesp_base_app.h"

namespace app {

template <typename T>
class RepeatMutable : public sensesp::Transform<T, T> {
 public:
  explicit RepeatMutable(unsigned long interval_ms)
      : sensesp::Transform<T, T>(""), interval_ms_(interval_ms) {}

  ~RepeatMutable() { remove_repeat(); }

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

  void set_interval(unsigned long interval_ms) {
    interval_ms_ = interval_ms;
    schedule_repeat();
  }

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
