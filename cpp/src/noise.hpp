#pragma once

#include "prng.hpp"

class Noise {
 public:
  explicit Noise(Prng& prng);

  double sample(double x, double y = 0.0, double z = 0.0);

 private:
  void ensureTable();

  static constexpr int kSize = 4095;
  static constexpr int kYWrap = 1 << 4;
  static constexpr int kZWrap = 1 << 8;

  Prng* prng_ = nullptr;
  bool ready_ = false;
  double table_[kSize + 1]{};
  int octaves_ = 4;
  double falloff_ = 0.5;
};
