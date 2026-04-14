#include "signalk/sk_time_listener.h"

#include "Debug.h"
#include "sensesp/system/lambda_consumer.h"

namespace app {

namespace {

int64_t days_from_civil(int year, unsigned month, unsigned day) {
  year -= month <= 2;
  const int era = (year >= 0 ? year : year - 399) / 400;
  const unsigned year_of_era = static_cast<unsigned>(year - era * 400);
  const unsigned day_of_year =
      (153 * (month + (month > 2 ? -3 : 9)) + 2) / 5 + day - 1;
  const unsigned day_of_era =
      year_of_era * 365 + year_of_era / 4 - year_of_era / 100 + day_of_year;
  return era * 146097 + static_cast<int>(day_of_era) - 719468;
}

}  // namespace

SKTimeListener::SKTimeListener(const String& sk_path, int listen_delay_ms,
                               const String& config_path)
    : sensesp::FileSystemSaveable(config_path),
      sk_path_(sk_path),
      listen_delay_ms_(listen_delay_ms) {
  if (!config_path.isEmpty()) {
    load();
  }
}

void SKTimeListener::begin() {
  if (started_) {
    return;
  }

  raw_listener_ = new sensesp::StringSKListener(sk_path_, listen_delay_ms_);
  raw_listener_->connect_to(new sensesp::LambdaConsumer<String>(
      [this](const String& datetime_string) {
        this->handle_datetime_string(datetime_string);
      }));
  started_ = true;
}

bool SKTimeListener::to_json(JsonObject& root) {
  root["sk_path"] = sk_path_;
  root["listen_delay"] = listen_delay_ms_;
  return true;
}

bool SKTimeListener::from_json(const JsonObject& config) {
  if (!config["sk_path"].isNull()) {
    sk_path_ = config["sk_path"].as<String>();
  }
  if (!config["listen_delay"].isNull()) {
    listen_delay_ms_ = config["listen_delay"].as<int>();
  }
  return true;
}

void SKTimeListener::handle_datetime_string(const String& datetime_string) {
  time_t epoch_seconds = 0;
  if (!parse_datetime_to_epoch(datetime_string, &epoch_seconds)) {
    LOG_W("Time sync: unable to parse Signal K datetime '%s'",
          datetime_string.c_str());
    return;
  }

  this->emit(epoch_seconds);
}

bool SKTimeListener::parse_datetime_to_epoch(const String& datetime_string,
                                             time_t* epoch_seconds) const {
  if (epoch_seconds == nullptr) {
    return false;
  }

  int year = 0;
  int month = 0;
  int day = 0;
  int hour = 0;
  int minute = 0;
  int second = 0;

  if (sscanf(datetime_string.c_str(), "%d-%d-%dT%d:%d:%d", &year, &month, &day,
             &hour, &minute, &second) != 6) {
    return false;
  }

  const int64_t days = days_from_civil(year, static_cast<unsigned>(month),
                                       static_cast<unsigned>(day));
  const int64_t seconds =
      days * 86400LL + hour * 3600LL + minute * 60LL + second;
  *epoch_seconds = static_cast<time_t>(seconds);
  return *epoch_seconds > 0;
}

const String ConfigSchema(const SKTimeListener& obj) {
  return R"json({
    "type":"object",
    "properties":{
      "sk_path":{"title":"Signal K Time Path","type":"string","description":"Signal K path that provides the ship datetime string."},
      "listen_delay":{"title":"Listen Delay (ms)","type":"number","description":"Minimum interval between Signal K time updates in milliseconds."}
    }
  })json";
}

}  // namespace app
