#include "noise.hpp"

#include <cmath>
#include <cstdint>

namespace {

double scaledCosine(double i) {
  return 0.5 * (1.0 - std::cos(i * 3.14159265358979323846));
}

}  // namespace

Noise::Noise(Prng& prng) : prng_(&prng) {}

void Noise::ensureTable() {
  if (ready_) {
    return;
  }
  ready_ = true;
  for (int i = 0; i < kSize + 1; ++i) {
    table_[i] = prng_->next();
  }
}

double Noise::sample(double x, double y, double z) {
  ensureTable();
  if (x < 0.0) {
    x = -x;
  }
  if (y < 0.0) {
    y = -y;
  }
  if (z < 0.0) {
    z = -z;
  }

  int32_t xi = static_cast<int32_t>(std::floor(x));
  int32_t yi = static_cast<int32_t>(std::floor(y));
  int32_t zi = static_cast<int32_t>(std::floor(z));
  double xf = x - xi;
  double yf = y - yi;
  double zf = z - zi;

  double result = 0.0;
  double amplitude = 0.5;
  for (int octave = 0; octave < octaves_; ++octave) {
    int32_t offset = xi + (yi << 4) + (zi << 8);
    const double rxf = scaledCosine(xf);
    const double ryf = scaledCosine(yf);

    double n1 = table_[offset & kSize];
    n1 += rxf * (table_[(offset + 1) & kSize] - n1);
    double n2 = table_[(offset + kYWrap) & kSize];
    n2 += rxf * (table_[(offset + kYWrap + 1) & kSize] - n2);
    n1 += ryf * (n2 - n1);

    offset += kZWrap;
    n2 = table_[offset & kSize];
    n2 += rxf * (table_[(offset + 1) & kSize] - n2);
    double n3 = table_[(offset + kYWrap) & kSize];
    n3 += rxf * (table_[(offset + kYWrap + 1) & kSize] - n3);
    n2 += ryf * (n3 - n2);
    n1 += scaledCosine(zf) * (n2 - n1);

    result += n1 * amplitude;
    amplitude *= falloff_;
    xi <<= 1;
    xf *= 2.0;
    yi <<= 1;
    yf *= 2.0;
    zi <<= 1;
    zf *= 2.0;
    if (xf >= 1.0) {
      xi++;
      xf--;
    }
    if (yf >= 1.0) {
      yi++;
      yf--;
    }
    if (zf >= 1.0) {
      zi++;
      zf--;
    }
  }
  return result;
}
