#include "ui_counter.h"

using namespace halmet;

UICounter UICounter::uiGroup_(100, 3000);

UICounter::UICounter(int increment, int initialValue) : increment_(increment), nextValue_(initialValue) {
    if (initialValue == -1) {
        nextValue_ = uiGroup_.nextValue();
    }
}

int UICounter::nextValue() {
    int nextValue = nextValue_;
    nextValue_ += increment_;
    return nextValue;
}
