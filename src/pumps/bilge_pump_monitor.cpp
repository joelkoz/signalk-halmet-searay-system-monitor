#include "pumps/bilge_pump_monitor.h"

#include <ArduinoJson.h>

#include <cmath>

#include "Debug.h"
#include "sensesp/signalk/signalk_output.h"
#include "sensesp/signalk/signalk_prefix_listener.h"
#include "sensesp/system/lambda_consumer.h"

namespace app {

namespace {

constexpr float kDefaultMaxRunTimeSeconds = 120.0f;
constexpr float kDefaultMinStopTimeSeconds = 3.0f;
constexpr float kDefaultMinIdleTimeSeconds = 300.0f;
constexpr int kDefaultMaxConsecutiveShortCycles = 5;
constexpr float kDefaultPrimaryStaleWarningDays = 30.0f;
constexpr float kDefaultEmergencyExerciseWarningDays = 60.0f;
constexpr float kDefaultEmergencyAlarmDelaySeconds = 3.0f;
constexpr float kMaxEmergencyAlarmDelaySeconds = 30.0f;
// A short emergency run within this time of the previous run is not a test:
// a pump making repeated short runs is keeping up with water. Real tests are
// weeks apart (see the exercise warning), so a long window costs little.
constexpr int kEmergencyTestRunWindowSeconds = 24 * 60 * 60;
constexpr unsigned long kEmergencyTestRunWindowMs =
    static_cast<unsigned long>(kEmergencyTestRunWindowSeconds) * 1000UL;

constexpr unsigned long kMonitorEvaluationIntervalMs = 200;
constexpr float kMinReportIntervalSeconds = 0.1f;
constexpr float kMinThresholdSeconds = 0.0f;
constexpr time_t kValidWallClockEpoch = 1704067200;  // 2024-01-01T00:00:00Z

float clamp_min_float(float value, float minimum) {
  return value < minimum ? minimum : value;
}

int clamp_min_int(int value, int minimum) {
  return value < minimum ? minimum : value;
}

unsigned long interval_ms_from_seconds(float seconds) {
  return static_cast<unsigned long>(
      clamp_min_float(seconds, kMinReportIntervalSeconds) * 1000.0f);
}

}  // namespace

BilgePumpMonitor::BilgePumpMonitor(const String& pump_role,
                                   const String& pump_name,
                                   const String& config_path,
                                   sensesp::BoolProducer* raw_run_input,
                                   BilgePumpKind pump_kind)
    : FileSystemSaveable(config_path),
      pump_role_(pump_role),
      pump_name_(pump_name),
      raw_run_input_(raw_run_input),
      pump_kind_(pump_kind),
      max_run_time_s_(kDefaultMaxRunTimeSeconds),
      min_stop_time_s_(kDefaultMinStopTimeSeconds),
      min_idle_time_s_(kDefaultMinIdleTimeSeconds),
      max_consecutive_short_cycles_(kDefaultMaxConsecutiveShortCycles),
      primary_stale_warning_days_(kDefaultPrimaryStaleWarningDays),
      emergency_exercise_warning_days_(kDefaultEmergencyExerciseWarningDays),
      emergency_alarm_delay_s_(kDefaultEmergencyAlarmDelaySeconds) {
  incident_alerts_[static_cast<size_t>(PumpIncidentAlertId::kEmergencyRunning)]
      .suffix = "emergencyRunning";
  incident_alerts_[static_cast<size_t>(PumpIncidentAlertId::kMaxRunTime)]
      .suffix = "maxRunTime";
  incident_alerts_[static_cast<size_t>(PumpIncidentAlertId::kShortCycles)]
      .suffix = "shortCycles";
  initialize_paths();
}

void BilgePumpMonitor::begin(float state_report_interval_seconds,
                             float aggregate_report_interval_seconds) {
  if (started_) {
    return;
  }

  running_state_ = new sensesp::ObservableValue<bool>(false);
  last_run_time_ = new sensesp::ObservableValue<float>(0.0f);
  last_idle_time_ = new sensesp::ObservableValue<float>(0.0f);
  daily_cycle_count_ = new sensesp::PersistingObservableValue<int>(
      0, get_config_path() + "/stats/daily_cycle_count");
  total_cycle_count_ = new sensesp::PersistingObservableValue<int>(
      0, get_config_path() + "/stats/total_cycle_count");
  short_cycle_count_ = new sensesp::ObservableValue<int>(0);
  average_run_time_ = new sensesp::ObservableValue<float>(0.0f);
  average_idle_time_ = new sensesp::ObservableValue<float>(0.0f);
  daily_stats_day_key_ = new sensesp::PersistingObservableValue<int>(
      -1, get_config_path() + "/stats/daily_stats_day_key");
  last_run_epoch_ = new sensesp::PersistingObservableValue<int>(
      0, get_config_path() + "/stats/last_run_epoch");
  health_reference_epoch_ = new sensesp::PersistingObservableValue<int>(
      0, get_config_path() + "/stats/health_reference_epoch");
  health_notification_ = new sensesp::ObservableValue<String>(
      make_notification_payload("normal", pump_name_ + ": Pump health is normal",
                                false));

  load();

  const unsigned long state_interval_ms =
      interval_ms_from_seconds(state_report_interval_seconds);
  const unsigned long aggregate_interval_ms =
      interval_ms_from_seconds(aggregate_report_interval_seconds);

  running_repeat_ = new RepeatMutable<bool>(state_interval_ms);
  running_state_->connect_to(running_repeat_)
      ->connect_to(new sensesp::SKOutputBool(running_sk_path_, "",
                                             make_running_metadata()));

  last_run_time_->connect_to(new sensesp::SKOutputFloat(
      last_run_time_sk_path_, "", make_last_run_time_metadata()));
  last_idle_time_->connect_to(new sensesp::SKOutputFloat(
      last_idle_time_sk_path_, "", make_last_idle_time_metadata()));

  daily_cycle_count_repeat_ = new RepeatMutable<int>(aggregate_interval_ms);
  daily_cycle_count_->connect_to(daily_cycle_count_repeat_)
      ->connect_to(new sensesp::SKOutputInt(
          daily_cycle_count_sk_path_, "", make_daily_cycle_count_metadata()));

  total_cycle_count_repeat_ = new RepeatMutable<int>(aggregate_interval_ms);
  total_cycle_count_->connect_to(total_cycle_count_repeat_)
      ->connect_to(new sensesp::SKOutputInt(
          total_cycle_count_sk_path_, "", make_total_cycle_count_metadata()));

  short_cycle_count_repeat_ = new RepeatMutable<int>(aggregate_interval_ms);
  short_cycle_count_->connect_to(short_cycle_count_repeat_)
      ->connect_to(new sensesp::SKOutputInt(
          short_cycle_count_sk_path_, "", make_short_cycle_count_metadata()));

  average_run_time_repeat_ = new RepeatMutable<float>(aggregate_interval_ms);
  average_run_time_->connect_to(average_run_time_repeat_)
      ->connect_to(new sensesp::SKOutputFloat(
          average_run_time_sk_path_, "", make_average_run_time_metadata()));

  average_idle_time_repeat_ = new RepeatMutable<float>(aggregate_interval_ms);
  average_idle_time_->connect_to(average_idle_time_repeat_)
      ->connect_to(new sensesp::SKOutputFloat(
          average_idle_time_sk_path_, "", make_average_idle_time_metadata()));

  // Notification paths carry no Signal K metadata. SensESP sends the metadata
  // of every output in one delta on each connect, and with all five pumps the
  // full set exceeded the websocket send buffer: it was dropped on every
  // connect, and building it nearly exhausted the heap. Value paths keep their
  // units and display names; the descriptions were dropped for the same
  // reason.
  health_notification_repeat_ =
      new RepeatMutable<String>(aggregate_interval_ms);
  health_notification_->connect_to(health_notification_repeat_)
      ->connect_to(new sensesp::SKOutputRawJson(health_notification_sk_path_,
                                                ""));

  for (auto& incident_alert : incident_alerts_) {
    incident_alert.notification = new sensesp::ObservableValue<String>("");
    incident_alert.notification->connect_to(
        new sensesp::SKOutputRawJson(incident_alert.sk_path, ""));
  }

  // A resolved incident stays a warning until someone acknowledges it in
  // Signal K. The server reports that in the notification's status, so watch
  // this pump's incident alert paths. On every (re)connect the server also
  // sends their current values, which lets an alert left over from before a
  // reboot be cleared once acknowledged.
  incident_alert_prefix_ = health_notification_sk_path_ + ".alert.";
  auto* incident_alert_listener =
      new sensesp::SKPrefixListener(incident_alert_prefix_);
  incident_alert_listener->connect_to(
      new sensesp::LambdaConsumer<sensesp::SKPathValue>(
          [this](const sensesp::SKPathValue& update) {
            handle_incident_alert_update(update);
          }));

  if (raw_run_input_ != nullptr) {
    raw_running_ = raw_run_input_->get();
    raw_run_input_->connect_to(new sensesp::LambdaConsumer<bool>(
        [this](bool running) { this->raw_running_ = running; }));
  }

  const unsigned long now_ms = millis();
  const time_t now_seconds = time(nullptr);
  update_daily_cycle_boundary(now_seconds);
  update_health_reference_epoch(now_seconds);

  logical_running_ = raw_running_;
  cycle_start_ms_ = logical_running_ ? now_ms : 0;
  idle_timer_active_ = !logical_running_;
  idle_start_ms_ = logical_running_ ? 0 : now_ms;
  stop_candidate_ms_ = 0;

  if (logical_running_ && valid_wall_clock(now_seconds)) {
    *last_run_epoch_ = static_cast<int>(now_seconds);
  }

  started_ = true;
  *running_state_ = logical_running_;
  update_health_notification(now_seconds);

  evaluation_event_ = sensesp::event_loop()->onRepeat(
      kMonitorEvaluationIntervalMs, [this]() { this->evaluate(); });

  LOG_I("%s: bilge pump monitor started role=%s kind=%s",
        pump_name_.c_str(), pump_role_.c_str(),
        is_emergency_pump() ? "emergency" : "primary");
}

void BilgePumpMonitor::initialize_paths() {
  running_sk_path_ = make_pump_path("running");
  last_run_time_sk_path_ = make_pump_path("lastCycleRunTime");
  last_idle_time_sk_path_ = make_pump_path("lastCycleIdleTime");
  daily_cycle_count_sk_path_ = make_pump_path("dailyCycleCount");
  total_cycle_count_sk_path_ = make_pump_path("totalCycleCount");
  short_cycle_count_sk_path_ = make_pump_path("shortCycleCount");
  average_run_time_sk_path_ = make_pump_path("averageCycleRunTime");
  average_idle_time_sk_path_ = make_pump_path("averageCycleIdleTime");
  health_notification_sk_path_ = make_notification_path();
  for (auto& incident_alert : incident_alerts_) {
    incident_alert.sk_path = make_incident_alert_path(incident_alert.suffix);
  }
}

String BilgePumpMonitor::make_pump_path(const String& suffix) const {
  return "electrical.pumps.bilge." + pump_role_ + "." + suffix;
}

String BilgePumpMonitor::make_notification_path() const {
  return "notifications.bilge.pumps." + pump_role_ + ".health";
}

String BilgePumpMonitor::make_incident_alert_path(const char* suffix) const {
  return health_notification_sk_path_ + ".alert." + suffix;
}

bool BilgePumpMonitor::valid_wall_clock(time_t now_seconds) const {
  return now_seconds >= kValidWallClockEpoch;
}

int BilgePumpMonitor::day_key_from_time(time_t now_seconds) const {
  if (!valid_wall_clock(now_seconds)) {
    return -1;
  }

  struct tm local_time {};
  if (localtime_r(&now_seconds, &local_time) == nullptr) {
    return -1;
  }

  return (local_time.tm_year + 1900) * 1000 + local_time.tm_yday;
}

void BilgePumpMonitor::update_daily_cycle_boundary(time_t now_seconds) {
  const int day_key = day_key_from_time(now_seconds);
  if (day_key < 0 || daily_stats_day_key_ == nullptr) {
    return;
  }

  if (daily_stats_day_key_->get() < 0) {
    *daily_stats_day_key_ = day_key;
    return;
  }

  if (day_key != daily_stats_day_key_->get()) {
    *daily_stats_day_key_ = day_key;
    *daily_cycle_count_ = 0;
    LOG_I("%s: reset daily cycle count at midnight boundary",
          pump_name_.c_str());
  }
}

void BilgePumpMonitor::update_health_reference_epoch(time_t now_seconds) {
  if (!valid_wall_clock(now_seconds) || health_reference_epoch_ == nullptr) {
    return;
  }

  if (health_reference_epoch_->get() <= 0) {
    *health_reference_epoch_ = static_cast<int>(now_seconds);
  }
}

String BilgePumpMonitor::make_notification_payload(const char* state,
                                                   const String& message,
                                                   bool audible) const {
  JsonDocument doc;
  JsonObject root = doc.to<JsonObject>();
  root["state"] = state;
  root["message"] = message;
  if (strcmp(state, "normal") != 0) {
    JsonArray method = root["method"].to<JsonArray>();
    method.add("visual");
    if (audible) {
      method.add("sound");
    }
  }

  String payload;
  serializeJson(root, payload);
  return payload;
}

String BilgePumpMonitor::format_alert_timestamp(time_t epoch_seconds) const {
  if (!valid_wall_clock(epoch_seconds)) {
    return "unknown time";
  }

  struct tm local_time {};
  if (localtime_r(&epoch_seconds, &local_time) == nullptr) {
    return "unknown time";
  }

  char buffer[20];
  strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M", &local_time);
  return String(buffer);
}

sensesp::SKMetadata* BilgePumpMonitor::make_running_metadata() const {
  return new sensesp::SKMetadata(
      "", pump_name_ + " Running");
}

sensesp::SKMetadata* BilgePumpMonitor::make_last_run_time_metadata() const {
  return new sensesp::SKMetadata(
      "s", pump_name_ + " Last Cycle Run Time");
}

sensesp::SKMetadata* BilgePumpMonitor::make_last_idle_time_metadata() const {
  return new sensesp::SKMetadata(
      "s", pump_name_ + " Last Cycle Idle Time");
}

sensesp::SKMetadata* BilgePumpMonitor::make_daily_cycle_count_metadata() const {
  return new sensesp::SKMetadata(
      "count", pump_name_ + " Daily Cycle Count");
}

sensesp::SKMetadata* BilgePumpMonitor::make_total_cycle_count_metadata() const {
  return new sensesp::SKMetadata(
      "count", pump_name_ + " Total Cycle Count");
}

sensesp::SKMetadata* BilgePumpMonitor::make_short_cycle_count_metadata() const {
  return new sensesp::SKMetadata(
      "count", pump_name_ + " Short Cycle Count");
}

sensesp::SKMetadata* BilgePumpMonitor::make_average_run_time_metadata() const {
  return new sensesp::SKMetadata(
      "s", pump_name_ + " Average Cycle Run Time");
}

sensesp::SKMetadata* BilgePumpMonitor::make_average_idle_time_metadata() const {
  return new sensesp::SKMetadata(
      "s", pump_name_ + " Average Cycle Idle Time");
}

void BilgePumpMonitor::set_health_notification(const char* state,
                                               const String& message,
                                               bool audible) {
  if (active_health_state_ == state && active_health_message_ == message) {
    return;
  }

  active_health_state_ = state;
  active_health_message_ = message;
  // Name the pump: a notification forwarded on its own (e.g. a push message)
  // does not carry its path, and several pumps share each message.
  health_notification_->set(
      make_notification_payload(state, pump_name_ + ": " + message, audible));

  if (strcmp(state, "normal") == 0) {
    LOG_I("%s: pump health normal", pump_name_.c_str());
  } else {
    LOG_W("%s: pump health %s: %s", pump_name_.c_str(), state,
          message.c_str());
  }
}

void BilgePumpMonitor::update_incident_alert(PumpIncidentAlertId alert_id,
                                             bool active, const char* state,
                                             const String& reason,
                                             bool audible,
                                             time_t now_seconds) {
  if (active) {
    emit_incident_alert(alert_id, state, reason, audible, now_seconds);
  } else {
    emit_incident_alert_resolution(alert_id);
  }
}

void BilgePumpMonitor::handle_incident_alert_update(
    const sensesp::SKPathValue& update) {
  if (!update.path.startsWith(incident_alert_prefix_)) {
    return;
  }
  const char* suffix = update.path.c_str() + incident_alert_prefix_.length();

  for (auto& incident_alert : incident_alerts_) {
    if (strcmp(suffix, incident_alert.suffix) != 0) {
      continue;
    }
    // An active incident stays raised whatever its acknowledgement.
    if (incident_alert.active || incident_alert.notification == nullptr) {
      return;
    }
    // Only the "resolved" warning may be cleared. An acknowledged alarm seen
    // after a reboot can be an incident that is still going on but not yet
    // re-detected (the max run timer restarts, short cycles are not kept).
    // Since boot, also require this device's own resolved message, so a late
    // acknowledgement of an earlier resolution cannot clear a newer one.
    const char* state = update.value["state"] | "";
    const char* message = update.value["message"] | "";
    const bool acknowledged = update.value["status"]["acknowledged"] | false;
    const bool own_resolution =
        incident_alert.resolved_message.isEmpty() ||
        incident_alert.resolved_message == message;
    if (strcmp(state, "warn") == 0 && acknowledged && own_resolution) {
      incident_alert.notification->set(make_notification_payload(
          "normal", pump_name_ + " incident resolved and acknowledged",
          false));
      incident_alert.resolved_message = "";
      LOG_I("%s: acknowledged incident alert returned to normal on %s",
            pump_name_.c_str(), update.path.c_str());
    }
    return;
  }
}

void BilgePumpMonitor::emit_incident_alert(PumpIncidentAlertId alert_id,
                                           const char* state,
                                           const String& reason, bool audible,
                                           time_t now_seconds) {
  auto& incident_alert = incident_alerts_[static_cast<size_t>(alert_id)];
  if (incident_alert.notification == nullptr) {
    return;
  }

  if (incident_alert.active && incident_alert.active_state == state &&
      incident_alert.active_reason == reason) {
    return;
  }

  if (incident_alert.first_epoch == 0 && valid_wall_clock(now_seconds)) {
    incident_alert.first_epoch = now_seconds;
  }
  incident_alert.active = true;
  incident_alert.active_state = state;
  incident_alert.active_reason = reason;

  const String alert_message = pump_name_ + ": " + reason;
  incident_alert.notification->set(
      make_notification_payload(state, alert_message, audible));
  LOG_W("%s: incident alert emitted on %s: %s", pump_name_.c_str(),
        incident_alert.sk_path.c_str(), alert_message.c_str());
}

void BilgePumpMonitor::emit_incident_alert_resolution(
    PumpIncidentAlertId alert_id) {
  auto& incident_alert = incident_alerts_[static_cast<size_t>(alert_id)];
  if (incident_alert.notification == nullptr || !incident_alert.active) {
    return;
  }

  String message = pump_name_ + " incident";
  if (incident_alert.first_epoch > 0) {
    message += " from " + format_alert_timestamp(incident_alert.first_epoch);
  }
  message += " now resolved: " + incident_alert.active_reason;

  incident_alert.resolved_message = message;
  incident_alert.notification->set(
      make_notification_payload("warn", message, false));
  LOG_I("%s: incident alert resolved on %s: %s", pump_name_.c_str(),
        incident_alert.sk_path.c_str(), message.c_str());

  incident_alert.active = false;
  incident_alert.active_state = "";
  incident_alert.active_reason = "";
  incident_alert.first_epoch = 0;
}

unsigned long BilgePumpMonitor::max_run_time_ms() const {
  return static_cast<unsigned long>(max_run_time_s_ * 1000.0f);
}

unsigned long BilgePumpMonitor::min_stop_time_ms() const {
  return static_cast<unsigned long>(min_stop_time_s_ * 1000.0f);
}

unsigned long BilgePumpMonitor::min_idle_time_ms() const {
  return static_cast<unsigned long>(min_idle_time_s_ * 1000.0f);
}

int BilgePumpMonitor::primary_stale_warning_seconds() const {
  return static_cast<int>(primary_stale_warning_days_ * 86400.0f);
}

int BilgePumpMonitor::emergency_exercise_warning_seconds() const {
  return static_cast<int>(emergency_exercise_warning_days_ * 86400.0f);
}

unsigned long BilgePumpMonitor::emergency_alarm_delay_ms() const {
  return static_cast<unsigned long>(emergency_alarm_delay_s_ * 1000.0f);
}

// A run of an emergency pump shorter than the alarm delay is a test, not an
// emergency; it still counts as a cycle and as exercising the pump. Only an
// isolated run qualifies: one that starts within kEmergencyTestRunWindowSeconds
// of the previous run alarms at once. Time stops counting once the input
// goes inactive, so the min stop time spent confirming the stop cannot stretch
// a short test past the delay.
bool BilgePumpMonitor::emergency_alarm_due(unsigned long now_ms) const {
  if (!is_emergency_pump() || !logical_running_) {
    return false;
  }
  if (!emergency_test_run_allowed_) {
    return true;
  }
  const unsigned long run_end_ms =
      stop_candidate_ms_ != 0 ? stop_candidate_ms_ : now_ms;
  return run_end_ms - cycle_start_ms_ >= emergency_alarm_delay_ms();
}

void BilgePumpMonitor::handle_running_start(unsigned long now_ms,
                                            time_t now_seconds) {
  // Read before last_run_epoch_ is overwritten below. It is persisted, so the
  // previous run is remembered across a restart once the clock is set; the
  // millis() record covers the time before that.
  const int previous_run_epoch = last_run_epoch_->get();
  bool recent_run = false;
  if (valid_wall_clock(now_seconds) && previous_run_epoch > 0) {
    recent_run = static_cast<int>(now_seconds) - previous_run_epoch <
                 kEmergencyTestRunWindowSeconds;
  } else if (emergency_run_ended_) {
    recent_run = now_ms - emergency_run_end_ms_ < kEmergencyTestRunWindowMs;
  }
  emergency_test_run_allowed_ = !recent_run;

  if (idle_timer_active_) {
    const unsigned long idle_duration = now_ms - idle_start_ms_;
    *last_idle_time_ = static_cast<float>(idle_duration) / 1000.0f;
    cumulative_idle_time_ms_ += idle_duration;
    idle_sample_count_++;
    *average_idle_time_ = static_cast<float>(cumulative_idle_time_ms_) /
                          static_cast<float>(idle_sample_count_) / 1000.0f;

    if (idle_duration < min_idle_time_ms()) {
      ++(*short_cycle_count_);
    } else {
      *short_cycle_count_ = 0;
    }
  }

  if (valid_wall_clock(now_seconds)) {
    *last_run_epoch_ = static_cast<int>(now_seconds);
  }

  logical_running_ = true;
  cycle_start_ms_ = now_ms;
  stop_candidate_ms_ = 0;
  idle_timer_active_ = false;
  LOG_I("%s: pump ON", pump_name_.c_str());
  *running_state_ = true;
}

void BilgePumpMonitor::handle_running_stop(unsigned long now_ms,
                                           time_t now_seconds) {
  const unsigned long run_duration = stop_candidate_ms_ - cycle_start_ms_;
  emergency_run_ended_ = true;
  emergency_run_end_ms_ = stop_candidate_ms_;
  update_daily_cycle_boundary(now_seconds);
  ++(*daily_cycle_count_);
  ++(*total_cycle_count_);
  completed_cycle_count_++;
  cumulative_run_time_ms_ += run_duration;

  logical_running_ = false;
  stop_candidate_ms_ = 0;
  cycle_start_ms_ = 0;
  idle_start_ms_ = now_ms;
  idle_timer_active_ = true;

  if (valid_wall_clock(now_seconds)) {
    *last_run_epoch_ = static_cast<int>(now_seconds);
  }

  *last_run_time_ = static_cast<float>(run_duration) / 1000.0f;
  *average_run_time_ = static_cast<float>(cumulative_run_time_ms_) /
                       static_cast<float>(completed_cycle_count_) / 1000.0f;
  *running_state_ = false;
}

void BilgePumpMonitor::update_short_cycle_window(unsigned long now_ms) {
  if (!idle_timer_active_ || logical_running_) {
    return;
  }

  if (short_cycle_count_->get() != 0 &&
      now_ms - idle_start_ms_ >= min_idle_time_ms()) {
    *short_cycle_count_ = 0;
  }
}

void BilgePumpMonitor::update_incident_alerts(time_t now_seconds) {
  const bool emergency_running = emergency_alarm_due(millis());
  const bool max_run_time_exceeded =
      logical_running_ && cycle_start_ms_ != 0 &&
      millis() - cycle_start_ms_ > max_run_time_ms();
  const bool short_cycles_exceeded =
      short_cycle_count_->get() > max_consecutive_short_cycles_;

  update_incident_alert(PumpIncidentAlertId::kEmergencyRunning,
                        emergency_running, "alarm",
                        "Emergency bilge pump is running", true, now_seconds);
  update_incident_alert(PumpIncidentAlertId::kMaxRunTime, max_run_time_exceeded,
                        "alarm", "Pump exceeded max continuous run time", true,
                        now_seconds);
  update_incident_alert(PumpIncidentAlertId::kShortCycles, short_cycles_exceeded,
                        "alarm", "Pump exceeded max consecutive short cycles",
                        true, now_seconds);
}

void BilgePumpMonitor::update_health_notification(time_t now_seconds) {
  if (logical_running_ && cycle_start_ms_ != 0 &&
      millis() - cycle_start_ms_ > max_run_time_ms()) {
    set_health_notification("alarm",
                            "Pump exceeded max continuous run time", true);
    return;
  }

  if (emergency_alarm_due(millis())) {
    set_health_notification("alarm", "Emergency bilge pump is running", true);
    return;
  }

  if (short_cycle_count_->get() > max_consecutive_short_cycles_) {
    set_health_notification("alarm",
                            "Pump exceeded max consecutive short cycles", true);
    return;
  }

  if (valid_wall_clock(now_seconds)) {
    const int last_run_epoch = last_run_epoch_->get();
    const int reference_epoch =
        last_run_epoch > 0 ? last_run_epoch : health_reference_epoch_->get();
    if (reference_epoch > 0) {
      const int age_s = static_cast<int>(now_seconds) - reference_epoch;
      if (is_emergency_pump()) {
        if (age_s > emergency_exercise_warning_seconds()) {
          set_health_notification(
              "warn",
              "Emergency bilge pump has not been exercised recently", false);
          return;
        }
      } else if (age_s > primary_stale_warning_seconds()) {
        set_health_notification("warn",
                                "Primary bilge pump has not run recently", false);
        return;
      }
    }
  }

  set_health_notification("normal", "Pump health is normal", false);
}

void BilgePumpMonitor::evaluate() {
  if (!started_) {
    return;
  }

  const unsigned long now_ms = millis();
  const time_t now_seconds = time(nullptr);
  update_daily_cycle_boundary(now_seconds);
  update_health_reference_epoch(now_seconds);

  if (raw_running_) {
    stop_candidate_ms_ = 0;
    if (!logical_running_) {
      handle_running_start(now_ms, now_seconds);
    }
  } else if (logical_running_ && stop_candidate_ms_ == 0) {
    stop_candidate_ms_ = now_ms;
    LOG_I("%s: pump OFF", pump_name_.c_str());
  }

  if (logical_running_ && !raw_running_ && stop_candidate_ms_ != 0 &&
      now_ms - stop_candidate_ms_ >= min_stop_time_ms()) {
    handle_running_stop(now_ms, now_seconds);
  }

  if (emergency_run_ended_ &&
      now_ms - emergency_run_end_ms_ >= kEmergencyTestRunWindowMs) {
    emergency_run_ended_ = false;  // expire before millis() can wrap onto it
  }

  update_short_cycle_window(now_ms);
  update_incident_alerts(now_seconds);
  update_health_notification(now_seconds);
}

void BilgePumpMonitor::onSKServerConnect() {
  if (!started_) {
    return;
  }

  // Outputs behind a RepeatMutable are re-sent through it (see resend()).
  // They go first: if SensESP's delta queue overflows it drops the oldest
  // entries, and these come back on their next repeat anyway.
  running_repeat_->resend(running_state_);
  daily_cycle_count_repeat_->resend(daily_cycle_count_);
  total_cycle_count_repeat_->resend(total_cycle_count_);
  short_cycle_count_repeat_->resend(short_cycle_count_);
  average_run_time_repeat_->resend(average_run_time_);
  average_idle_time_repeat_->resend(average_idle_time_);
  health_notification_repeat_->resend(health_notification_);

  // Outputs with no repeater go last, by notifying their source: last_run_time,
  // last_idle_time and the incident alerts below.
  last_run_time_->notify();
  last_idle_time_->notify();

  // Incident alerts are sent only when they change, so re-send each one that
  // is still active: an alert raised while disconnected (e.g. during a server
  // restart) would otherwise never reach Signal K. A resolved incident is not
  // re-sent, so a server restart does not raise its old warning again.
  for (auto& incident_alert : incident_alerts_) {
    if (incident_alert.active && incident_alert.notification != nullptr) {
      incident_alert.notification->notify();
    }
  }
}

void BilgePumpMonitor::log_state_debug() const {
  LOG_D(
      "%s state: raw_running=%s logical_running=%s short_cycles=%d daily=%d total=%d health=%s",
      pump_name_.c_str(), raw_running_ ? "true" : "false",
      logical_running_ ? "true" : "false",
      started_ ? short_cycle_count_->get() : 0,
      started_ ? daily_cycle_count_->get() : 0,
      started_ ? total_cycle_count_->get() : 0,
      active_health_state_.c_str());
}

bool BilgePumpMonitor::to_json(JsonObject& root) {
  root["max_run_time_s"] = max_run_time_s_;
  root["min_stop_time_s"] = min_stop_time_s_;
  root["min_idle_time_s"] = min_idle_time_s_;
  root["max_consecutive_short_cycles"] = max_consecutive_short_cycles_;
  root["primary_stale_warning_days"] = primary_stale_warning_days_;
  root["emergency_exercise_warning_days"] = emergency_exercise_warning_days_;
  root["emergency_alarm_delay_s"] = emergency_alarm_delay_s_;
  root["running_sk_path"] = running_sk_path_;
  root["last_run_time_sk_path"] = last_run_time_sk_path_;
  root["last_idle_time_sk_path"] = last_idle_time_sk_path_;
  root["daily_cycle_count_sk_path"] = daily_cycle_count_sk_path_;
  root["total_cycle_count_sk_path"] = total_cycle_count_sk_path_;
  root["short_cycle_count_sk_path"] = short_cycle_count_sk_path_;
  root["average_run_time_sk_path"] = average_run_time_sk_path_;
  root["average_idle_time_sk_path"] = average_idle_time_sk_path_;
  root["health_notification_sk_path"] = health_notification_sk_path_;
  return true;
}

bool BilgePumpMonitor::from_json(const JsonObject& config) {
  if (!config["max_run_time_s"].isNull()) {
    max_run_time_s_ =
        clamp_min_float(config["max_run_time_s"].as<float>(), 1.0f);
  }
  if (!config["min_stop_time_s"].isNull()) {
    min_stop_time_s_ =
        clamp_min_float(config["min_stop_time_s"].as<float>(),
                        kMinThresholdSeconds);
  }
  if (!config["min_idle_time_s"].isNull()) {
    min_idle_time_s_ =
        clamp_min_float(config["min_idle_time_s"].as<float>(),
                        kMinThresholdSeconds);
  }
  if (!config["max_consecutive_short_cycles"].isNull()) {
    max_consecutive_short_cycles_ =
        clamp_min_int(config["max_consecutive_short_cycles"].as<int>(), 0);
  }
  if (!config["primary_stale_warning_days"].isNull()) {
    primary_stale_warning_days_ = clamp_min_float(
        config["primary_stale_warning_days"].as<float>(), 0.0f);
  }
  if (!config["emergency_exercise_warning_days"].isNull()) {
    emergency_exercise_warning_days_ = clamp_min_float(
        config["emergency_exercise_warning_days"].as<float>(), 0.0f);
  }
  if (!config["emergency_alarm_delay_s"].isNull()) {
    const float delay_s = config["emergency_alarm_delay_s"].as<float>();
    if (!std::isnan(delay_s)) {
      const float clamped_s = clamp_min_float(delay_s, kMinThresholdSeconds);
      emergency_alarm_delay_s_ = clamped_s > kMaxEmergencyAlarmDelaySeconds
                                     ? kMaxEmergencyAlarmDelaySeconds
                                     : clamped_s;
    }
  }
  if (!config["running_sk_path"].isNull()) {
    running_sk_path_ = config["running_sk_path"].as<String>();
  }
  if (!config["last_run_time_sk_path"].isNull()) {
    last_run_time_sk_path_ = config["last_run_time_sk_path"].as<String>();
  }
  if (!config["last_idle_time_sk_path"].isNull()) {
    last_idle_time_sk_path_ = config["last_idle_time_sk_path"].as<String>();
  }
  if (!config["daily_cycle_count_sk_path"].isNull()) {
    daily_cycle_count_sk_path_ =
        config["daily_cycle_count_sk_path"].as<String>();
  }
  if (!config["total_cycle_count_sk_path"].isNull()) {
    total_cycle_count_sk_path_ =
        config["total_cycle_count_sk_path"].as<String>();
  }
  if (!config["short_cycle_count_sk_path"].isNull()) {
    short_cycle_count_sk_path_ =
        config["short_cycle_count_sk_path"].as<String>();
  }
  if (!config["average_run_time_sk_path"].isNull()) {
    average_run_time_sk_path_ =
        config["average_run_time_sk_path"].as<String>();
  }
  if (!config["average_idle_time_sk_path"].isNull()) {
    average_idle_time_sk_path_ =
        config["average_idle_time_sk_path"].as<String>();
  }
  if (!config["health_notification_sk_path"].isNull()) {
    health_notification_sk_path_ =
        config["health_notification_sk_path"].as<String>();
  }
  for (auto& incident_alert : incident_alerts_) {
    incident_alert.sk_path = make_incident_alert_path(incident_alert.suffix);
  }

  return true;
}

const String ConfigSchema(const BilgePumpMonitor& obj) {
  return R"json({
    "type":"object",
    "properties":{
      "max_run_time_s":{"title":"Max Run Time (s)","description":"Maximum seconds the pump may run continuously before health changes to alarm.","type":"number"},
      "min_stop_time_s":{"title":"Min Stop Time (s)","description":"Seconds the input must stay inactive before a run cycle is considered stopped.","type":"number"},
      "min_idle_time_s":{"title":"Min Idle Time (s)","description":"Idle time threshold used to clear the consecutive short-cycle counter.","type":"number"},
      "max_consecutive_short_cycles":{"title":"Max Consecutive Short Cycles","description":"Number of consecutive short cycles allowed before health changes to alarm.","type":"number"},
      "primary_stale_warning_days":{"title":"Primary Stale Warning (days)","description":"Days a primary pump can go without running before health changes to warn.","type":"number"},
      "emergency_exercise_warning_days":{"title":"Emergency Exercise Warning (days)","description":"Days an emergency pump can go without running before health changes to warn.","type":"number"},
      "emergency_alarm_delay_s":{"title":"Emergency Alarm Delay (s)","description":"Seconds (0-30) an emergency pump must run before it raises the running alarm. A shorter run is treated as a test, unless it starts within 24 hours of the previous run.","type":"number"},
      "running_sk_path":{"title":"Running Path","description":"Signal K path used to report whether the pump is currently running.","type":"string"},
      "last_run_time_sk_path":{"title":"Last Run Time Path","description":"Signal K path used to report the most recently completed run duration.","type":"string"},
      "last_idle_time_sk_path":{"title":"Last Idle Time Path","description":"Signal K path used to report the idle time before a run cycle starts.","type":"string"},
      "daily_cycle_count_sk_path":{"title":"Daily Cycle Count Path","description":"Signal K path used to report completed cycles since the most recent midnight boundary.","type":"string"},
      "total_cycle_count_sk_path":{"title":"Total Cycle Count Path","description":"Signal K path used to report completed cycles retained across reboot.","type":"string"},
      "short_cycle_count_sk_path":{"title":"Short Cycle Count Path","description":"Signal K path used to report the current consecutive short-cycle count.","type":"string"},
      "average_run_time_sk_path":{"title":"Average Run Time Path","description":"Signal K path used to report average completed run duration since boot.","type":"string"},
      "average_idle_time_sk_path":{"title":"Average Idle Time Path","description":"Signal K path used to report average idle duration since boot.","type":"string"},
      "health_notification_sk_path":{"title":"Health Notification Path","description":"Signal K notification path used to report overall pump health.","type":"string"}
    }
  })json";
}

}  // namespace app
