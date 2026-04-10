#pragma once

#include <Arduino.h>

#include "sensesp/signalk/signalk_put_request_listener.h"

namespace app {

class MutableBoolSKPutRequestListener
    : public sensesp::BoolSKPutRequestListener {
 public:
  explicit MutableBoolSKPutRequestListener(const String& sk_path);

  void set_sk_path(const String& sk_path);
  void parse_value(const JsonObject& put) override;
};

}  // namespace app
