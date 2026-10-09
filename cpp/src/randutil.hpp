#pragma once

#include "prng.hpp"

#include <cmath>
#include <cstddef>
#include <vector>

inline double mapRange(double value, double istart, double istop, double ostart, double ostop) {
  return ostart + (ostop - ostart) * ((value - istart) / (istop - istart));
}

inline void loopNoise(std::vector<double>& values) {
  const double difference = values.back() - values.front();
  double low = 100;
  double high = -100;
  const double span = static_cast<double>(values.size() - 1);
  for (std::size_t i = 0; i < values.size(); ++i) {
    values[i] += (difference * (span - static_cast<double>(i))) / span;
    if (values[i] < low) {
      low = values[i];
    }
    if (values[i] > high) {
      high = values[i];
    }
  }
  for (double& value : values) {
    value = mapRange(value, low, high, 0, 1);
  }
}

inline int randChoice(Prng& prng, const int* values, std::size_t count) {
  return values[static_cast<std::size_t>(std::floor(static_cast<double>(count) * prng.next()))];
}

inline double weightedRandom(Prng& prng, double (*accept)(double)) {
  const double x = prng.next();
  const double y = prng.next();
  if (y < accept(x)) {
    return x;
  }
  return weightedRandom(prng, accept);
}

inline double gaussianPeak(double x) {
  return std::exp(-24.0 * std::pow(x - 0.5, 2));
}

inline double randGaussian(Prng& prng) {
  return weightedRandom(prng, gaussianPeak) * 2.0 - 1.0;
}
