#pragma once

#include "geom.hpp"
#include "noise.hpp"
#include "prng.hpp"

#include <vector>

struct BlobStyle {
  double length = 20;
  double width = 5;
  double angle = 0;
  double noise = 0.5;
  double (*shape)(double) = nullptr;
};

std::vector<Vec2> blobPolygon(double x, double y, Prng& prng, Noise& noise, const BlobStyle& style = {});
