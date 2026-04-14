#include "signalk/signalk_time_sync.h"

#include <sys/time.h>
#include <time.h>

#include "Debug.h"
#include "sensesp/system/lambda_consumer.h"

namespace app {

SignalKTimeSync::SignalKTimeSync(const String& sk_path, int listen_delay_ms,
                                 const String& config_path)
    : sensesp::FileSystemSaveable(config_path),
      sk_path_(sk_path),
      listen_delay_ms_(listen_delay_ms) {
  load();
}

void SignalKTimeSync::begin() {
  if (started_) {
    return;
  }

  input_ = new SKTimeListener(sk_path_, listen_delay_ms_, "");
  input_->begin();
  input_->connect_to(new sensesp::LambdaConsumer<time_t>(
      [this](time_t epoch_seconds) { this->handle_time_update(epoch_seconds); }));
  started_ = true;
}

void SignalKTimeSync::handle_time_update(time_t epoch_seconds) {
  const time_t now_seconds = time(nullptr);
  if (now_seconds == epoch_seconds && last_synced_epoch_ == epoch_seconds) {
    return;
  }

  const timeval tv = {.tv_sec = epoch_seconds, .tv_usec = 0};
  if (settimeofday(&tv, nullptr) != 0) {
    LOG_E("Time sync: settimeofday failed for epoch %lld",
          static_cast<long long>(epoch_seconds));
    return;
  }

  last_synced_epoch_ = epoch_seconds;
  LOG_I("Time sync: system clock set from Signal K epoch %lld",
        static_cast<long long>(epoch_seconds));
}

bool SignalKTimeSync::to_json(JsonObject& root) {
  root["sk_path"] = sk_path_;
  root["listen_delay"] = listen_delay_ms_;
  return true;
}

bool SignalKTimeSync::from_json(const JsonObject& config) {
  if (!config["sk_path"].isNull()) {
    sk_path_ = config["sk_path"].as<String>();
  }
  if (!config["listen_delay"].isNull()) {
    listen_delay_ms_ = config["listen_delay"].as<int>();
  }
  return true;
}

const String ConfigSchema(const SignalKTimeSync& obj) {
  return R"json({
    "type":"object",
    "properties":{
      "sk_path":{"title":"Signal K Time Path","type":"string","description":"Signal K path that provides the ship datetime string."},
      "listen_delay":{"title":"Listen Delay (ms)","type":"number","description":"Minimum interval between Signal K time updates in milliseconds."}
    }
  })json";
}

}  // namespace app
