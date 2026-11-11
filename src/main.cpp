#include <Arduino.h>

#include <memory>
#include <stdlib.h>
#include <time.h>

#include <ArduinoJson.h>
#include <ArduinoOTA.h>
#include <WiFi.h>
#include <esp_heap_caps.h>
#include <esp_task_wdt.h>

#include "env_config.h"

#include "Debug.h"
#include "pumps/bilge_pump_monitor.h"
#include "sensors/analog_threshold_input.h"
#include "signalk/signalk_time_sync.h"
#include "sensesp/sensors/digital_input.h"
#include "sensesp/signalk/signalk_output.h"
#include "sensesp/signalk/signalk_ws_client.h"
#include "sensesp/system/lambda_consumer.h"
#include "sensesp/system/observablevalue.h"
#include "sensesp/ui/config_item.h"
#include "sensesp_app_builder.h"
#include "system/heartbeat_reporter.h"
#include "system/halmet_const.h"
#include "system/halmet_serial.h"
#include "system/ui_counter.h"
#include "transforms/repeat_mutable.h"
#include "utils/ExpiryTimer.h"

using namespace sensesp;
using namespace halmet;
using namespace app;

namespace {

constexpr uint8_t kAftBilgePumpChannel = 0;
constexpr uint8_t kEngineRoomFireDetectorChannel = 1;
constexpr uint8_t kPortIgnitionChannel = 2;
constexpr uint8_t kStarboardIgnitionChannel = 3;

constexpr int kForwardEmergencyPumpPin = kDigitalInputPin1;
constexpr int kInteriorEmergencyPumpPin = kDigitalInputPin2;
constexpr int kAftEmergencyPumpPin = kDigitalInputPin3;
constexpr int kForwardPumpPin = kDigitalInputPin4;

constexpr unsigned int kDigitalInputReadIntervalMs = 100;
constexpr unsigned int kAnalogInputReadIntervalMs = 100;
constexpr unsigned int kPumpStateLogIntervalMs = 2000;
constexpr unsigned int kNetworkStatusLogIntervalMs = 5000;

// SensESP queues at most 20 outputs between sends (one send every 100 ms) and
// drops the oldest beyond that. A pump re-sends up to 12 outputs on connect,
// so the connect re-send runs one step at a time, this far apart.
constexpr unsigned int kResendStepSpacingMs = 250;

// The main loop must tick at least this often or the device reboots. OTA
// uploads run inside the loop, so the OTA callbacks feed the watchdog too.
constexpr uint32_t kMainLoopWatchdogTimeoutMs = 15000;

constexpr float kDefaultPumpStateReportIntervalSeconds = 10.0f;
constexpr float kDefaultPumpAggregateReportIntervalSeconds = 30.0f;
constexpr const char* kPumpStateReportIntervalConfigPath =
    "/system/pump_state_report_interval";
constexpr const char* kPumpAggregateReportIntervalConfigPath =
    "/system/pump_aggregate_report_interval_seconds";

constexpr const char* kHeartbeatConfigPath = "/system/heartbeat";
constexpr float kDefaultHeartbeatIntervalSeconds = 10.0f;
constexpr const char* kHeartbeatSignalKPath =
    "electrical.system.monitor.heartbeat";

constexpr const char* kEngineRoomFireNotificationPath =
    "notifications.fire.engineRoom";
constexpr const char* kPortIgnitionPath = "propulsion.port.ignition";
constexpr const char* kStarboardIgnitionPath = "propulsion.starboard.ignition";

std::shared_ptr<BilgePumpMonitor> forward_pump;
std::shared_ptr<BilgePumpMonitor> aft_pump;
std::shared_ptr<BilgePumpMonitor> forward_emergency_pump;
std::shared_ptr<BilgePumpMonitor> aft_emergency_pump;
std::shared_ptr<BilgePumpMonitor> interior_emergency_pump;
std::shared_ptr<HeartbeatReporter> heartbeat_reporter;
std::shared_ptr<PersistingObservableValue<float>> pump_state_report_interval_s;
std::shared_ptr<PersistingObservableValue<float>>
    pump_aggregate_report_interval_s;
std::shared_ptr<SignalKTimeSync> signalk_time_sync;

DigitalInputState* forward_emergency_input = nullptr;
DigitalInputState* interior_emergency_input = nullptr;
DigitalInputState* aft_emergency_input = nullptr;
DigitalInputState* forward_input = nullptr;
AnalogThresholdInput* aft_input = nullptr;
AnalogThresholdInput* engine_room_fire_input = nullptr;
AnalogThresholdInput* port_ignition_input = nullptr;
AnalogThresholdInput* starboard_ignition_input = nullptr;

ObservableValue<String>* engine_room_fire_notification = nullptr;
RepeatMutable<String>* engine_room_fire_repeat = nullptr;
RepeatMutable<bool>* port_ignition_repeat = nullptr;
RepeatMutable<bool>* starboard_ignition_repeat = nullptr;

const char* bool_text(bool value) { return value ? "true" : "false"; }

DigitalInputState* make_digital_input(uint8_t pin) {
  auto* input =
      new DigitalInputState(pin, INPUT, kDigitalInputReadIntervalMs);
  input->emit(digitalRead(pin) == HIGH);
  return input;
}

String make_notification_payload(const char* state, const String& message,
                                 bool audible) {
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

void configure_ship_timezone() {
  setenv("TZ", kShipTimeZonePosix, 1);
  tzset();
  LOG_I("Time sync: ship timezone set to %s (%s)", kShipTimeZoneName,
        kShipTimeZonePosix);
}

void set_engine_room_fire_notification(bool active) {
  if (engine_room_fire_notification == nullptr) {
    return;
  }

  if (active) {
    engine_room_fire_notification->set(make_notification_payload(
        "emergency", "Engine room fire heat detector active", true));
    LOG_E("Engine room fire heat detector active");
  } else {
    engine_room_fire_notification->set(
        make_notification_payload("normal",
                                  "Engine room fire heat detector normal",
                                  false));
    ONCE_EVERY(5000, { LOG_I("Engine room fire heat detector normal"); });
  }
}

void log_heap(const char* when) {
  LOG_I("Heap %s: free %u, largest block %u, min free %u", when,
        static_cast<unsigned>(esp_get_free_heap_size()),
        static_cast<unsigned>(
            heap_caps_get_largest_free_block(MALLOC_CAP_8BIT)),
        static_cast<unsigned>(esp_get_minimum_free_heap_size()));
}

void resend_device_outputs() {
  // Repeater-backed outputs first; the heartbeat has no repeater, so it goes
  // last where a queue overflow (which drops the oldest) cannot reach it.
  engine_room_fire_repeat->resend(engine_room_fire_notification);
  port_ignition_repeat->resend(port_ignition_input);
  starboard_ignition_repeat->resend(starboard_ignition_input);
  heartbeat_reporter->onSKServerConnect();
}

// The connect re-send, one step per entry, in order.
void (*const kResendSteps[])() = {
    resend_device_outputs,
    []() { forward_pump->onSKServerConnect(); },
    []() { aft_pump->onSKServerConnect(); },
    []() { forward_emergency_pump->onSKServerConnect(); },
    []() { aft_emergency_pump->onSKServerConnect(); },
    []() { interior_emergency_pump->onSKServerConnect(); },
};
constexpr size_t kResendStepCount =
    sizeof(kResendSteps) / sizeof(kResendSteps[0]);

// Incremented on every connect; a re-send chain started by an earlier connect
// sees the mismatch and stops.
uint32_t sk_connect_generation = 0;

// The since-boot minimum free heap when the latest connect began.
uint32_t min_free_at_connect = 0;

/**
 * @brief Runs one step of the connect re-send and schedules the next.
 *
 * Each step is scheduled kResendStepSpacingMs after the previous one actually
 * ran, so a stalled loop cannot merge two steps into one send window.
 */
void run_resend_step(size_t step, uint32_t generation) {
  if (generation != sk_connect_generation) {
    return;
  }
  if (step >= kResendStepCount) {
    // The metadata delta is built at the first send after connecting, so the
    // connect's heap low point shows up here, not in the log at connect.
    log_heap("after connect re-send");
    const uint32_t min_free = esp_get_minimum_free_heap_size();
    if (min_free < min_free_at_connect) {
      LOG_I("Heap: this connect set a new low (min free %u, was %u)",
            static_cast<unsigned>(min_free),
            static_cast<unsigned>(min_free_at_connect));
    }
    return;
  }
  kResendSteps[step]();
  event_loop()->onDelay(kResendStepSpacingMs, [step, generation]() {
    run_resend_step(step + 1, generation);
  });
}

/**
 * @brief Re-sends every current output. Called when the Signal K websocket
 * connects or reconnects, so the server has the full state.
 *
 * The device-level outputs and then each pump go one step at a time (see
 * run_resend_step()), so no burst overflows SensESP's delta queue. The first
 * step waits one spacing, keeping the outputs out of the first delta, which
 * carries the metadata.
 */
void onSKServerConnect() {
  // Compared at the end of the re-send to tell whether this connect set a new
  // low. (IDF's local-minimum window would be more precise, but while open it
  // also changes the minimum SensESP records as "min free before last reset".)
  min_free_at_connect = esp_get_minimum_free_heap_size();
  const uint32_t generation = ++sk_connect_generation;
  event_loop()->onDelay(kResendStepSpacingMs,
                        [generation]() { run_resend_step(0, generation); });
}

}  // namespace

void setup() {
  SetupLogging(ESP_LOG_DEBUG);

  Serial.begin(115200);
  debug_setup();

  SensESPAppBuilder builder;
  sensesp_app = (&builder)
                    ->set_hostname(kDeviceHostname)
                    ->set_wifi_client(kBoatWifiSsid, kBoatWifiPassword)
                    ->set_wifi_access_point(kDeviceHostname, kBoatWifiPassword)
                    ->set_sk_server(kSignalKServerHost, kSignalKServerPort)
                    ->get_app();

  // Task watchdog on the main loop: a boat-installed unit should reboot and
  // recover rather than hang silently if event_loop()->tick() ever stalls.
  // This runs after get_app(), which formats the filesystem on a blank board
  // and can block the loop for longer than the timeout. The Arduino core has
  // already started the watchdog; reconfigure it with a longer timeout and
  // keep the idle-task checks the core enables (on the ESP32, core 0's idle
  // task, where WiFi and lwIP run).
  esp_task_wdt_config_t wdt_config = {
      .timeout_ms = kMainLoopWatchdogTimeoutMs,
#if CONFIG_ESP_TASK_WDT_CHECK_IDLE_TASK_CPU0
      .idle_core_mask = BIT(0),
#else
      .idle_core_mask = 0,
#endif
      .trigger_panic = true};
  esp_err_t wdt_ret = esp_task_wdt_reconfigure(&wdt_config);
  if (wdt_ret == ESP_ERR_INVALID_STATE) {
    // The core did not start it (CONFIG_ESP_TASK_WDT_INIT off): start it.
    wdt_ret = esp_task_wdt_init(&wdt_config);
  }
  if (wdt_ret != ESP_OK) {
    LOG_E("Watchdog setup failed: %d", wdt_ret);
  } else {
    esp_err_t add_ret = esp_task_wdt_add(NULL);
    if (add_ret != ESP_OK) {
      LOG_E("Watchdog add failed: %d", add_ret);
    } else {
      LOG_I("Watchdog timer initialized with %lu ms timeout",
            static_cast<unsigned long>(kMainLoopWatchdogTimeoutMs));
    }
  }

  LOG_I("Signal K: direct server mode enabled host=%s port=%u use_mdns=false",
        kSignalKServerHost, kSignalKServerPort);

  configure_ship_timezone();

  signalk_time_sync = std::make_shared<SignalKTimeSync>();
  signalk_time_sync->begin();

  pump_state_report_interval_s =
      std::make_shared<PersistingObservableValue<float>>(
          kDefaultPumpStateReportIntervalSeconds,
          kPumpStateReportIntervalConfigPath);
  pump_aggregate_report_interval_s =
      std::make_shared<PersistingObservableValue<float>>(
          kDefaultPumpAggregateReportIntervalSeconds,
          kPumpAggregateReportIntervalConfigPath);

  forward_emergency_input = make_digital_input(kForwardEmergencyPumpPin);
  interior_emergency_input = make_digital_input(kInteriorEmergencyPumpPin);
  aft_emergency_input = make_digital_input(kAftEmergencyPumpPin);
  forward_input = make_digital_input(kForwardPumpPin);
  aft_input = new AnalogThresholdInput(kAftBilgePumpChannel, "Aft Bilge Pump",
                                       kAnalogInputReadIntervalMs);
  engine_room_fire_input = new AnalogThresholdInput(
      kEngineRoomFireDetectorChannel, "Engine Room Fire Heat Detector",
      kAnalogInputReadIntervalMs);
  port_ignition_input = new AnalogThresholdInput(
      kPortIgnitionChannel, "Port Ignition", kAnalogInputReadIntervalMs);
  starboard_ignition_input = new AnalogThresholdInput(
      kStarboardIgnitionChannel, "Starboard Ignition", kAnalogInputReadIntervalMs);

  forward_pump = std::make_shared<BilgePumpMonitor>(
      "forward", "Forward Bilge Pump", "/pumps/forward", forward_input,
      BilgePumpKind::kPrimary);
  aft_pump = std::make_shared<BilgePumpMonitor>(
      "aft", "Aft Bilge Pump", "/pumps/aft", aft_input,
      BilgePumpKind::kPrimary);
  forward_emergency_pump = std::make_shared<BilgePumpMonitor>(
      "forward.emergency", "Forward Emergency Bilge Pump",
      "/pumps/forward_emergency", forward_emergency_input,
      BilgePumpKind::kEmergency);
  aft_emergency_pump = std::make_shared<BilgePumpMonitor>(
      "aft.emergency", "Aft Emergency Bilge Pump", "/pumps/aft_emergency",
      aft_emergency_input, BilgePumpKind::kEmergency);
  interior_emergency_pump = std::make_shared<BilgePumpMonitor>(
      "interior.emergency", "Interior Emergency Bilge Pump",
      "/pumps/interior_emergency", interior_emergency_input,
      BilgePumpKind::kEmergency);

  UICounter uiCounter;

  ConfigItem(signalk_time_sync)
      ->set_title("Signal K Time Sync")
      ->set_description(
          "Signal K datetime path and update interval used to set the device wall clock.")
      ->set_sort_order(uiCounter.nextValue());

  ConfigItem(pump_state_report_interval_s)
      ->set_title("Pump State Report Interval (s)")
      ->set_description(
          "How often pump running and ignition states are re-sent to Signal K.")
      ->set_sort_order(uiCounter.nextValue());
  ConfigItem(pump_aggregate_report_interval_s)
      ->set_title("Pump Aggregate Report Interval (s)")
      ->set_description(
          "How often cycle statistics and health notifications are re-sent to Signal K.")
      ->set_sort_order(uiCounter.nextValue());

  forward_pump->begin(pump_state_report_interval_s->get(),
                      pump_aggregate_report_interval_s->get());
  aft_pump->begin(pump_state_report_interval_s->get(),
                  pump_aggregate_report_interval_s->get());
  forward_emergency_pump->begin(pump_state_report_interval_s->get(),
                                pump_aggregate_report_interval_s->get());
  aft_emergency_pump->begin(pump_state_report_interval_s->get(),
                            pump_aggregate_report_interval_s->get());
  interior_emergency_pump->begin(pump_state_report_interval_s->get(),
                                 pump_aggregate_report_interval_s->get());

  ConfigItem(forward_pump)
      ->set_title("Forward Bilge Pump")
      ->set_sort_order(uiCounter.nextValue());
  ConfigItem(aft_pump)
      ->set_title("Aft Bilge Pump")
      ->set_sort_order(uiCounter.nextValue());
  ConfigItem(forward_emergency_pump)
      ->set_title("Forward Emergency Bilge Pump")
      ->set_sort_order(uiCounter.nextValue());
  ConfigItem(aft_emergency_pump)
      ->set_title("Aft Emergency Bilge Pump")
      ->set_sort_order(uiCounter.nextValue());
  ConfigItem(interior_emergency_pump)
      ->set_title("Interior Emergency Bilge Pump")
      ->set_sort_order(uiCounter.nextValue());

  const unsigned long state_interval_ms = static_cast<unsigned long>(
      pump_state_report_interval_s->get() * 1000.0f);
  const unsigned long aggregate_interval_ms = static_cast<unsigned long>(
      pump_aggregate_report_interval_s->get() * 1000.0f);

  engine_room_fire_notification = new ObservableValue<String>(
      make_notification_payload("normal", "Engine room fire heat detector normal",
                                false));
  // No metadata on notification paths; see BilgePumpMonitor::begin().
  auto* engine_room_fire_output = new SKOutputRawJson(
      kEngineRoomFireNotificationPath, "/inputs/engine_room_fire/sk");
  engine_room_fire_repeat = new RepeatMutable<String>(aggregate_interval_ms);
  engine_room_fire_notification->connect_to(engine_room_fire_repeat)
      ->connect_to(engine_room_fire_output);
  ConfigItem(engine_room_fire_output)
      ->set_title("Engine Room Fire Notification")
      ->set_sort_order(uiCounter.nextValue());
  engine_room_fire_input->connect_to(new LambdaConsumer<bool>(
      [](bool active) { set_engine_room_fire_notification(active); }));

  auto* port_ignition_output = new SKOutputBool(
      kPortIgnitionPath, "/inputs/port_ignition/sk",
      new SKMetadata("", "Port Ignition"));
  port_ignition_repeat = new RepeatMutable<bool>(state_interval_ms);
  port_ignition_input->connect_to(port_ignition_repeat)
      ->connect_to(port_ignition_output);
  ConfigItem(port_ignition_output)
      ->set_title("Port Ignition Path")
      ->set_sort_order(uiCounter.nextValue());

  auto* starboard_ignition_output = new SKOutputBool(
      kStarboardIgnitionPath, "/inputs/starboard_ignition/sk",
      new SKMetadata("", "Starboard Ignition"));
  starboard_ignition_repeat = new RepeatMutable<bool>(state_interval_ms);
  starboard_ignition_input->connect_to(starboard_ignition_repeat)
      ->connect_to(starboard_ignition_output);
  ConfigItem(starboard_ignition_output)
      ->set_title("Starboard Ignition Path")
      ->set_sort_order(uiCounter.nextValue());

  sensesp_app->get_ws_client()->connect_to(
      new LambdaConsumer<SKWSConnectionState>([](SKWSConnectionState state) {
        if (state == SKWSConnectionState::kSKWSConnected) {
          LOG_I("Signal K: websocket connected, emitting full system state");
          // The connect path is the device's largest transient allocation
          // (websocket buffers plus the metadata delta).
          log_heap("at connect");
          onSKServerConnect();
        } else {
          // Stop a re-send chain still running from the previous connect.
          ++sk_connect_generation;
        }
      }));

  heartbeat_reporter = std::make_shared<HeartbeatReporter>(
      kHeartbeatConfigPath, kHeartbeatSignalKPath,
      kDefaultHeartbeatIntervalSeconds);
  ConfigItem(heartbeat_reporter)
      ->set_title("Device Heartbeat")
      ->set_sort_order(uiCounter.nextValue());

  event_loop()->onDelay(0, []() {
    ArduinoOTA.setHostname(kDeviceHostname);
    ArduinoOTA.setPassword(kOtaPassword);
    ArduinoOTA.onStart([]() {
      esp_task_wdt_reset();
      LOG_W("OTA: starting update");
    });
    ArduinoOTA.onEnd([]() {
      esp_task_wdt_reset();
      LOG_W("OTA: update complete");
    });
    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
      esp_task_wdt_reset();
      LOG_I("OTA: progress %u%%", (progress / (total / 100)));
    });
    ArduinoOTA.onError([](ota_error_t error) {
      esp_task_wdt_reset();
      LOG_E("OTA: error[%u]", error);
      if (error == OTA_AUTH_ERROR) {
        LOG_E("OTA: auth failed");
      } else if (error == OTA_BEGIN_ERROR) {
        LOG_E("OTA: begin failed");
      } else if (error == OTA_CONNECT_ERROR) {
        LOG_E("OTA: connect failed");
      } else if (error == OTA_RECEIVE_ERROR) {
        LOG_E("OTA: receive failed");
      } else if (error == OTA_END_ERROR) {
        LOG_E("OTA: end failed");
      }
    });
    ArduinoOTA.begin();
    event_loop()->onRepeat(20, []() { ArduinoOTA.handle(); });
  });

  event_loop()->onRepeat(kPumpStateLogIntervalMs, []() {
    forward_pump->log_state_debug();
    aft_pump->log_state_debug();
    forward_emergency_pump->log_state_debug();
    aft_emergency_pump->log_state_debug();
    interior_emergency_pump->log_state_debug();
  });

  event_loop()->onRepeat(kNetworkStatusLogIntervalMs, []() {
    const wl_status_t wifi_status = WiFi.status();
    const bool connected = wifi_status == WL_CONNECTED;
    const String current_ssid = connected ? WiFi.SSID() : "";
    const bool on_primary_boat_network =
        connected && current_ssid == kBoatWifiSsid;
    const String ip = connected ? WiFi.localIP().toString() : "0.0.0.0";
    const String sk_status =
        sensesp_app != nullptr && sensesp_app->get_ws_client() != nullptr
            ? sensesp_app->get_ws_client()->get_connection_status()
            : "unavailable";

    LOG_I(
        "Network: connected=%s status=%d current_ssid='%s' expected_ssid='%s' "
        "primary_match=%s ip=%s sk=%s",
        connected ? "true" : "false", static_cast<int>(wifi_status),
        current_ssid.c_str(), kBoatWifiSsid,
        on_primary_boat_network ? "true" : "false", ip.c_str(),
        sk_status.c_str());
  });

  LOG_I("SeaRay systems monitoring firmware started");
}

void loop() {
  esp_task_wdt_reset();
  ONCE_EVERY(2000, {
    LOG_I(
        "Pumps running: fwd: %s, aft: %s, efwd: %s, eaft: %s, eint: %s",
        bool_text(forward_pump->running_producer()->get()),
        bool_text(aft_pump->running_producer()->get()),
        bool_text(forward_emergency_pump->running_producer()->get()),
        bool_text(aft_emergency_pump->running_producer()->get()),
        bool_text(interior_emergency_pump->running_producer()->get()));
  });

  event_loop()->tick();
}
