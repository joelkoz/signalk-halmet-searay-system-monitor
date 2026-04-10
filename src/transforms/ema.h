#ifndef SENSESP_TRANSFORMS_EMA_H_
#define SENSESP_TRANSFORMS_EMA_H_

#include <vector>

#include "sensesp/ui/config_item.h"
#include "sensesp/transforms/transform.h"

namespace halmet {

/**
 * @brief Outputs the median value of alpha inputs.
 *
 * Creates a sorted list of alpha values and outputs the one
 * in the middle (i.e. element number 'alpha / 2').
 *
 * @param alpha Number of values you want to take the median of.
 *
 * @param config_path Path to configure this transform in the Config UI.
 */
class EMA : public sensesp::FloatTransform {
 public:
  EMA(float alpha = 0.1, const String& config_path = "");
  virtual void set(const float& input) override;
  virtual bool to_json(JsonObject& root) override;
  virtual bool from_json(const JsonObject& config) override;

  void reset() { initialized_ = false; }
  
 private:
  bool initialized_;
  float alpha_;
};

const String ConfigSchema(const EMA& obj);

inline bool ConfigRequiresRestart(const EMA& obj) {
  return true;
}

}  // namespace halmet
#endif
