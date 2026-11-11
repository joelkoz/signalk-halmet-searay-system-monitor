#include "system/ui_counter.h"

namespace app {

UICounter UICounter::uiGroup_(100, 3000);

UICounter::UICounter(int increment, int initialValue)
    : nextValue_(initialValue), increment_(increment) {
  if (initialValue == -1) {
    nextValue_ = uiGroup_.nextValue();
  }
}

int UICounter::nextValue() {
  int nextValue = nextValue_;
  nextValue_ += increment_;
  return nextValue;
}

}  // namespace app
