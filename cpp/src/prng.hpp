#pragma once

#include <string>

// Same generator as the page: s = (s * s) % (999979 * 999983), in double precision.
class Prng {
 public:
  double seed(const std::string& text);
  double next();
  double state() const { return s_; }

 private:
  static double hash(const std::string& text);

  double s_ = 1234.0;
  static constexpr double p_ = 999979.0;
  static constexpr double q_ = 999983.0;
  static constexpr double m_ = p_ * q_;
};
