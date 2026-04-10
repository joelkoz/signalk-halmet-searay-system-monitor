#ifndef FLOOR_H
#define FLOOR_H

#include "sensesp/transforms/transform.h"


/**
 * Transform that clamps the input to a minimum value. If the input value is less
 * than the minimum value, the output will be the minimum value.
 */
template <typename T>
class Floor : public sensesp::Transform<T, T> {
 public:
  Floor(T minVal) {
    _minVal = minVal;
  }

  void set(const T& input) override {
    T output = (input < _minVal) ? _minVal : input;
    this->emit(output);
  }  

 private:
  T _minVal;
};

#endif