#pragma once

#include <Arduino.h>

#include <array>
#include <time.h>

#include "sensesp/system/observablevalue.h"
#include "sensesp/system/saveable.h"
#include "sensesp/system/valueproducer.h"
#include "transforms/repeat_mutable.h"

namespace sensesp {
class SKMetadata;
}  // namespace sensesp

namespace app {

enum class BilgePumpKind { kPrimary, kEmergency };
enum class PumpIncidentAlertId {
  kEmergencyRunning = 0,
  kMaxRunTime = 1,
  kShortCycles = 2,
  kCount
};

class BilgePumpMonitor : public sensesp::FileSystemSaveable {
 public:
  BilgePumpMonitor(const String& pump_role, const String& pump_name,
                   const String& config_path,
                   sensesp::BoolProducer* raw_run_input,
                   BilgePumpKind pump_kind);

  void begin(float state_report_interval_seconds,
             float aggregate_report_interval_seconds);
  void log_state_debug() const;
  void republish_outputs();
  void onSKServerConnect();

  const String& pump_name() const { return pump_name_; }
  const String& pump_role() const { return pump_role_; }
  bool is_emergency_pump() const {
    return pump_kind_ == BilgePumpKind::kEmergency;
  }

  sensesp::BoolProducer* raw_run_input() const { return raw_run_input_; }
  sensesp::BoolProducer* running_producer() const { return running_state_; }
  sensesp::FloatProducer* last_run_time_producer() const {
    return last_run_time_;
  }
  sensesp::FloatProducer* last_idle_time_producer() const {
    return last_idle_time_;
  }
  sensesp::IntProducer* daily_cycle_count_producer() const {
    return daily_cycle_count_;
  }
  sensesp::IntProducer* total_cycle_count_producer() const {
    return total_cycle_count_;
  }
  sensesp::IntProducer* short_cycle_count_producer() const {
    return short_cycle_count_;
  }
  sensesp::FloatProducer* average_run_time_producer() const {
    return average_run_time_;
  }
  sensesp::FloatProducer* average_idle_time_producer() const {
    return average_idle_time_;
  }
  sensesp::StringProducer* health_notification_producer() const {
    return health_notification_;
  }

  bool to_json(JsonObject& root) override;
  bool from_json(const JsonObject& config) override;

 private:
  void initialize_paths();
  String make_pump_path(const String& suffix) const;
  String make_notification_path() const;

  unsigned long max_run_time_ms() const;
  unsigned long min_stop_time_ms() const;
  unsigned long min_idle_time_ms() const;
  int primary_stale_warning_seconds() const;
  int emergency_exercise_warning_seconds() const;
  void update_daily_cycle_boundary(time_t now_seconds);
  void update_health_reference_epoch(time_t now_seconds);
  int day_key_from_time(time_t now_seconds) const;
  bool valid_wall_clock(time_t now_seconds) const;

  String make_notification_payload(const char* state, const String& message,
                                   bool audible) const;
  String format_alert_timestamp(time_t epoch_seconds) const;
  String make_incident_alert_path(const char* suffix) const;
  sensesp::SKMetadata* make_running_metadata() const;
  sensesp::SKMetadata* make_last_run_time_metadata() const;
  sensesp::SKMetadata* make_last_idle_time_metadata() const;
  sensesp::SKMetadata* make_daily_cycle_count_metadata() const;
  sensesp::SKMetadata* make_total_cycle_count_metadata() const;
  sensesp::SKMetadata* make_short_cycle_count_metadata() const;
  sensesp::SKMetadata* make_average_run_time_metadata() const;
  sensesp::SKMetadata* make_average_idle_time_metadata() const;
  sensesp::SKMetadata* make_health_notification_metadata() const;
  sensesp::SKMetadata* make_incident_alert_metadata(const String& title) const;

  void set_health_notification(const char* state, const String& message,
                               bool audible);
  void update_incident_alerts(time_t now_seconds);
  void update_incident_alert(PumpIncidentAlertId alert_id, bool active,
                             const char* state, const String& reason,
                             bool audible, time_t now_seconds);
  void emit_incident_alert(PumpIncidentAlertId alert_id, const char* state,
                           const String& reason, bool audible,
                           time_t now_seconds);
  void emit_incident_alert_resolution(PumpIncidentAlertId alert_id);
  void evaluate();
  void handle_running_start(unsigned long now_ms, time_t now_seconds);
  void handle_running_stop(unsigned long now_ms, time_t now_seconds);
  void update_short_cycle_window(unsigned long now_ms);
  void update_health_notification(time_t now_seconds);

  String pump_role_;
  String pump_name_;
  sensesp::BoolProducer* raw_run_input_;
  BilgePumpKind pump_kind_;
  bool started_ = false;

  float max_run_time_s_;
  float min_stop_time_s_;
  float min_idle_time_s_;
  int max_consecutive_short_cycles_;
  float primary_stale_warning_days_;
  float emergency_exercise_warning_days_;

  String running_sk_path_;
  String last_run_time_sk_path_;
  String last_idle_time_sk_path_;
  String daily_cycle_count_sk_path_;
  String total_cycle_count_sk_path_;
  String short_cycle_count_sk_path_;
  String average_run_time_sk_path_;
  String average_idle_time_sk_path_;
  String health_notification_sk_path_;

  bool raw_running_ = false;
  bool logical_running_ = false;
  bool idle_timer_active_ = false;
  unsigned long cycle_start_ms_ = 0;
  unsigned long stop_candidate_ms_ = 0;
  unsigned long idle_start_ms_ = 0;
  unsigned long long cumulative_run_time_ms_ = 0;
  unsigned long long cumulative_idle_time_ms_ = 0;
  unsigned long completed_cycle_count_ = 0;
  unsigned long idle_sample_count_ = 0;
  String active_health_state_;
  String active_health_message_;

  struct IncidentAlertChannel {
    const char* suffix = "";
    const char* title = "";
    String sk_path;
    sensesp::ObservableValue<String>* notification = nullptr;
    bool active = false;
    String active_state;
    String active_reason;
    time_t first_epoch = 0;
  };

  std::array<IncidentAlertChannel,
             static_cast<size_t>(PumpIncidentAlertId::kCount)>
      incident_alerts_;

  sensesp::ObservableValue<bool>* running_state_ = nullptr;
  sensesp::ObservableValue<float>* last_run_time_ = nullptr;
  sensesp::ObservableValue<float>* last_idle_time_ = nullptr;
  sensesp::PersistingObservableValue<int>* daily_cycle_count_ = nullptr;
  sensesp::PersistingObservableValue<int>* total_cycle_count_ = nullptr;
  sensesp::ObservableValue<int>* short_cycle_count_ = nullptr;
  sensesp::ObservableValue<float>* average_run_time_ = nullptr;
  sensesp::ObservableValue<float>* average_idle_time_ = nullptr;
  sensesp::PersistingObservableValue<int>* daily_stats_day_key_ = nullptr;
  sensesp::PersistingObservableValue<int>* last_run_epoch_ = nullptr;
  sensesp::PersistingObservableValue<int>* health_reference_epoch_ = nullptr;
  sensesp::ObservableValue<String>* health_notification_ = nullptr;

  reactesp::RepeatEvent* evaluation_event_ = nullptr;
};

const String ConfigSchema(const BilgePumpMonitor& obj);

inline bool ConfigRequiresRestart(const BilgePumpMonitor& obj) { return true; }

}  // namespace app
