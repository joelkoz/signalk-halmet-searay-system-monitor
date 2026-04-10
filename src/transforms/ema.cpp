#include "ema.h"

namespace halmet {

EMA::EMA(float alpha, const String& config_path)
    : sensesp::FloatTransform(config_path), alpha_{alpha}, initialized_{false} {
  load();
}

void EMA::set(const float& input) {
    if (!initialized_) {
        output_ = input;
        initialized_ = true;
    }
    else {
        output_ += alpha_ * (input - output_);
    }

    notify();

}

bool EMA::to_json(JsonObject& root) {
  root["alpha"] = alpha_;
  return true;
}

bool EMA::from_json(const JsonObject& config) {
  String const expected[] = {"alpha"};
  for (auto str : expected) {
    if (!config[str].is<JsonVariant>()) {
      return false;
    }
  }
  float const alpha_new = config["alpha"];
  if (alpha_ != alpha_new) {
    alpha_ = alpha_new;
    initialized_ = false;
    output_ = 0.0;
  }
  return true;
}

const String ConfigSchema(const EMA& obj) {
  return R"###({"type":"object","properties":{"alpha":{"title":"Smoothing factor","description":"Smoothing factor between 0 and 1. Lower values give more smoothing. Higher value is more responsive to changes.","type":"number"}}})###";
}

}  // namespace halmet
