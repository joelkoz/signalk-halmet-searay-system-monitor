#ifndef APP_REPEAT_MUTABLE_H_
#define APP_REPEAT_MUTABLE_H_

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

#include <atomic>

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
 * Its repeat timer and outputs belong to the task that constructed it (the
 * main loop). A set() from any other task, such as the web server applying a
 * config save, is handed to the main loop instead of running there.
 * Instances must outlive the event loop (a handed-over set() holds `this`).
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
      : sensesp::Transform<T, T>(""),
        interval_ms_(interval_ms),
        owner_task_(xTaskGetCurrentTaskHandle()) {}

  ~RepeatMutable() { remove_repeat(); }

  /**
   * @brief Stores a new value and emits it if it changed.
   *
   * The repeat timer is also started or refreshed so the stored value will be
   * re-emitted on the configured cadence. When called from a task other than
   * the owner, the value is applied on the main loop instead; a newer set()
   * made before it runs wins.
   *
   * @param input Latest input value to store and potentially emit.
   */
  void set(const T& input) override {
    const uint32_t version = ++version_;
    if (xTaskGetCurrentTaskHandle() != owner_task_) {
      sensesp::event_loop()->onDelay(0, [this, input, version]() {
        if (version_ == version) {
          apply(input);
        }
      });
      return;
    }
    apply(input);
  }

  /**
   * @brief Re-emits the stored value now and restarts the repeat timer.
   *
   * Use this when the Signal K connection is (re)established: set() drops an
   * unchanged value, so notifying the upstream producer would not reach the
   * output. If no value has arrived yet, @p source's current value is taken
   * as the first value. Only this transform's consumers see it; the source's
   * other consumers (relays, for example) are not re-driven.
   *
   * Call it only from the main loop.
   *
   * @param source The producer this transform is connected to, or nullptr.
   */
  void resend(sensesp::ValueProducer<T>* source) {
    if (has_value_) {
      this->notify();
      schedule_repeat();
    } else if (source != nullptr) {
      set(source->get());
    }
  }

  /**
   * @brief Updates the repeat interval for future re-emissions.
   *
   * Call it only from the main loop.
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
  void apply(const T& input) {
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
  const TaskHandle_t owner_task_;
  std::atomic<uint32_t> version_{0};
  bool has_value_ = false;
  reactesp::RepeatEvent* repeat_event_ = nullptr;
};

}  // namespace app

#endif  // APP_REPEAT_MUTABLE_H_
