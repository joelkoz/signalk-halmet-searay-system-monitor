#include "signalk/mutable_bool_sk_put_request_listener.h"

namespace app {

MutableBoolSKPutRequestListener::MutableBoolSKPutRequestListener(
    const String& sk_path)
    : sensesp::BoolSKPutRequestListener(sk_path) {}

void MutableBoolSKPutRequestListener::set_sk_path(const String& sk_path) {
  this->sk_path = sk_path;
}

void MutableBoolSKPutRequestListener::parse_value(const JsonObject& put) {
  if (put["value"].is<bool>()) {
    this->emit(put["value"].as<bool>());
    return;
  }

  if (put["value"].is<int>()) {
    this->emit(put["value"].as<int>() != 0);
  }
}

}  // namespace app
