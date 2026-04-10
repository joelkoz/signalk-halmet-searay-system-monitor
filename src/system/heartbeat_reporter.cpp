#include "system/heartbeat_reporter.h"

#include "system/timer.h"
#include "sensesp/system/lambda_consumer.h"
#include "sensesp/signalk/signalk_output.h"

namespace app {

namespace {

constexpr float kMinHeartbeatIntervalSeconds = 0.1f;

float clamp_heartbeat_interval(float seconds) {
  return seconds < kMinHeartbeatIntervalSeconds ? kMinHeartbeatIntervalSeconds
                                                : seconds;
}

unsigned long heartbeat_interval_ms(float seconds) {
  return static_cast<unsigned long>(clamp_heartbeat_interval(seconds) * 1000.0f);
}

}  // namespace

HeartbeatReporter::HeartbeatReporter(const String& config_path,
                                     const String& default_sk_path,
                                     float default_interval_s)
    : FileSystemSaveable(config_path),
      sk_path_(default_sk_path),
      heartbeat_interval_s_(default_interval_s) {
  load();

  heartbeat_value_ = new sensesp::ObservableValue<float>(0.0f);
  auto* timer = new Timer("", heartbeat_interval_ms(heartbeat_interval_s_));
  timer
      ->connect_to(new sensesp::LambdaConsumer<unsigned long>(
          [this](unsigned long uptime_ms) {
            if (heartbeat_value_ != nullptr) {
              heartbeat_value_->set(static_cast<float>(uptime_ms) / 1000.0f);
            }
          }));
  heartbeat_value_->connect_to(new sensesp::SKOutputFloat(
      sk_path_, "", new sensesp::SKMetadata(
                       "s", "SeaRay Systems Monitor Heartbeat",
                       "Current device uptime in seconds")));
  sensesp::event_loop()->onDelay(0, [this]() {
    if (heartbeat_value_ != nullptr) {
      heartbeat_value_->set(static_cast<float>(millis()) / 1000.0f);
    }
  });
}

void HeartbeatReporter::republish_output() {
  if (heartbeat_value_ != nullptr) {
    heartbeat_value_->notify();
  }
}

bool HeartbeatReporter::to_json(JsonObject& root) {
  root["heartbeat_interval_s"] = heartbeat_interval_s_;
  root["sk_path"] = sk_path_;
  return true;
}

bool HeartbeatReporter::from_json(const JsonObject& config) {
  if (config["heartbeat_interval_s"].is<float>()) {
    heartbeat_interval_s_ = config["heartbeat_interval_s"].as<float>();
  }

  if (!config["sk_path"].isNull()) {
    sk_path_ = config["sk_path"].as<String>();
  }

  return true;
}

const String ConfigSchema(const HeartbeatReporter& obj) {
  return R"json({"type":"object","properties":{"heartbeat_interval_s":{"title":"Heartbeat Interval (s)","description":"How often the device heartbeat is sent to Signal K, in seconds. Changes require a device restart.","type":"number"},"sk_path":{"title":"Heartbeat Path","description":"Signal K path used to publish device uptime in seconds as a heartbeat value. Changes require a device restart.","type":"string"}}})json";
}

}  // namespace app
