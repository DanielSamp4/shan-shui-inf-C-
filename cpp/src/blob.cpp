#include "blob.hpp"

#include "randutil.hpp"

#include <cmath>

namespace {

double defaultBlobShape(double p) {
  constexpr double kPi = 3.14159265358979323846;
  if (p <= 1.0) {
    return std::pow(std::sin(p * kPi), 0.5);
  }
  return -std::pow(std::sin((p + 1.0) * kPi), 0.5);
}

}  // namespace

std::vector<Vec2> blobPolygon(double x, double y, Prng& prng, Noise& noise, const BlobStyle& style) {
  constexpr double kResolution = 20.0;
  const auto shape = style.shape ? style.shape : defaultBlobShape;
  std::vector<Vec2> samples;
  samples.reserve(21);
  for (int i = 0; i < 21; ++i) {
    const double p = (static_cast<double>(i) / kResolution) * 2.0;
    const double xo = style.length / 2.0 - std::abs(p - 1.0) * style.length;
    const double yo = (shape(p) * style.width) / 2.0;
    const double angle = std::atan2(yo, xo);
    const double length = std::sqrt(xo * xo + yo * yo);
    samples.push_back({length, angle});
  }

  std::vector<double> noiseValues;
  const double n0 = prng.next() * 10.0;
  for (int i = 0; i < 21; ++i) {
    noiseValues.push_back(noise.sample(i * 0.05, n0));
  }
  loopNoise(noiseValues);

  std::vector<Vec2> polygon;
  for (std::size_t i = 0; i < samples.size(); ++i) {
    const double scale = noiseValues[i] * style.noise + (1.0 - style.noise);
    polygon.push_back({x + std::cos(samples[i].y + style.angle) * samples[i].x * scale,
                       y + std::sin(samples[i].y + style.angle) * samples[i].x * scale});
  }
  return polygon;
}
