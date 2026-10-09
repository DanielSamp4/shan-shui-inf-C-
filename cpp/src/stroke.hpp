#pragma once

#include "geom.hpp"
#include "noise.hpp"
#include "prng.hpp"

#include <functional>
#include <vector>

struct StrokeStyle {
  double width = 2;
  double noise = 0.5;
  double xOffset = 0;
  double yOffset = 0;
  std::function<double(double)> widthFn;
};

// Closed ribbon around the polyline, same construction as the page's stroke().
std::vector<Vec2> strokeRibbon(const std::vector<Vec2>& points, Prng& prng, Noise& noise,
                               const StrokeStyle& style = {});

// Even stroke used by polyline marks. Does not consume the random generator.
std::vector<Vec2> solidRibbon(const std::vector<Vec2>& points, double width);
