#include "scenery.hpp"

#include "blob.hpp"
#include "noise.hpp"
#include "prng.hpp"
#include "randutil.hpp"
#include "stroke.hpp"

#include <algorithm>
#include <cmath>
#include <vector>

namespace {

double textureSpread(Prng& prng) {
  if (prng.next() > 0.5) {
    return (1.0 / 3.0) * prng.next();
  }
  return (2.0 / 3.0) + (1.0 / 3.0) * prng.next();
}

}  // namespace

std::vector<std::vector<Vec2>> textureCenters(const std::vector<std::vector<Vec2>>& layers, int count, Prng& prng,
                                             Noise& noise, double (*spread)(Prng&), double span, double wobble) {
  std::vector<std::vector<Vec2>> lines(static_cast<std::size_t>(count));
  if (layers.empty() || layers.front().empty()) {
    return lines;
  }
  const int rows = static_cast<int>(layers.size());
  const int cols = static_cast<int>(layers.front().size());
  for (int i = 0; i < count; ++i) {
    const auto pick = spread != nullptr ? spread : textureSpread;
    const int mid = static_cast<int>(pick(prng) * cols);
    const int half = static_cast<int>(std::floor(prng.next() * (cols * span)));
    const int start = std::min(std::max(mid - half, 0), cols);
    const int end = std::min(std::max(mid + half, 0), cols);
    const double layer = (static_cast<double>(i) / count) * (rows - 1);
    const int low = static_cast<int>(std::floor(layer));
    const int high = static_cast<int>(std::ceil(layer));
    const double blend = layer - low;
    const double sway = wobble >= 0.0 ? wobble : 30.0 / (layer + 1.0);
    for (int j = start; j < end; ++j) {
      const Vec2& a = layers[static_cast<std::size_t>(low)][static_cast<std::size_t>(j)];
      const Vec2& b = layers[static_cast<std::size_t>(high)][static_cast<std::size_t>(j)];
      const double px = a.x * blend + b.x * (1.0 - blend);
      const double py = a.y * blend + b.y * (1.0 - blend);
      lines[static_cast<std::size_t>(i)].push_back(
          {px + sway * (noise.sample(px, j * 0.5) - 0.5), py + sway * (noise.sample(py, j * 0.5) - 0.5)});
    }
  }
  return lines;
}

namespace {

constexpr double kPi = 3.14159265358979323846;

Rgba gray(double alpha) {
  return {100, 100, 100, alpha};
}

double shrubShape(double p) {
  if (p <= 1.0) {
    return std::pow(std::sin(p * kPi) * p, 0.5);
  }
  return -std::pow(std::sin((p - 2.0) * kPi * (p - 2.0)), 0.5);
}

double boatWidth(double t) {
  return std::sin(t * kPi * 2.0);
}

struct Ridge {
  std::vector<std::vector<Vec2>> layers;
  double height = 0;
};

Ridge buildRidge(double yOffset, double seed, Prng& prng, Noise& noise, double height, double width) {
  Ridge ridge;
  ridge.height = height;
  double hoff = 0;
  for (int j = 0; j < 10; ++j) {
    hoff += (prng.next() * yOffset) / 100.0;
    std::vector<Vec2> layer;
    for (int i = 0; i < 50; ++i) {
      const double x = (static_cast<double>(i) / 50.0 - 0.5) * kPi;
      double y = std::cos(x);
      y *= noise.sample(x + 10.0, j * 0.15, seed);
      const double p = 1.0 - static_cast<double>(j) / 10.0;
      layer.push_back({(x / kPi) * width * p, -y * height * p + hoff});
    }
    ridge.layers.push_back(layer);
  }
  return ridge;
}

void addRibbon(std::vector<Ink>& inks, const std::vector<Vec2>& center, Prng& prng, Noise& noise, StrokeStyle style,
               Rgba color) {
  if (center.empty()) {
    return;
  }
  Ink ink;
  ink.polygon = strokeRibbon(center, prng, noise, style);
  ink.color = color;
  inks.push_back(ink);
}

double fixed3(double value) {
  return std::round(value * 1000.0) / 1000.0;
}

double fixed1(double value) {
  return std::round(value * 10.0) / 10.0;
}

double unitWidth(double) {
  return 1.0;
}

double halfWidth(double) {
  return 0.5;
}

Rgba mountainHatch(Prng& prng) {
  return gray(fixed3(prng.next() * 0.3));
}

Rgba rockHatch(Prng& prng) {
  return {180, 180, 180, fixed3(0.3 + prng.next() * 0.3)};
}

double rockSpread(Prng& prng) {
  if (prng.next() > 0.5) {
    return 0.15 + 0.15 * prng.next();
  }
  return 0.85 - 0.15 * prng.next();
}

std::vector<Vec2> subdivide(const std::vector<Vec2>& points, int resolution) {
  std::vector<Vec2> result;
  if (points.empty() || resolution <= 0) {
    return result;
  }
  const int span = (static_cast<int>(points.size()) - 1) * resolution;
  for (int i = 0; i < span; ++i) {
    const int last = i / resolution;
    const int next = static_cast<int>(std::ceil(static_cast<double>(i) / resolution));
    const double p = static_cast<double>(i % resolution) / resolution;
    const Vec2& a = points[static_cast<std::size_t>(last)];
    const Vec2& b = points[static_cast<std::size_t>(next)];
    result.push_back({a.x * (1.0 - p) + b.x * p, a.y * (1.0 - p) + b.y * p});
  }
  result.push_back(points.back());
  return result;
}

void addMovedRibbon(std::vector<Ink>& inks, const std::vector<Vec2>& local, double x, double y, StrokeStyle style,
                    Rgba color, Prng& prng, Noise& noise) {
  std::vector<Vec2> moved;
  moved.reserve(local.size());
  for (const Vec2& point : local) {
    moved.push_back({point.x + x, point.y + y});
  }
  addRibbon(inks, moved, prng, noise, style, color);
}

void addSolid(std::vector<Ink>& inks, const std::vector<Vec2>& points, double width, Rgba color) {
  Ink ink;
  ink.polygon = solidRibbon(points, width);
  ink.color = color;
  if (ink.polygon.size() >= 3) {
    inks.push_back(std::move(ink));
  }
}

void appendTexture(std::vector<Ink>& inks, const std::vector<std::vector<Vec2>>& layers, double x, double y, int count,
                   int shade, double hatchWidth, Rgba (*colorFor)(Prng&), double (*spread)(Prng&), Prng& prng,
                   Noise& noise, double span = 0.2, double wobble = -1) {
  const auto centers = textureCenters(layers, count, prng, noise, spread, span, wobble);
  if (shade != 0) {
    StrokeStyle style;
    style.width = shade;
    for (int j = 0; j < count; j += 2) {
      addMovedRibbon(inks, centers[static_cast<std::size_t>(j)], x, y, style, gray(0.1), prng, noise);
    }
  }
  StrokeStyle style;
  style.width = hatchWidth;
  for (int j = shade; j < count; j += 1 + shade) {
    addMovedRibbon(inks, centers[static_cast<std::size_t>(j)], x, y, style, colorFor(prng), prng, noise);
  }
}

void strokePolylines(std::vector<Ink>& inks, const std::vector<std::vector<Vec2>>& lines, double x, double y,
                     double width, double noiseAmount, double (*widthFn)(double), Rgba color, Prng& prng, Noise& noise) {
  StrokeStyle style;
  style.width = width;
  style.noise = noiseAmount;
  style.widthFn = widthFn;
  for (const auto& line : lines) {
    addMovedRibbon(inks, line, x, y, style, color, prng, noise);
  }
}

std::vector<std::vector<Vec2>> buildFoot(const std::vector<std::vector<Vec2>>& layers, double xOffset, Prng& prng,
                                        Noise& noise) {
  std::vector<std::vector<Vec2>> lines;
  if (layers.size() < 3) {
    return lines;
  }
  const int choices[] = {1, 2};
  int next = 0;
  const int lastLayer = static_cast<int>(layers.size()) - 2;
  for (int i = 0; i < lastLayer; ++i) {
    if (i != next) {
      continue;
    }
    next = std::min(next + randChoice(prng, choices, 2), static_cast<int>(layers.size()) - 1);
    const auto& layer = layers[static_cast<std::size_t>(i)];
    const auto& far = layers[static_cast<std::size_t>(next)];
    std::vector<Vec2> left;
    std::vector<Vec2> right;
    const double steps = std::min(static_cast<double>(layer.size()) / 8.0, 10.0);
    for (int j = 0; j < steps; ++j) {
      const Vec2& west = layer[static_cast<std::size_t>(j)];
      const Vec2& east = layer[layer.size() - 1 - static_cast<std::size_t>(j)];
      const double nudge = noise.sample(j * 0.1, i) * 10.0;
      left.push_back({west.x + nudge, west.y});
      right.push_back({east.x - nudge, east.y});
    }
    std::reverse(left.begin(), left.end());
    std::reverse(right.begin(), right.end());
    for (int j = 0; j < 10; ++j) {
      const double p = j / 10.0;
      const double vib = -1.7 * (p - 1.0) * std::pow(p, 0.2);
      const double sway = noise.sample(xOffset * 0.05, i) * 5.0;
      left.push_back({layer.front().x * (1.0 - p) + far.front().x * p,
                      layer.front().y * (1.0 - p) + far.front().y * p + vib * 5.0 + sway});
      right.push_back({layer.back().x * (1.0 - p) + far.back().x * p,
                       layer.back().y * (1.0 - p) + far.back().y * p + vib * 5.0 +
                           noise.sample(xOffset * 0.05, i) * 5.0});
    }
    lines.push_back(std::move(left));
    lines.push_back(std::move(right));
  }
  return lines;
}

void addFoot(std::vector<Ink>& inks, const std::vector<std::vector<Vec2>>& layers, double x, double y, Prng& prng,
             Noise& noise) {
  const auto lines = buildFoot(layers, x, prng, noise);
  for (const auto& line : lines) {
    Ink fill;
    fill.color = {255, 255, 255, 1};
    for (const Vec2& point : line) {
      fill.polygon.push_back({point.x + x, point.y + y});
    }
    inks.push_back(std::move(fill));
  }
  StrokeStyle style;
  style.width = 1;
  for (const auto& line : lines) {
    addMovedRibbon(inks, line, x, y, style, gray(fixed3(0.1 + prng.next() * 0.1)), prng, noise);
  }
}

std::vector<Ink> tree01(double x, double y, double height, double width, double alpha, Prng& prng, Noise& noise) {
  constexpr int resolution = 10;
  std::vector<std::vector<double>> field(resolution, std::vector<double>(2));
  for (int i = 0; i < resolution; ++i) {
    field[i][0] = noise.sample(i * 0.5);
    field[i][1] = noise.sample(i * 0.5, 0.5);
  }
  std::vector<Ink> inks;
  std::vector<Vec2> left;
  std::vector<Vec2> right;
  for (int i = 0; i < resolution; ++i) {
    const double ny = y - (i * height) / resolution;
    if (i >= resolution / 4.0) {
      const double limit = static_cast<double>(resolution - i) / 5.0;
      for (int j = 0; j < limit; ++j) {
        const double bx = x + (prng.next() - 0.5) * width * 1.2 * (resolution - i);
        const double by = ny + (prng.next() - 0.5) * width;
        BlobStyle style;
        style.length = prng.next() * 20.0 * (resolution - i) * 0.2 + 10.0;
        style.width = prng.next() * 6.0 + 3.0;
        style.angle = ((prng.next() - 0.5) * kPi) / 6.0;
        const double leaf = fixed1(prng.next() * 0.2 + alpha);
        Ink ink;
        ink.polygon = blobPolygon(bx, by, prng, noise, style);
        ink.color = gray(leaf);
        inks.push_back(std::move(ink));
      }
    }
    left.push_back({x + (field[i][0] - 0.5) * width - width / 2.0, ny});
    right.push_back({x + (field[i][1] - 0.5) * width + width / 2.0, ny});
  }
  addSolid(inks, left, 1.5, gray(alpha));
  addSolid(inks, right, 1.5, gray(alpha));
  return inks;
}

std::vector<Ink> tree03(double x, double y, double height, double bend, double alpha, Prng& prng, Noise& noise) {
  constexpr int resolution = 10;
  constexpr double width = 5.0;
  std::vector<std::vector<double>> field(resolution, std::vector<double>(2));
  for (int i = 0; i < resolution; ++i) {
    field[i][0] = noise.sample(i * 0.5);
    field[i][1] = noise.sample(i * 0.5, 0.5);
  }
  const int sides[] = {-1, 1};
  std::vector<Ink> leaves;
  std::vector<Vec2> left;
  std::vector<Vec2> right;
  for (int i = 0; i < resolution; ++i) {
    const double nx = x + (static_cast<double>(i) / resolution) * bend * 100.0;
    const double ny = y - (i * height) / resolution;
    if (i >= resolution / 5) {
      const double shape = std::log(50.0 * (resolution - i) / static_cast<double>(resolution) + 1.0) / 3.95;
      for (int j = 0; j < (resolution - i) * 2; ++j) {
        const double reach = prng.next() * width * 2.0 * shape;
        const int side = randChoice(prng, sides, 2);
        const double by = ny + (prng.next() - 0.5) * width * 2.0;
        BlobStyle style;
        style.length = reach * 2.0;
        style.width = prng.next() * 6.0 + 3.0;
        style.angle = ((prng.next() - 0.5) * kPi) / 6.0;
        const double leaf = fixed3(prng.next() * 0.2 + alpha);
        Ink ink;
        ink.polygon = blobPolygon(nx + reach * side, by, prng, noise, style);
        ink.color = gray(leaf);
        leaves.push_back(std::move(ink));
      }
    }
    const double taper = static_cast<double>(resolution - i) / resolution;
    left.push_back({nx + ((field[i][0] - 0.5) * width - width / 2.0) * taper, ny});
    right.push_back({nx + ((field[i][1] - 0.5) * width + width / 2.0) * taper, ny});
  }
  std::vector<Ink> inks;
  Ink canopy;
  canopy.color = {255, 255, 255, 1};
  canopy.polygon = left;
  canopy.polygon.insert(canopy.polygon.end(), right.rbegin(), right.rend());
  inks.push_back(std::move(canopy));
  addSolid(inks, inks.front().polygon, 1.5, gray(alpha));
  inks.insert(inks.end(), leaves.begin(), leaves.end());
  return inks;
}

std::vector<std::vector<Vec2>> decorate(int style, Vec2 pul, Vec2 pur, Vec2 pdl, Vec2 pdr, int h0, int h1, int v0,
                                       int v1) {
  const auto left = subdivide({pul, pdl}, v1);
  const auto right = subdivide({pur, pdr}, v1);
  const auto top = subdivide({pul, pur}, h1);
  const auto bottom = subdivide({pdl, pdr}, h1);
  std::vector<std::vector<Vec2>> lines;
  auto vertical = [&](int slot) {
    const Vec2 upper = top[static_cast<std::size_t>(slot)];
    const Vec2 lower = bottom[static_cast<std::size_t>(slot)];
    return subdivide({upper, lower}, 5);
  };
  if (style == 1 || style == 3) {
    const int rightSlot = static_cast<int>(top.size()) - 1 - h0;
    const auto innerLeft = subdivide({top[static_cast<std::size_t>(h0)], bottom[static_cast<std::size_t>(h0)]}, v1);
    const auto innerRight =
        subdivide({top[static_cast<std::size_t>(rightSlot)], bottom[static_cast<std::size_t>(rightSlot)]}, v1);
    for (int i = v0; i < static_cast<int>(left.size()) - v0; i += v0) {
      if (style == 1) {
        lines.push_back(subdivide({innerLeft[static_cast<std::size_t>(i)], left[static_cast<std::size_t>(i)]}, 5));
        lines.push_back(subdivide({innerRight[static_cast<std::size_t>(i)], right[static_cast<std::size_t>(i)]}, 5));
      } else {
        const auto acrossTop = subdivide(
            {top[static_cast<std::size_t>(h0)], top[static_cast<std::size_t>(rightSlot)]}, v1);
        const auto acrossBottom = subdivide(
            {bottom[static_cast<std::size_t>(h0)], bottom[static_cast<std::size_t>(rightSlot)]}, v1);
        lines.push_back(
            subdivide({innerLeft[static_cast<std::size_t>(i)], innerRight[static_cast<std::size_t>(i)]}, 5));
        lines.push_back(
            subdivide({acrossTop[static_cast<std::size_t>(i)], acrossBottom[static_cast<std::size_t>(i)]}, 5));
      }
    }
    lines.push_back(vertical(h0));
    lines.push_back(vertical(rightSlot));
  } else if (style == 2) {
    for (int i = h0; i < static_cast<int>(top.size()) - h0; i += h0) {
      lines.push_back(vertical(i));
    }
  }
  return lines;
}

void addBox(std::vector<Ink>& inks, double x, double y, double height, double width, double rotation, double perspective,
            bool transparent, double weight, int style, const int* horizontal, const int* vertical, Prng& prng,
            Noise& noise) {
  const double mid = -width * 0.5 + width * rotation;
  const double back = -width * 0.5 + width * (1.0 - rotation);
  std::vector<std::vector<Vec2>> lines;
  lines.push_back(subdivide({{-width * 0.5, -height}, {-width * 0.5, 0}}, 5));
  lines.push_back(subdivide({{width * 0.5, -height}, {width * 0.5, 0}}, 5));
  lines.push_back(subdivide({{-width * 0.5, 0}, {mid, perspective}}, 5));
  lines.push_back(subdivide({{width * 0.5, 0}, {mid, perspective}}, 5));
  lines.push_back(subdivide({{mid, -height}, {mid, perspective}}, 5));
  if (transparent) {
    lines.push_back(subdivide({{-width * 0.5, 0}, {back, -perspective}}, 5));
    lines.push_back(subdivide({{width * 0.5, 0}, {back, -perspective}}, 5));
    lines.push_back(subdivide({{back, -height}, {back, -perspective}}, 5));
  }
  if (style != 0) {
    const double facing = rotation < 0.5 ? 1.0 : -1.0;
    auto decor = decorate(style, {facing * width * 0.5, -height}, {mid, -height + perspective},
                          {facing * width * 0.5, 0}, {mid, perspective}, horizontal[0], horizontal[1], vertical[0],
                          vertical[1]);
    lines.insert(lines.end(), decor.begin(), decor.end());
  }
  if (!transparent) {
    Ink wall;
    wall.color = {255, 255, 255, 1};
    for (const Vec2& point : std::vector<Vec2>{{-width * 0.5, -height},
                                               {width * 0.5, -height},
                                               {width * 0.5, 0},
                                               {mid, perspective},
                                               {-width * 0.5, 0}}) {
      wall.polygon.push_back({point.x + x, point.y + y});
    }
    inks.push_back(std::move(wall));
  }
  strokePolylines(inks, lines, x, y, weight, 1, unitWidth, gray(0.4), prng, noise);
}

void addRail(std::vector<Ink>& inks, double x, double y, double seed, double height, double width, double rotation,
             double perspective, int segments, double weight, bool transparent, Prng& prng, Noise& noise,
             bool front = true) {
  const double mid = -width * 0.5 + width * rotation;
  const double back = -width * 0.5 + width * (1.0 - rotation);
  std::vector<std::vector<Vec2>> lines;
  if (front) {
    lines.push_back(subdivide({{-width * 0.5, 0}, {mid, perspective}}, segments));
    lines.push_back(subdivide({{mid, perspective}, {width * 0.5, 0}}, segments));
  }
  if (transparent) {
    lines.push_back(subdivide({{-width * 0.5, 0}, {back, -perspective}}, segments));
    lines.push_back(subdivide({{back, -perspective}, {width * 0.5, 0}}, segments));
  }
  if (front) {
    lines.push_back(subdivide({{-width * 0.5, -height}, {mid, -height + perspective}}, segments));
    lines.push_back(subdivide({{mid, -height + perspective}, {width * 0.5, -height}}, segments));
  }
  if (transparent) {
    lines.push_back(subdivide({{-width * 0.5, -height}, {back, -height - perspective}}, segments));
    lines.push_back(subdivide({{back, -height - perspective}, {width * 0.5, -height}}, segments));
  }
  if (transparent && !lines.empty()) {
    const int open = static_cast<int>(std::floor(prng.next() * static_cast<double>(lines.size())));
    auto& line = lines[static_cast<std::size_t>(open)];
    if (!line.empty()) {
      line.pop_back();
    }
    if (!line.empty()) {
      line.pop_back();
    }
  }
  const int half = static_cast<int>(lines.size()) / 2;
  for (int i = 0; i < half; ++i) {
    auto& lower = lines[static_cast<std::size_t>(i)];
    auto& upper = lines[static_cast<std::size_t>((half + i) % static_cast<int>(lines.size()))];
    if (upper.empty()) {
      continue;
    }
    for (int j = 0; j < static_cast<int>(lower.size()); ++j) {
      lower[static_cast<std::size_t>(j)].y += (noise.sample(i, j * 0.5, seed) - 0.5) * height;
      const int paired = j % static_cast<int>(upper.size());
      upper[static_cast<std::size_t>(paired)].y += (noise.sample(i + 0.5, j * 0.5, seed) - 0.5) * height;
      auto post = subdivide({lower[static_cast<std::size_t>(j)], upper[static_cast<std::size_t>(paired)]}, 2);
      post[0].x += (prng.next() - 0.5) * height * 0.5;
      std::vector<Vec2> placed;
      for (const Vec2& point : post) {
        placed.push_back({point.x + x, point.y + y});
      }
      addSolid(inks, placed, 2, gray(0.5));
    }
  }
  strokePolylines(inks, lines, x, y, weight, 0.5, unitWidth, gray(0.5), prng, noise);
}

void flipHorizontal(std::vector<Vec2>& points) {
  for (Vec2& point : points) {
    point.x = -point.x;
  }
}

void addRoof(std::vector<Ink>& inks, double x, double y, double height, double width, double rotation, double perspective,
             double weight, Prng& prng, Noise& noise) {
  const bool mirrored = rotation < 0.5;
  const double facing = mirrored ? 1.0 - rotation : rotation;
  const double mid = -width * 0.5 + width * facing;
  const double quat = (mid + width * 0.5) * 0.5 - mid;
  constexpr double corbel = 5;
  std::vector<std::vector<Vec2>> lines = {
      subdivide({{-width * 0.5 + quat, -height - perspective / 2.0},
                 {-width * 0.5 + quat * 0.5, -height / 2.0 - perspective / 4.0},
                 {-width * 0.5 - corbel, 0}},
                5),
      subdivide({{mid + quat, -height}, {(mid + quat + width * 0.5) / 2.0, -height / 2.0}, {width * 0.5 + corbel, 0}},
                5),
      subdivide({{mid + quat, -height}, {mid + quat / 2.0, -height / 2.0 + perspective / 2.0}, {mid + corbel, perspective}},
                5),
      subdivide({{-width * 0.5 - corbel, 0}, {mid + corbel, perspective}}, 5),
      subdivide({{width * 0.5 + corbel, 0}, {mid + corbel, perspective}}, 5),
      subdivide({{-width * 0.5 + quat, -height - perspective / 2.0}, {mid + quat, -height}}, 5),
  };
  std::vector<Vec2> cap = {{-width * 0.5, 0},
                           {-width * 0.5 + quat, -height - perspective / 2.0},
                           {mid + quat, -height},
                           {width * 0.5, 0},
                           {mid, perspective}};
  if (mirrored) {
    for (auto& line : lines) {
      flipHorizontal(line);
    }
    flipHorizontal(cap);
  }
  Ink fill;
  fill.color = {255, 255, 255, 1};
  for (const Vec2& point : cap) {
    fill.polygon.push_back({point.x + x, point.y + y});
  }
  inks.push_back(std::move(fill));
  strokePolylines(inks, lines, x, y, weight, 1, unitWidth, gray(0.4), prng, noise);
}

void addPagodaRoof(std::vector<Ink>& inks, double x, double y, double height, double width, double perspective,
                   double weight, Prng& prng, Noise& noise) {
  constexpr int sides = 4;
  constexpr double corbel = 10;
  std::vector<std::vector<Vec2>> lines;
  std::vector<Vec2> cap = {{0, -height}};
  Vec2 previous;
  bool hasPrevious = false;
  for (int i = 0; i < sides; ++i) {
    const double t = static_cast<double>(i) / (sides - 1) - 0.5;
    const double fx = width * t;
    const double fy = perspective * (1.0 - std::abs(t) * 2.0);
    const double fxx = (width + corbel) * t;
    if (hasPrevious) {
      lines.push_back(subdivide({previous, {fxx, fy}}, 5));
    }
    lines.push_back(subdivide({{0, -height}, {fx * 0.5, (-height + fy) * 0.5}, {fxx, fy}}, 5));
    cap.push_back({fxx, fy});
    previous = {fxx, fy};
    hasPrevious = true;
  }
  Ink fill;
  fill.color = {255, 255, 255, 1};
  for (const Vec2& point : cap) {
    fill.polygon.push_back({point.x + x, point.y + y});
  }
  inks.push_back(std::move(fill));
  strokePolylines(inks, lines, x, y, weight, 1, unitWidth, gray(0.4), prng, noise);
}

void addHouse(std::vector<Ink>& inks, double x, double y, double seed, Prng& prng, Noise& noise) {
  (void)seed;
  const int kinds[] = {0, 0, 1, 1, 1, 2};
  const int kind = randChoice(prng, kinds, 6);
  if (kind == 0) {
    return;
  }
  if (kind == 1) {
    const int storiesChoices[] = {1, 2, 2, 3};
    const int styleChoices[] = {1, 2, 3};
    const double width = 40.0 + prng.next() * 30.0;
    const int stories = randChoice(prng, storiesChoices, 4);
    const double rotation = prng.next();
    const int style = randChoice(prng, styleChoices, 3);
    const int horizontal[][2] = {{0, 0}, {1, 5}, {1, 5}, {1, 4}};
    const int vertical[][2] = {{0, 0}, {1, 2}, {1, 2}, {1, 3}};
    double rise = 0;
    for (int story = 0; story < stories; ++story) {
      const double floorWidth = width * std::pow(0.85, story);
      addBox(inks, x, y - rise, 10, floorWidth, rotation, 5, false, 1.5, style, horizontal[style], vertical[style],
             prng, noise);
      if (stories == 1 && prng.next() < 1.0 / 3.0) {
        // The page may stamp a sign here. The ink is text, so only the coin flip is kept.
      }
      addRoof(inks, x, y - rise - 10, 10, width * std::pow(0.9, story), rotation, 5, 1.5, prng, noise);
      rise += 15;
    }
    return;
  }
  const int storiesChoices[] = {1, 1, 1, 2, 2};
  const int stories = randChoice(prng, storiesChoices, 5);
  double rise = 0;
  for (int story = 0; story < stories; ++story) {
    const double floorWidth = 30.0 * std::pow(0.85, story);
    addBox(inks, x, y - rise, 15, floorWidth, 0.7, 2.5, true, 1.5, 0, nullptr, nullptr, prng, noise);
    addRail(inks, x, y - rise, story * 0.2, 5, floorWidth * 1.2, 0.7, 2.5, 3, 0.5, true, prng, noise);
    addPagodaRoof(inks, x, y - rise - 15, 15, 30.0 * std::pow(0.9, story), 5, 1.5, prng, noise);
    rise += 18;
  }
}

void addPagoda(std::vector<Ink>& inks, double x, double y, Prng& prng, Noise& noise) {
  const int storiesChoices[] = {5, 7};
  const int stories = randChoice(prng, storiesChoices, 2);
  const double width = 40.0 + prng.next() * 20.0;
  const int horizontal[] = {1, 4};
  const int vertical[] = {1, 2};
  double rise = 0;
  for (int story = 0; story < stories; ++story) {
    const double floorWidth = width * std::pow(0.85, story);
    addBox(inks, x, y - rise, 10, floorWidth, 0.7, 2.5, false, 1.5, 1, horizontal, vertical, prng, noise);
    addRail(inks, x, y - rise, story * 0.2, 5, floorWidth * 1.1, 0.7, 2.5, 5, 0.5, false, prng, noise);
    addPagodaRoof(inks, x, y - rise - 10, 15, width * std::pow(0.9, story), 5, 1.5, prng, noise);
    rise += 15;
  }
}

void addTower(std::vector<Ink>& inks, double x, double y, Prng& prng, Noise& noise) {
  constexpr double height = 100;
  constexpr double width = 20;
  const Vec2 topLeft{-width * 0.05, -height};
  const Vec2 topRight{width * 0.05, -height};
  const Vec2 upperLeft{-width * 0.1, -height * 0.9};
  const Vec2 upperRight{width * 0.1, -height * 0.9};
  const Vec2 middleLeft{-width * 0.2, -height * 0.5};
  const Vec2 middleRight{width * 0.2, -height * 0.5};
  const Vec2 footLeft{-width * 0.5, 0};
  const Vec2 footRight{width * 0.5, 0};
  const double arms[][2] = {{0.7, -0.85}, {1, -0.675}, {0.7, -0.5}};
  StrokeStyle style;
  style.width = 1;
  style.widthFn = halfWidth;
  auto mark = [&](const std::vector<Vec2>& points) {
    addMovedRibbon(inks, subdivide(points, 5), x, y, style, gray(0.4), prng, noise);
  };
  for (const auto& arm : arms) {
    const Vec2 left{-arm[0] * width, arm[1] * height};
    const Vec2 right{arm[0] * width, arm[1] * height};
    const Vec2 center{0, (arm[1] - 0.05) * height};
    mark({left, right});
    mark({left, center});
    mark({right, center});
    mark({left, {-arm[0] * width, (arm[1] + 0.1) * height}});
    mark({right, {arm[0] * width, (arm[1] + 0.1) * height}});
  }
  const auto leftLeg = subdivide({topLeft, upperLeft, middleLeft, footLeft}, 5);
  const auto rightLeg = subdivide({topRight, upperRight, middleRight, footRight}, 5);
  for (std::size_t i = 0; i + 1 < leftLeg.size(); ++i) {
    mark({leftLeg[i], rightLeg[i + 1]});
    mark({rightLeg[i], leftLeg[i + 1]});
  }
  mark({topLeft, topRight});
  mark({upperLeft, upperRight});
  mark({middleLeft, middleRight});
  mark({topLeft, upperLeft, middleLeft, footLeft});
  mark({topRight, upperRight, middleRight, footRight});
}

void addRock(std::vector<Ink>& inks, double x, double y, double seed, double width, double height, Prng& prng,
             Noise& noise, int shade = 2) {
  constexpr int rows = 10;
  constexpr int cols = 50;
  std::vector<std::vector<Vec2>> layers(rows);
  for (int i = 0; i < rows; ++i) {
    std::vector<double> field;
    field.reserve(cols);
    for (int j = 0; j < cols; ++j) {
      field.push_back(noise.sample(i, j * 0.2, seed));
    }
    loopNoise(field);
    for (int j = 0; j < cols; ++j) {
      const double angle = (static_cast<double>(j) / cols) * kPi * 2.0 - kPi / 2.0;
      double radius = (width * height) /
                      std::sqrt(std::pow(height * std::cos(angle), 2) + std::pow(width * std::sin(angle), 2));
      radius *= 0.7 + 0.3 * field[static_cast<std::size_t>(j)];
      const double scale = 1.0 - static_cast<double>(i) / rows;
      double nx = std::cos(angle) * radius * scale;
      double ny = -std::sin(angle) * radius * scale;
      if (kPi < angle || angle < 0) {
        ny *= 0.2;
      }
      ny += height * (static_cast<double>(i) / rows) * 0.2;
      layers[static_cast<std::size_t>(i)].push_back({nx, ny});
    }
  }
  Ink body;
  body.color = {255, 255, 255, 1};
  for (const Vec2& point : layers[0]) {
    body.polygon.push_back({point.x + x, point.y + y});
  }
  body.polygon.push_back({x, y});
  inks.push_back(std::move(body));
  StrokeStyle style;
  style.width = 3;
  style.noise = 1;
  addMovedRibbon(inks, layers[0], x, y, style, gray(0.3), prng, noise);
  appendTexture(inks, layers, x, y, 40, shade, 3, rockHatch, rockSpread, prng, noise);
}

struct Spot {
  double x = 0;
  double y = 0;
};

void finishMountain(std::vector<Ink>& inks, const std::vector<std::vector<Vec2>>& layers, double x, double y,
                    double seed, double height, Prng& prng, Noise& noise) {
  addFoot(inks, layers, x, y, prng, noise);
  const int shadeChoices[] = {0, 0, 0, 0, 5};
  const int shade = randChoice(prng, shadeChoices, 5);
  appendTexture(inks, layers, x, y, 200, shade, 1.5, mountainHatch, nullptr, prng, noise);

  std::vector<Spot> canopy;
  for (int layer = 0; layer < static_cast<int>(layers.size()); ++layer) {
    for (int index = 0; index < static_cast<int>(layers[static_cast<std::size_t>(layer)].size()); ++index) {
      const Vec2& point = layers[static_cast<std::size_t>(layer)][static_cast<std::size_t>(index)];
      const double cover = noise.sample(layer * 0.1, index * 0.1, seed + 2.0);
      if (cover * cover * cover < 0.1 && std::abs(point.y) / height > 0.5) {
        canopy.push_back({point.x, point.y});
      }
    }
  }
  for (const Spot& spot : canopy) {
    const double alpha = fixed3(noise.sample(0.01 * spot.x, 0.01 * spot.y) * 0.5 * 0.3 + 0.5);
    auto shrubs = tree02(spot.x + x, spot.y + y, prng, noise, 5, 16, 8, alpha);
    inks.insert(inks.end(), shrubs.begin(), shrubs.end());
  }

  std::vector<Spot> pines;
  for (int layer = 0; layer < static_cast<int>(layers.size()); ++layer) {
    for (int index = 0; index < static_cast<int>(layers[static_cast<std::size_t>(layer)].size()); ++index) {
      const Vec2& point = layers[static_cast<std::size_t>(layer)][static_cast<std::size_t>(index)];
      const double cover = noise.sample(layer * 0.2, index * 0.05, seed);
      if (index % 2 != 0 && cover * cover * cover * cover < 0.012 && std::abs(point.y) / height < 0.3) {
        pines.push_back({point.x, point.y});
      }
    }
  }
  for (std::size_t i = 0; i < pines.size(); ++i) {
    int neighbors = 0;
    for (std::size_t j = 0; j < pines.size(); ++j) {
      if (i == j) {
        continue;
      }
      const double dx = pines[i].x - pines[j].x;
      const double dy = pines[i].y - pines[j].y;
      if (dx * dx + dy * dy < 30.0 * 30.0) {
        ++neighbors;
      }
      if (neighbors > 2) {
        break;
      }
    }
    if (neighbors <= 2) {
      continue;
    }
    double treeHeight = ((height + pines[i].y) / height) * 70.0;
    treeHeight = treeHeight * 0.3 + prng.next() * treeHeight * 0.7;
    const double treeWidth = prng.next() * 3.0 + 1.0;
    const double alpha = fixed3(noise.sample(0.01 * pines[i].x, 0.01 * pines[i].y) * 0.5 * 0.3 + 0.3);
    auto crown = tree01(pines[i].x + x, pines[i].y + y, treeHeight, treeWidth, alpha, prng, noise);
    inks.insert(inks.end(), crown.begin(), crown.end());
  }

  for (int layer = 0; layer < static_cast<int>(layers.size()); ++layer) {
    const auto& row = layers[static_cast<std::size_t>(layer)];
    for (int index = 0; index < static_cast<int>(row.size()); ++index) {
      const bool edge = index == 0 || index == static_cast<int>(row.size()) - 1;
      const double cover = noise.sample(layer * 0.2, index * 0.05, seed);
      if (edge && cover * cover * cover * cover < 0.012) {
        const Vec2& point = row[static_cast<std::size_t>(index)];
        double treeHeight = ((height + point.y) / height) * 120.0;
        treeHeight = treeHeight * 0.5 + prng.next() * treeHeight * 0.5;
        const double bend = prng.next() * 0.1;
        const double alpha = fixed3(noise.sample(0.01 * point.x, 0.01 * point.y) * 0.5 * 0.3 + 0.3);
        auto crown = tree03(point.x + x, point.y + y, treeHeight, bend, alpha, prng, noise);
        inks.insert(inks.end(), crown.begin(), crown.end());
      }
    }
  }

  for (int layer = 0; layer < static_cast<int>(layers.size()); ++layer) {
    const auto& row = layers[static_cast<std::size_t>(layer)];
    for (int index = 0; index < static_cast<int>(row.size()); ++index) {
      const bool post = index == 1 || index == static_cast<int>(row.size()) - 2;
      const double cover = noise.sample(layer * 0.2, index * 0.05, seed + 10.0);
      if (layer != 0 && post && cover * cover * cover * cover < 0.008) {
        const Vec2& point = row[static_cast<std::size_t>(index)];
        addHouse(inks, point.x + x, point.y + y, seed, prng, noise);
      }
    }
  }

  const auto& ridge = layers[1];
  const double middle = static_cast<double>(ridge.size()) / 2.0;
  for (int index = 0; index < static_cast<int>(ridge.size()); ++index) {
    if (std::abs(index - middle) < 1.0 && prng.next() < 0.02) {
      const Vec2& point = ridge[static_cast<std::size_t>(index)];
      addPagoda(inks, point.x + x, point.y + y, prng, noise);
    }
  }

  for (int layer = 0; layer < static_cast<int>(layers.size()); ++layer) {
    const auto& row = layers[static_cast<std::size_t>(layer)];
    for (int index = 0; index < static_cast<int>(row.size()); ++index) {
      const bool post = index == 1 || index == static_cast<int>(row.size()) - 2;
      const double cover = noise.sample(layer * 0.2, index * 0.05, seed + 20.0 * kPi);
      if (layer % 2 == 0 && post && cover * cover * cover * cover < 0.002) {
        const Vec2& point = row[static_cast<std::size_t>(index)];
        addTower(inks, point.x + x, point.y + y, prng, noise);
      }
    }
  }

  for (int layer = 0; layer < static_cast<int>(layers.size()); ++layer) {
    const auto& row = layers[static_cast<std::size_t>(layer)];
    for (int index = 0; index < static_cast<int>(row.size()); ++index) {
      if ((index == 0 || index == static_cast<int>(row.size()) - 1) && prng.next() < 0.1) {
        const Vec2& point = row[static_cast<std::size_t>(index)];
        const double rockWidth = 20.0 + prng.next() * 20.0;
        const double rockHeight = 20.0 + prng.next() * 20.0;
        addRock(inks, point.x + x, point.y + y, seed, rockWidth, rockHeight, prng, noise);
      }
    }
  }
}

}  // namespace

std::vector<std::vector<Vec2>> footLines(const std::vector<std::vector<Vec2>>& layers, double xOffset, Prng& prng,
                                        Noise& noise) {
  return buildFoot(layers, xOffset, prng, noise);
}

std::vector<Ink> tree02(double x, double y, Prng& prng, Noise& noise, int clusters, double height, double width,
                       double alpha) {
  std::vector<Ink> inks;
  for (int i = 0; i < clusters; ++i) {
    const double px = x + randGaussian(prng) * clusters * 4.0;
    const double py = y + randGaussian(prng) * clusters * 4.0;
    BlobStyle style;
    style.angle = kPi / 2.0;
    style.shape = shrubShape;
    style.width = prng.next() * width * 0.75 + width * 0.5;
    style.length = prng.next() * height * 0.75 + height * 0.5;
    Ink ink;
    ink.polygon = blobPolygon(px, py, prng, noise, style);
    ink.color = gray(alpha);
    inks.push_back(ink);
  }
  return inks;
}

std::vector<Ink> waterInk(double x, double y, Prng& prng, Noise& noise) {
  constexpr int clusters = 10;
  constexpr double length = 800;
  std::vector<std::vector<Vec2>> groups(clusters);
  double yk = 0;
  for (int i = 0; i < clusters; ++i) {
    const double xk = (prng.next() - 0.5) * (length / 8.0);
    yk += prng.next() * 5.0;
    const double lk = length / 4.0 + prng.next() * (length / 4.0);
    for (double j = -lk; j < lk; j += 5.0) {
      groups[i].push_back({j + xk, std::sin(j * 0.2) * 2.0 * noise.sample(j * 0.1) - 20.0 + yk});
    }
  }

  std::vector<Ink> inks;
  StrokeStyle style;
  style.width = 1;
  for (int group = 1; group < clusters; ++group) {
    std::vector<Vec2> moved;
    for (const Vec2& point : groups[group]) {
      moved.push_back({point.x + x, point.y + y});
    }
    const double alpha = 0.3 + prng.next() * 0.3;
    addRibbon(inks, moved, prng, noise, style, gray(alpha));
  }
  return inks;
}

namespace {
void addPerson(std::vector<Ink>& inks, double x, double y, double scale, bool flip, Prng& prng, Noise& noise,
               const double* lengthOverride = nullptr, bool wideHat = false, bool pole = false);
}

std::vector<Ink> boatHull(double x, double y, double scale, bool flip, Prng* prng, Noise* noise) {
  const double direction = flip ? -1.0 : 1.0;
  std::vector<Ink> inks;
  if (prng != nullptr && noise != nullptr) {
    const double boatLengths[] = {0, 30, 20, 30, 10, 30, 30, 30, 30};
    addPerson(inks, x + 20.0 * scale * direction, y, 0.5 * scale, !flip, *prng, *noise, boatLengths, true, true);
  }
  const double length = 120.0;
  std::vector<Vec2> upper;
  std::vector<Vec2> lower;
  for (double i = 0; i < length * scale; i += 5.0 * scale) {
    const double t = i / length;
    const double rise = std::pow(std::sin(t * kPi), 0.5);
    upper.push_back({i * direction, rise * 7.0 * scale});
    lower.push_back({i * direction, rise * 10.0 * scale});
  }
  std::vector<Vec2> hull = upper;
  for (auto it = lower.rbegin(); it != lower.rend(); ++it) {
    hull.push_back(*it);
  }

  Ink body;
  for (const Vec2& point : hull) {
    body.polygon.push_back({point.x + x, point.y + y});
  }
  body.color = {255, 255, 255, 1};
  inks.push_back(body);

  if (prng != nullptr && noise != nullptr) {
    std::vector<Vec2> center;
    for (const Vec2& point : hull) {
      center.push_back({point.x + x, point.y + y});
    }
    StrokeStyle style;
    style.width = 1;
    style.widthFn = boatWidth;
    addRibbon(inks, center, *prng, *noise, style, gray(0.4));
  }
  return inks;
}

std::vector<Ink> mountainInk(double x, double y, double seed, Prng& prng, Noise& noise, double height, double width) {
  const Ridge ridge = buildRidge(y, seed, prng, noise, height, width);
  std::vector<Ink> inks;

  const auto& crest = ridge.layers[0];
  for (int j = 0; j < 50; ++j) {
    const double ns = noise.sample(j * 0.1, seed);
    if (ns * ns * ns < 0.1 && std::abs(crest[j].y) / height > 0.2) {
      const double shade = noise.sample(0.01 * crest[j].x, 0.01 * crest[j].y) * 0.5 * 0.3 + 0.5;
      auto shrubs = tree02(crest[j].x + x, crest[j].y + y - 5.0, prng, noise, 2, 16, 8, shade);
      inks.insert(inks.end(), shrubs.begin(), shrubs.end());
    }
  }

  Ink body;
  for (const Vec2& point : crest) {
    body.polygon.push_back({point.x + x, point.y + y});
  }
  body.polygon.push_back({x, y + 40.0});
  body.color = {255, 255, 255, 1};
  inks.push_back(body);

  std::vector<Vec2> outline;
  for (const Vec2& point : crest) {
    outline.push_back({point.x + x, point.y + y});
  }
  StrokeStyle style;
  style.width = 3;
  style.noise = 1;
  addRibbon(inks, outline, prng, noise, style, gray(0.3));
  finishMountain(inks, ridge.layers, x, y, seed, height, prng, noise);
  return inks;
}

namespace {

double segmentLength(Vec2 a, Vec2 b) {
  const double dx = a.x - b.x;
  const double dy = a.y - b.y;
  return std::sqrt(dx * dx + dy * dy);
}

double triangleArea(const std::vector<Vec2>& poly) {
  const double a = segmentLength(poly[0], poly[1]);
  const double b = segmentLength(poly[1], poly[2]);
  const double c = segmentLength(poly[2], poly[0]);
  const double s = (a + b + c) / 2.0;
  const double value = s * (s - a) * (s - b) * (s - c);
  if (value <= 0.0) {
    return 0.0;
  }
  return std::sqrt(value);
}

std::vector<std::vector<Vec2>> shatter(const std::vector<Vec2>& poly, double limit) {
  if (poly.empty()) {
    return {};
  }
  if (poly.size() < 3 || triangleArea(poly) < limit) {
    return {poly};
  }
  const int count = static_cast<int>(poly.size());
  int longest = 0;
  double best = -1.0;
  for (int i = 0; i < count; ++i) {
    const double length = segmentLength(poly[static_cast<std::size_t>(i)], poly[static_cast<std::size_t>((i + 1) % count)]);
    if (length > best) {
      best = length;
      longest = i;
    }
  }
  const int next = (longest + 1) % count;
  const int after = (longest + 2) % count;
  const Vec2 mid = midPoint(poly[static_cast<std::size_t>(longest)], poly[static_cast<std::size_t>(next)]);
  auto left = shatter({poly[static_cast<std::size_t>(longest)], mid, poly[static_cast<std::size_t>(after)]}, limit);
  auto right = shatter({poly[static_cast<std::size_t>(after)], poly[static_cast<std::size_t>(next)], mid}, limit);
  left.insert(left.end(), right.begin(), right.end());
  return left;
}

std::vector<std::vector<Vec2>> triangulate(std::vector<Vec2> poly, double area = 100.0) {
  if (poly.size() <= 3) {
    return shatter(poly, area);
  }
  const Vec2 previous = poly.back();
  const Vec2 current = poly.front();
  const Vec2 next = poly[1];
  poly.erase(poly.begin());
  auto parts = shatter({previous, current, next}, area);
  auto rest = triangulate(std::move(poly), area);
  parts.insert(parts.end(), rest.begin(), rest.end());
  return parts;
}

Rgba distantShade(Noise& noise, double x, double y, double yOffset) {
  const int shade = static_cast<int>(noise.sample(x * 0.02, y * 0.02, yOffset) * 55.0 + 200.0);
  const auto channel = static_cast<std::uint8_t>(shade);
  return {channel, channel, channel, 1};
}

Vec2 centroid(const std::vector<Vec2>& poly) {
  Vec2 sum;
  for (const Vec2& point : poly) {
    sum.x += point.x;
    sum.y += point.y;
  }
  const double count = static_cast<double>(poly.size());
  sum.x /= count;
  sum.y /= count;
  return sum;
}

}  // namespace

std::vector<Ink> distMountInk(double x, double y, double seed, Noise& noise, double height, int length) {
  constexpr int kSpan = 10;
  constexpr int kSegments = 5;
  const double strips = static_cast<double>(length) / kSpan / kSegments;
  const double along = static_cast<double>(length) / kSpan;
  std::vector<Ink> inks;
  for (int i = 0; i < strips; ++i) {
    std::vector<Vec2> strip;
    for (int j = 0; j < kSegments + 1; ++j) {
      const double k = i * kSegments + j;
      const double wave = std::pow(std::sin((kPi * k) / along), 0.5);
      strip.push_back({x + k * kSpan, y - height * noise.sample(k * 0.05, seed) * wave});
    }
    for (int j = 0; j < kSegments / 2.0 + 1.0; ++j) {
      const double k = i * kSegments + j * 2.0;
      const double wave = std::pow(std::sin((kPi * k) / along), 1.0);
      strip.insert(strip.begin(), {x + k * kSpan, y + 24.0 * noise.sample(k * 0.05, 2.0, seed) * wave});
    }
    Ink body;
    body.polygon = strip;
    body.color = distantShade(noise, strip.back().x, strip.back().y, y);
    inks.push_back(std::move(body));
    for (const auto& facet : triangulate(strip)) {
      if (facet.size() < 3) {
        continue;
      }
      Ink piece;
      piece.polygon = facet;
      const Vec2 middle = centroid(facet);
      piece.color = distantShade(noise, middle.x, middle.y, y);
      inks.push_back(std::move(piece));
    }
  }
  return inks;
}

Vec2 mountainRidgePoint(Prng& prng, Noise& noise, double yOffset, double height, double width) {
  return buildRidge(yOffset, 0, prng, noise, height, width).layers[0][25];
}

namespace {

double flatSpread(Prng& prng) {
  if (prng.next() > 0.5) {
    return 0.1 + 0.4 * prng.next();
  }
  return 0.9 - 0.4 * prng.next();
}

Rgba flatHatch(Prng& prng) {
  return gray(fixed3(prng.next() * 0.3));
}

double normRand(Prng& prng, double low, double high) {
  return low + (high - low) * prng.next();
}

void appendInks(std::vector<Ink>& destination, std::vector<Ink> source) {
  destination.insert(destination.end(), std::make_move_iterator(source.begin()), std::make_move_iterator(source.end()));
}

void addWhite(std::vector<Ink>& inks, std::vector<Vec2> polygon) {
  if (polygon.size() < 3) {
    return;
  }
  Ink ink;
  ink.polygon = std::move(polygon);
  ink.color = {255, 255, 255, 1};
  inks.push_back(std::move(ink));
}

struct Branch {
  std::vector<Vec2> left;
  std::vector<Vec2> right;
};

Branch growBranch(double height, double width, double angle, double detail, double bend, Prng& prng, Noise& noise) {
  std::vector<Vec2> spine{{0, 0}};
  double nx = 0;
  double ny = 0;
  double heading = 0;
  constexpr int steps = 3;
  const int signs[] = {-1, 1};
  for (int i = 0; i < steps; ++i) {
    heading += (bend / 2.0 + (prng.next() * bend) / 2.0) * randChoice(prng, signs, 2);
    nx += std::cos(heading) * height / steps;
    ny -= std::sin(heading) * height / steps;
    spine.push_back({nx, ny});
  }
  const double turn = std::atan2(spine.back().y, spine.back().x);
  for (Vec2& point : spine) {
    const double a = std::atan2(point.y, point.x);
    const double distance = std::hypot(point.x, point.y);
    point.x = distance * std::cos(a - turn + angle);
    point.y = distance * std::sin(a - turn + angle);
  }

  Branch branch;
  const double span = detail;
  const double total = (static_cast<double>(spine.size()) - 1.0) * span;
  double previousX = 0;
  double previousY = 0;
  for (int i = 0; i < total; ++i) {
    const double at = static_cast<double>(i);
    const int last = static_cast<int>(std::floor(at / span));
    const int next = std::min(static_cast<int>(std::ceil(at / span)), static_cast<int>(spine.size()) - 1);
    const double p = std::fmod(at, span) / span;
    const Vec2& from = spine[static_cast<std::size_t>(last)];
    const Vec2& to = spine[static_cast<std::size_t>(next)];
    const double x = from.x * (1.0 - p) + to.x * p;
    const double y = from.y * (1.0 - p) + to.y * p;
    const double direction = std::atan2(y - previousY, x - previousX);
    const double wobble = (noise.sample(at * 0.3) - 0.5) * width * height / 80.0;
    const double extra = p == 0.0 ? prng.next() * width : 0.0;
    const double half = width * (((total - at) / total) * 0.5 + 0.5);
    branch.left.push_back({x + std::cos(direction + kPi / 2.0) * (half + wobble + extra),
                           y + std::sin(direction + kPi / 2.0) * (half + wobble + extra)});
    branch.right.push_back({x + std::cos(direction - kPi / 2.0) * (half - wobble + extra),
                            y + std::sin(direction - kPi / 2.0) * (half - wobble + extra)});
    previousX = x;
    previousY = y;
  }
  return branch;
}

std::vector<Vec2> joinBranch(const Branch& branch) {
  std::vector<Vec2> line = branch.left;
  line.insert(line.end(), branch.right.rbegin(), branch.right.rend());
  return line;
}

void addTwig(std::vector<Ink>& inks, double x, double y, int depth, int direction, double scale, double width,
             double angle, bool leaves, double leafReach, Prng& prng, Noise& noise) {
  constexpr int count = 10;
  const double lengthScale = prng.next() * 0.5 + 0.5;
  const int ignored[] = {0};
  (void)randChoice(prng, ignored, 1);
  const double base = ((prng.next() * kPi) / 6.0) * direction + angle;
  std::vector<Vec2> line;
  const int signs[] = {-1, 1};
  for (int i = 0; i < count; ++i) {
    const double bend = -1.0 / std::pow(static_cast<double>(i) / count + 1.0, 5.0) + 1.0;
    const double mx = direction * bend * 50.0 * scale * lengthScale;
    const double my = -i * 5.0 * scale;
    const double heading = std::atan2(my, mx);
    const double distance = std::hypot(mx, my);
    const double nx = std::cos(heading + base) * distance;
    const double ny = std::sin(heading + base) * distance;
    line.push_back({nx + x, ny + y});
    if ((i == count / 3 || i == (count * 2) / 3) && depth > 0) {
      addTwig(inks, nx + x, ny + y, depth - 1, direction * randChoice(prng, signs, 2), scale * 0.8, width, angle,
              leaves, leafReach, prng, noise);
    }
    if (i == count - 1 && leaves) {
      for (int j = 0; j < 5; ++j) {
        const double offset = (j - 2.5) * 5.0;
        BlobStyle style;
        style.width = (6.0 + 3.0 * prng.next()) * width;
        style.length = (15.0 + 12.0 * prng.next()) * width;
        style.angle = angle / 2.0 + kPi / 2.0 + kPi * 0.2 * (prng.next() - 0.5);
        style.shape = shrubShape;
        Ink ink;
        ink.polygon = blobPolygon(nx + x + std::cos(angle) * offset * width,
                                  ny + y + (std::sin(angle) * offset - leafReach / (depth + 1.0)) * width, prng, noise,
                                  style);
        ink.color = gray(fixed3(0.5 + depth * 0.2));
        inks.push_back(std::move(ink));
      }
    }
  }
  StrokeStyle style;
  style.width = 1;
  style.widthFn = [](double t) { return std::cos((t * kPi) / 2.0); };
  addRibbon(inks, line, prng, noise, style, gray(0.5));
}

void addBark(std::vector<Ink>& inks, double x, double y, const Branch& branch, Prng& prng, Noise& noise) {
  const auto& left = branch.left;
  const auto& right = branch.right;
  const int count = std::min(static_cast<int>(left.size()), static_cast<int>(right.size()));
  for (int i = 2; i < count - 1; ++i) {
    const Vec2& west = left[static_cast<std::size_t>(i)];
    const Vec2& east = right[static_cast<std::size_t>(i)];
    const double leftAngle = std::atan2(west.y - left[static_cast<std::size_t>(i - 1)].y,
                                        west.x - left[static_cast<std::size_t>(i - 1)].x);
    const double rightAngle = std::atan2(east.y - right[static_cast<std::size_t>(i - 1)].y,
                                         east.x - right[static_cast<std::size_t>(i - 1)].x);
    const double mix = prng.next();
    const double nx = west.x * (1.0 - mix) + east.x * mix;
    const double ny = west.y * (1.0 - mix) + east.y * mix;
    const double angle = (leftAngle + rightAngle) / 2.0;
    if (prng.next() < 0.2) {
      BlobStyle style;
      style.noise = 1;
      style.length = 15;
      style.width = 6.0 - std::abs(mix - 0.5) * 10.0;
      style.angle = angle;
      Ink ink;
      ink.polygon = blobPolygon(nx + x, ny + y, prng, noise, style);
      ink.color = gray(0.6);
      inks.push_back(std::move(ink));
    } else {
      const double len = 10.0 + 10.0 * prng.next();
      std::vector<Vec2> samples;
      for (int step = 0; step <= 20; ++step) {
        const double p = (static_cast<double>(step) / 20.0) * 2.0;
        const double xo = len / 2.0 - std::abs(p - 1.0) * len;
        const double yo = (p <= 1.0 ? std::pow(std::sin(p * kPi), 0.5)
                                    : -std::pow(std::sin((p + 1.0) * kPi), 0.5)) *
                          (5.0 - std::abs(mix - 0.5) * 10.0) / 2.0;
        samples.push_back({std::hypot(xo, yo), std::atan2(yo, xo)});
      }
      std::vector<double> field;
      const double n0 = prng.next() * 10.0;
      for (int step = 0; step <= 20; ++step) {
        field.push_back(noise.sample(step * 0.05, n0));
      }
      loopNoise(field);
      std::vector<Vec2> center;
      for (int step = 0; step < static_cast<int>(samples.size()); ++step) {
        const double scale = field[static_cast<std::size_t>(step)] * 0.5 + 0.5;
        center.push_back({nx + x + std::cos(samples[static_cast<std::size_t>(step)].y + angle) *
                                       samples[static_cast<std::size_t>(step)].x * scale,
                          ny + y + std::sin(samples[static_cast<std::size_t>(step)].y + angle) *
                                       samples[static_cast<std::size_t>(step)].x * scale});
      }
      const double fringe = prng.next();
      StrokeStyle style;
      style.width = 0.8;
      style.noise = 0;
      style.widthFn = [fringe](double t) { return std::sin((t + fringe) * kPi * 3.0); };
      addRibbon(inks, center, prng, noise, style, gray(0.4));
    }
    if (prng.next() < 0.05) {
      const double marks = prng.next() * 2.0 + 2.0;
      const int sides[] = {0, 1};
      const int side = randChoice(prng, sides, 2);
      const Vec2 origin = side == 0 ? west : east;
      const double sideAngle = side == 0 ? leftAngle : rightAngle;
      for (int j = 0; j < marks; ++j) {
        BlobStyle style;
        style.width = 4;
        style.length = 4.0 + 6.0 * prng.next();
        style.angle = leftAngle + kPi / 2.0;
        Ink ink;
        ink.polygon = blobPolygon(origin.x + x + std::cos(sideAngle) * (j - marks / 2.0) * 4.0,
                                  origin.y + y + std::sin(sideAngle) * (j - marks / 2.0) * 4.0, prng, noise, style);
        ink.color = gray(0.6);
        inks.push_back(std::move(ink));
      }
    }
  }

  std::vector<Vec2> ring = left;
  ring.insert(ring.end(), right.rbegin(), right.rend());
  std::vector<std::vector<Vec2>> groups(1);
  for (const Vec2& point : ring) {
    if (prng.next() < 0.5) {
      groups.push_back({});
    } else {
      groups.back().push_back(point);
    }
  }
  for (int i = 0; i < static_cast<int>(groups.size()); ++i) {
    auto line = subdivide(groups[static_cast<std::size_t>(i)], 4);
    for (int j = 0; j < static_cast<int>(line.size()); ++j) {
      line[static_cast<std::size_t>(j)].x +=
          (noise.sample(i, j * 0.1, 1) - 0.5) * (15.0 + 5.0 * randGaussian(prng));
      line[static_cast<std::size_t>(j)].y +=
          (noise.sample(i, j * 0.1, 2) - 0.5) * (15.0 + 5.0 * randGaussian(prng));
    }
    std::vector<Vec2> moved;
    for (const Vec2& point : line) {
      moved.push_back({point.x + x, point.y + y});
    }
    StrokeStyle style;
    style.width = 1.5;
    addRibbon(inks, moved, prng, noise, style, gray(0.7));
  }
}

void addCanopyStroke(std::vector<Ink>& inks, std::vector<Vec2> line, double alpha, bool trim, Prng& prng,
                     Noise& noise) {
  const Rgba color = gray(fixed3(alpha + prng.next() * 0.1));
  if (trim) {
    if (!line.empty()) {
      line.erase(line.begin());
    }
    if (!line.empty()) {
      line.pop_back();
    }
  }
  StrokeStyle style;
  style.width = 2.5;
  style.noise = 0.9;
  style.widthFn = [](double) { return std::sin(1.0); };
  addRibbon(inks, line, prng, noise, style, color);
}

std::vector<Vec2> place(const std::vector<Vec2>& local, double x, double y) {
  std::vector<Vec2> moved;
  moved.reserve(local.size());
  for (const Vec2& point : local) {
    moved.push_back({point.x + x, point.y + y});
  }
  return moved;
}

void addTree04(std::vector<Ink>& inks, double x, double y, Prng& prng, Noise& noise) {
  constexpr double height = 300;
  constexpr double width = 6;
  std::vector<Ink> detail;
  const Branch trunk = growBranch(height, width, -kPi / 2.0, 10, kPi * 0.2, prng, noise);
  addBark(detail, x, y, trunk, prng, noise);
  const std::vector<Vec2> outline = joinBranch(trunk);
  std::vector<Vec2> canopy;
  const double count = static_cast<double>(outline.size());
  for (int i = 0; i < static_cast<int>(outline.size()); ++i) {
    bool shoot = false;
    if (i >= count * 0.3 && i <= count * 0.7) {
      shoot = prng.next() < 0.1;
    }
    if (shoot || static_cast<double>(i) == count / 2.0 - 1.0) {
      const double branchAngle = kPi * 0.2 - kPi * 1.4 * (i > count / 2.0 ? 1.0 : 0.0);
      Branch side = growBranch(height * (prng.next() + 1.0) * 0.3, width * 0.5, branchAngle, 10, kPi * 0.2, prng, noise);
      if (!side.left.empty()) {
        side.left.erase(side.left.begin());
      }
      if (!side.right.empty()) {
        side.right.erase(side.right.begin());
      }
      Branch shifted = side;
      for (Vec2& point : shifted.left) {
        point.x += outline[static_cast<std::size_t>(i)].x;
        point.y += outline[static_cast<std::size_t>(i)].y;
      }
      for (Vec2& point : shifted.right) {
        point.x += outline[static_cast<std::size_t>(i)].x;
        point.y += outline[static_cast<std::size_t>(i)].y;
      }
      addBark(detail, x, y, shifted, prng, noise);
      const bool upright = branchAngle > -kPi / 2.0;
      for (int j = 0; j < static_cast<int>(side.left.size()); ++j) {
        if (prng.next() < 0.2 || j + 1 == static_cast<int>(side.left.size())) {
          const Vec2& tip = side.left[static_cast<std::size_t>(j)];
          addTwig(detail, tip.x + outline[static_cast<std::size_t>(i)].x + x,
                  tip.y + outline[static_cast<std::size_t>(i)].y + y, 1, upright ? 1 : -1, 0.5, height / 300.0,
                  upright ? branchAngle : branchAngle + kPi, true, 12, prng, noise);
        }
      }
      for (const Vec2& point : joinBranch(side)) {
        canopy.push_back({point.x + outline[static_cast<std::size_t>(i)].x, point.y + outline[static_cast<std::size_t>(i)].y});
      }
    } else {
      canopy.push_back(outline[static_cast<std::size_t>(i)]);
    }
  }
  addWhite(inks, place(canopy, x, y));
  addCanopyStroke(inks, place(canopy, x, y), 0.4, true, prng, noise);
  appendInks(inks, std::move(detail));
}

void addTree05(std::vector<Ink>& inks, double x, double y, double height, Prng& prng, Noise& noise) {
  constexpr double width = 5;
  std::vector<Ink> detail;
  const Branch trunk = growBranch(height, width, -kPi / 2.0, 10, 0, prng, noise);
  addBark(detail, x, y, trunk, prng, noise);
  const std::vector<Vec2> outline = joinBranch(trunk);
  std::vector<Vec2> canopy;
  const double count = static_cast<double>(outline.size());
  for (int i = 0; i < static_cast<int>(outline.size()); ++i) {
    const double along = std::abs(i - count * 0.5) / (count * 0.5);
    bool shoot = false;
    if (i >= count * 0.2 && i <= count * 0.8 && i % 3 == 0) {
      shoot = prng.next() > along;
    }
    if (shoot || static_cast<double>(i) == count / 2.0 - 1.0) {
      const double bar = prng.next() * 0.2;
      const double branchAngle = -bar * kPi - (1.0 - bar * 2.0) * kPi * (i > count / 2.0 ? 1.0 : 0.0);
      Branch side =
          growBranch(height * (0.3 * along - prng.next() * 0.05), width * 0.5, branchAngle, 10, 0.5, prng, noise);
      if (!side.left.empty()) {
        side.left.erase(side.left.begin());
      }
      if (!side.right.empty()) {
        side.right.erase(side.right.begin());
      }
      const bool upright = branchAngle > -kPi / 2.0;
      for (int j = 0; j < static_cast<int>(side.left.size()); ++j) {
        if (j % 20 == 0 || j + 1 == static_cast<int>(side.left.size())) {
          const Vec2& tip = side.left[static_cast<std::size_t>(j)];
          addTwig(detail, tip.x + outline[static_cast<std::size_t>(i)].x + x,
                  tip.y + outline[static_cast<std::size_t>(i)].y + y, 0, upright ? 1 : -1, (0.2 * height) / 300.0,
                  height / 300.0, upright ? branchAngle : branchAngle + kPi, true, 5, prng, noise);
        }
      }
      for (const Vec2& point : joinBranch(side)) {
        canopy.push_back({point.x + outline[static_cast<std::size_t>(i)].x, point.y + outline[static_cast<std::size_t>(i)].y});
      }
    } else {
      canopy.push_back(outline[static_cast<std::size_t>(i)]);
    }
  }
  addWhite(inks, place(canopy, x, y));
  addCanopyStroke(inks, place(canopy, x, y), 0.4, true, prng, noise);
  appendInks(inks, std::move(detail));
}

void addTree06(std::vector<Ink>& inks, double x, double y, double height, Prng& prng, Noise& noise) {
  constexpr double width = 6;
  std::vector<Ink> detail;
  std::function<std::vector<Vec2>(double, double, int, double, double, double, double)> grow;
  grow = [&](double xOffset, double yOffset, int depth, double branchHeight, double branchWidth, double angle,
             double bend) {
    const Branch trunk = growBranch(branchHeight, branchWidth, angle, branchHeight / 20.0, bend, prng, noise);
    addBark(detail, xOffset, yOffset, trunk, prng, noise);
    const std::vector<Vec2> outline = joinBranch(trunk);
    std::vector<Vec2> canopy;
    const double count = static_cast<double>(outline.size());
    for (int i = 0; i < static_cast<int>(outline.size()); ++i) {
      const bool rare = prng.next() < 0.025 && i >= count * 0.2 && i <= count * 0.8;
      const int middle = static_cast<int>(count / 2.0);
      if ((rare || i == middle - 1 || i == middle + 1) && depth > 0) {
        const double bar = 0.02 + prng.next() * 0.08;
        const double branchAngle = bar * kPi - bar * 2.0 * kPi * (i > count / 2.0 ? 1.0 : 0.0);
        const std::vector<Vec2> child =
            grow(outline[static_cast<std::size_t>(i)].x + xOffset, outline[static_cast<std::size_t>(i)].y + yOffset,
                 depth - 1, branchHeight * (0.7 + prng.next() * 0.2), branchWidth * 0.6, angle + branchAngle, 0.55);
        for (int j = 0; j < static_cast<int>(child.size()); ++j) {
          if (prng.next() < 0.03) {
            const Vec2& tip = child[static_cast<std::size_t>(j)];
            addTwig(detail, tip.x + outline[static_cast<std::size_t>(i)].x + xOffset,
                    tip.y + outline[static_cast<std::size_t>(i)].y + yOffset, 2, branchAngle > 0 ? 1 : -1, 0.3, 1,
                    branchAngle * (prng.next() * 0.5 + 0.75), false, 0, prng, noise);
          }
        }
        for (const Vec2& point : child) {
          canopy.push_back({point.x + outline[static_cast<std::size_t>(i)].x, point.y + outline[static_cast<std::size_t>(i)].y});
        }
      } else {
        canopy.push_back(outline[static_cast<std::size_t>(i)]);
      }
    }
    return canopy;
  };
  const std::vector<Vec2> canopy = grow(x, y, 3, height, width, -kPi / 2.0, 0);
  addWhite(inks, place(canopy, x, y));
  addCanopyStroke(inks, place(canopy, x, y), 0.4, true, prng, noise);
  appendInks(inks, std::move(detail));
}

double reedShape(double p) {
  if (p <= 1.0) {
    return 2.75 * p * std::pow(1.0 - p, 1.0 / 1.8);
  }
  return 2.75 * (p - 2.0) * std::pow(p - 1.0, 1.0 / 1.8);
}

void addTree07(std::vector<Ink>& inks, double x, double y, double height, Prng& prng, Noise& noise) {
  constexpr int resolution = 10;
  constexpr double width = 4;
  std::vector<Vec2> field;
  for (int i = 0; i < resolution; ++i) {
    field.push_back({noise.sample(i * 0.5), noise.sample(i * 0.5, 0.5)});
  }
  std::vector<Vec2> left;
  std::vector<Vec2> right;
  std::vector<std::vector<Vec2>> facets;
  for (int i = 0; i < resolution; ++i) {
    const double nx = x + std::sqrt(static_cast<double>(i) / resolution) * 0.2 * 100.0;
    const double ny = y - (i * height) / resolution;
    if (i >= resolution / 4.0) {
      const double px = nx + (prng.next() - 0.5) * width * 1.2 * (resolution - i) * 0.5;
      const double py = ny + (prng.next() - 0.5) * width * 0.5;
      BlobStyle style;
      style.length = prng.next() * 50.0 + 20.0;
      style.width = prng.next() * 12.0 + 12.0;
      style.angle = -prng.next() * kPi / 6.0;
      style.shape = reedShape;
      for (const auto& facet : triangulate(blobPolygon(px, py, prng, noise, style), 50)) {
        if (facet.size() >= 3) {
          facets.push_back(facet);
        }
      }
    }
    left.push_back({nx + (field[static_cast<std::size_t>(i)].x - 0.5) * width - width / 2.0, ny});
    right.push_back({nx + (field[static_cast<std::size_t>(i)].y - 0.5) * width + width / 2.0, ny});
  }
  std::vector<Vec2> trunk = left;
  trunk.insert(trunk.end(), right.rbegin(), right.rend());
  auto triangles = triangulate(std::move(trunk), 50);
  triangles.insert(triangles.end(), facets.begin(), facets.end());
  for (const auto& facet : triangles) {
    if (facet.size() < 3) {
      continue;
    }
    const Vec2 middle = centroid(facet);
    const int shade = static_cast<int>(noise.sample(middle.x * 0.02, middle.y * 0.02) * 200.0 + 50.0);
    const auto channel = static_cast<std::uint8_t>(shade);
    Ink ink;
    ink.polygon = facet;
    ink.color = {channel, channel, channel, 0.8};
    inks.push_back(std::move(ink));
  }
}

void addReed(std::vector<Ink>& inks, double x, double y, int depth, double length, double angle, double bend, Prng& prng,
             Noise& noise) {
  const int curves[] = {0, 1};
  const int curve = randChoice(prng, curves, 2);
  std::vector<Vec2> line = subdivide({{x, y}, {x + length, y}}, 10);
  for (int i = 0; i < static_cast<int>(line.size()); ++i) {
    const double wave = std::sin((static_cast<double>(i) / static_cast<double>(line.size())) * kPi);
    line[static_cast<std::size_t>(i)].y += (curve == 0 ? wave : -wave) * 2.0;
  }
  for (Vec2& point : line) {
    const double dx = point.x - x;
    const double dy = point.y - y;
    const double distance = std::hypot(dx, dy);
    const double heading = std::atan2(dy, dx);
    point.x = x + distance * std::cos(heading + angle);
    point.y = y + distance * std::sin(heading + angle);
  }
  StrokeStyle style;
  style.width = 0.8;
  if (depth == 0) {
    style.widthFn = [](double t) { return std::cos(0.5 * kPi * t); };
  } else {
    style.widthFn = [](double) { return 1.0; };
  }
  addRibbon(inks, line, prng, noise, style, gray(0.5));
  if (depth == 0) {
    return;
  }
  const int lean[] = {-1, 1};
  const double nextBend = bend + randChoice(prng, lean, 2) * kPi * 0.001 * depth * depth;
  const double endX = x + std::cos(angle) * length;
  const double endY = y + std::sin(angle) * length;
  if (prng.next() < 0.5) {
    const int first[] = {0, 1};
    const int fork = randChoice(prng, first, 2);
    const double low = fork == 0 ? normRand(prng, -1, 0.5) : normRand(prng, 0.5, 1);
    const double high = fork == 0 ? normRand(prng, 0.5, 1) : normRand(prng, -1, 0.5);
    (void)high;
    addReed(inks, endX, endY, depth - 1, length * normRand(prng, 0.8, 0.9), angle + bend + kPi * low * 0.2, nextBend,
            prng, noise);
    const int second[] = {0, 1};
    const int other = randChoice(prng, second, 2);
    const double left = other == 0 ? normRand(prng, -1, -0.5) : normRand(prng, 0.5, 1);
    const double right = other == 0 ? normRand(prng, 0.5, 1) : normRand(prng, -1, -0.5);
    (void)right;
    addReed(inks, endX, endY, depth - 1, length * normRand(prng, 0.8, 0.9), angle + bend + kPi * left * 0.2, nextBend,
            prng, noise);
  } else {
    addReed(inks, endX, endY, depth - 1, length * normRand(prng, 0.8, 0.9), angle + bend, nextBend, prng, noise);
  }
}

void addTree08(std::vector<Ink>& inks, double x, double y, double height, Prng& prng, Noise& noise) {
  const double lean = normRand(prng, -1, 1) * kPi * 0.2;
  const Branch trunk = growBranch(height, 1, -kPi / 2.0 + lean, height / 20.0, kPi * 0.2, prng, noise);
  const std::vector<Vec2> outline = joinBranch(trunk);
  std::vector<Ink> detail;
  for (int i = 0; i < static_cast<int>(outline.size()); ++i) {
    if (prng.next() < 0.2) {
      addReed(detail, x + outline[static_cast<std::size_t>(i)].x, y + outline[static_cast<std::size_t>(i)].y,
              static_cast<int>(std::floor(4.0 * prng.next())), 15, -kPi / 2.0 - lean * prng.next(), 0, prng, noise);
    } else if (i == static_cast<int>(std::floor(static_cast<double>(outline.size()) / 2.0))) {
      addReed(detail, x + outline[static_cast<std::size_t>(i)].x, y + outline[static_cast<std::size_t>(i)].y, 3, 15,
              -kPi / 2.0 + lean, 0, prng, noise);
    }
  }
  addWhite(inks, place(outline, x, y));
  addCanopyStroke(inks, place(outline, x, y), 0.6, false, prng, noise);
  appendInks(inks, std::move(detail));
}

struct Bounds {
  double xmin = 0;
  double xmax = 0;
  double ymin = 0;
  double ymax = 0;
};

void addFlatRocks(std::vector<Ink>& inks, double x, double y, const Bounds& bounds, double rolls, double yJitter,
                  double yLift, double widthBase, double heightBase, int shade, Prng& prng, Noise& noise) {
  for (int i = 0; i < prng.next() * rolls; ++i) {
    const double rx = x + normRand(prng, bounds.xmin, bounds.xmax);
    const double ry = y + (bounds.ymin + bounds.ymax) / 2.0 + normRand(prng, -yJitter, yJitter) + yLift;
    const double seed = prng.next() * 100.0;
    const double rockWidth = widthBase + prng.next() * 20.0;
    const double rockHeight = heightBase + prng.next() * 20.0;
    addRock(inks, rx, ry, seed, rockWidth, rockHeight, prng, noise, shade);
  }
}

double squareWeight(double value) {
  return value * value;
}

double hutSpread(Prng& prng) {
  return weightedRandom(prng, squareWeight);
}

Rgba hutHatch(Prng& prng) {
  return {120, 120, 120, fixed3(0.3 + prng.next() * 0.3)};
}

void addHut(std::vector<Ink>& inks, double x, double y, double height, double width, Prng& prng, Noise& noise) {
  constexpr int rows = 10;
  constexpr int columns = 10;
  std::vector<std::vector<Vec2>> layers(rows);
  for (int row = 0; row < rows; ++row) {
    const double rise = height + height * 0.2 * prng.next();
    for (int column = 0; column < columns; ++column) {
      const double across = static_cast<double>(column) / (columns - 1);
      const double down = static_cast<double>(row) / (rows - 1);
      layers[static_cast<std::size_t>(row)].push_back(
          {width * (down - 0.5) * std::pow(across, 0.7), rise * across});
    }
  }
  std::vector<Vec2> cap = layers.front();
  cap.pop_back();
  std::vector<Vec2> tail = layers.back();
  tail.pop_back();
  cap.insert(cap.end(), tail.rbegin(), tail.rend());
  addWhite(inks, place(cap, x, y));
  addSolid(inks, place(layers.front(), x, y), 2, gray(0.3));
  addSolid(inks, place(layers.back(), x, y), 2, gray(0.3));
  appendTexture(inks, layers, x, y, 300, 0, 1, hutHatch, hutSpread, prng, noise, 0.25, 5);
}

std::vector<Vec2> bendCurve(const std::vector<Vec2>& source, double weight) {
  std::vector<Vec2> points = source;
  if (points.size() == 2) {
    points = {points[0], midPoint(points[0], points[1]), points[1]};
  }
  std::vector<Vec2> result;
  const int lastSegment = static_cast<int>(points.size()) - 3;
  for (int segment = 0; segment < static_cast<int>(points.size()) - 2; ++segment) {
    const Vec2 start = segment == 0 ? points[0] : midPoint(points[static_cast<std::size_t>(segment)],
                                                           points[static_cast<std::size_t>(segment + 1)]);
    const Vec2 control = points[static_cast<std::size_t>(segment + 1)];
    const bool closing = segment == lastSegment;
    const Vec2 end = closing ? points[static_cast<std::size_t>(segment + 2)]
                             : midPoint(points[static_cast<std::size_t>(segment + 1)],
                                        points[static_cast<std::size_t>(segment + 2)]);
    constexpr int steps = 20;
    for (int i = 0; i < steps + (closing ? 1 : 0); ++i) {
      const double t = static_cast<double>(i) / steps;
      const double blend = std::pow(1.0 - t, 2) + 2.0 * t * (1.0 - t) * weight + t * t;
      result.push_back({(std::pow(1.0 - t, 2) * start.x + 2.0 * t * (1.0 - t) * control.x * weight + t * t * end.x) /
                            blend,
                        (std::pow(1.0 - t, 2) * start.y + 2.0 * t * (1.0 - t) * control.y * weight + t * t * end.y) /
                            blend});
    }
  }
  return result;
}

struct Ribbon {
  std::vector<Vec2> left;
  std::vector<Vec2> right;
};

Ribbon expandRibbon(const std::vector<Vec2>& points, const std::function<double(double)>& widthAt, Prng& prng) {
  (void)(prng.next() * 10.0);
  Ribbon ribbon;
  if (points.size() < 2) {
    return ribbon;
  }
  const double count = static_cast<double>(points.size());
  for (std::size_t i = 1; i + 1 < points.size(); ++i) {
    const double w = widthAt(static_cast<double>(i) / count);
    const double a1 = std::atan2(points[i].y - points[i - 1].y, points[i].x - points[i - 1].x);
    const double a2 = std::atan2(points[i].y - points[i + 1].y, points[i].x - points[i + 1].x);
    double angle = (a1 + a2) / 2.0;
    if (angle < a2) {
      angle += kPi;
    }
    ribbon.left.push_back({points[i].x + w * std::cos(angle), points[i].y + w * std::sin(angle)});
    ribbon.right.push_back({points[i].x - w * std::cos(angle), points[i].y - w * std::sin(angle)});
  }
  const std::size_t last = points.size() - 1;
  const double startAngle = std::atan2(points[1].y - points[0].y, points[1].x - points[0].x) - kPi / 2.0;
  const double endAngle = std::atan2(points[last].y - points[last - 1].y, points[last].x - points[last - 1].x) - kPi / 2.0;
  const double startWidth = widthAt(0);
  const double endWidth = widthAt(1);
  ribbon.left.insert(ribbon.left.begin(), {points[0].x + startWidth * std::cos(startAngle),
                                           points[0].y + startWidth * std::sin(startAngle)});
  ribbon.right.insert(ribbon.right.begin(), {points[0].x - startWidth * std::cos(startAngle),
                                             points[0].y - startWidth * std::sin(startAngle)});
  ribbon.left.push_back({points[last].x + endWidth * std::cos(endAngle), points[last].y + endWidth * std::sin(endAngle)});
  ribbon.right.push_back(
      {points[last].x - endWidth * std::cos(endAngle), points[last].y - endWidth * std::sin(endAngle)});
  return ribbon;
}

std::vector<Vec2> mapPoints(const std::vector<Vec2>& points, const std::function<Vec2(Vec2)>& map) {
  std::vector<Vec2> moved;
  moved.reserve(points.size());
  for (const Vec2& point : points) {
    moved.push_back(map(point));
  }
  return moved;
}

void addCloth(std::vector<Ink>& inks, const std::vector<Vec2>& spine, const std::function<double(double)>& widthAt,
              const std::function<Vec2(Vec2)>& toGlobal, Prng& prng, Noise& noise) {
  const Ribbon ribbon = expandRibbon(bendCurve(spine, 2), widthAt, prng);
  std::vector<Vec2> body = mapPoints(ribbon.left, toGlobal);
  const auto right = mapPoints(ribbon.right, toGlobal);
  body.insert(body.end(), right.rbegin(), right.rend());
  addWhite(inks, std::move(body));
  StrokeStyle style;
  style.width = 1;
  addRibbon(inks, mapPoints(ribbon.left, toGlobal), prng, noise, style, gray(0.5));
  addRibbon(inks, mapPoints(ribbon.right, toGlobal), prng, noise, style, gray(0.6));
}

std::vector<Vec2> placeOnSegment(Vec2 from, Vec2 to, const std::vector<Vec2>& local, bool flip) {
  const double angle = std::atan2(to.y - from.y, to.x - from.x) - kPi / 2.0;
  const double scale = std::hypot(to.x - from.x, to.y - from.y);
  std::vector<Vec2> placed;
  for (Vec2 point : local) {
    if (flip) {
      point.x = -point.x;
    }
    point.x = -point.x;
    const double distance = std::hypot(point.x, point.y);
    const double heading = std::atan2(point.y, point.x);
    placed.push_back({from.x + distance * scale * std::cos(angle + heading),
                      from.y + distance * scale * std::sin(angle + heading)});
  }
  return placed;
}

void addWideHat(std::vector<Ink>& inks, Vec2 from, Vec2 to, bool flip, Prng& prng) {
  (void)prng.next();
  const std::vector<Vec2> brim = {{-0.3, 0.5}, {-1.1, 0.5}, {-1.2, 0.6}, {-1.1, 0.7}, {-0.3, 0.8},
                                  {0.3, 0.8},  {1.0, 0.7},  {1.3, 0.6},  {1.2, 0.5},  {0.3, 0.5}};
  Ink fill;
  fill.polygon = placeOnSegment(from, to, brim, flip);
  fill.color = gray(0.8);
  if (fill.polygon.size() >= 3) {
    inks.push_back(std::move(fill));
  }
}

void addStick(std::vector<Ink>& inks, Vec2 from, Vec2 to, bool flip, Prng& prng, Noise& noise) {
  const double seed = prng.next();
  std::vector<Vec2> line;
  constexpr int count = 12;
  for (int i = 0; i < count; ++i) {
    const double along = static_cast<double>(i) / count;
    line.push_back({-noise.sample(i * 0.1, seed) * 0.1 * std::sin(along * kPi) * 5.0, i * 0.3});
  }
  addSolid(inks, placeOnSegment(from, to, line, flip), 1, gray(0.5));
}

void addHat(std::vector<Ink>& inks, Vec2 from, Vec2 to, bool flip, Prng& prng, Noise& noise) {
  const double seed = prng.next();
  const std::vector<Vec2> crown = {{-0.3, 0.5}, {0.3, 0.8}, {0.2, 1}, {0, 1.1}, {-0.3, 1.15}, {-0.55, 1}, {-0.65, 0.5}};
  Ink fill;
  fill.polygon = placeOnSegment(from, to, crown, flip);
  fill.color = gray(0.8);
  if (fill.polygon.size() >= 3) {
    inks.push_back(std::move(fill));
  }
  std::vector<Vec2> brim;
  for (int i = 0; i < 10; ++i) {
    brim.push_back({-0.3 - noise.sample(i * 0.2, seed) * i * 0.1, 0.5 - i * 0.3});
  }
  addSolid(inks, placeOnSegment(from, to, brim, flip), 1, gray(0.8));
}

void addPerson(std::vector<Ink>& inks, double x, double y, double scale, bool flip, Prng& prng, Noise& noise,
               const double* lengthOverride, bool wideHat, bool pole) {
  const double angle[9] = {0,
                           -kPi / 2.0,
                           normRand(prng, 0, 0),
                           (kPi / 4.0) * prng.next(),
                           ((kPi * 3.0) / 4.0) * prng.next(),
                           (kPi * 3.0) / 4.0,
                           -kPi / 4.0,
                           (-kPi * 3.0) / 4.0 - (kPi / 4.0) * prng.next(),
                           -kPi / 4.0};
  double length[9] = {0, 30, 20, 30, 30, 30, 30, 30, 30};
  if (lengthOverride != nullptr) {
    for (int i = 0; i < 9; ++i) {
      length[i] = lengthOverride[i];
    }
  }
  for (double& value : length) {
    value *= scale;
  }
  const int chains[9][4] = {{0}, {0, 1}, {0, 1, 2}, {0, 3}, {0, 3, 4}, {0, 1, 5}, {0, 1, 5, 6}, {0, 1, 7}, {0, 1, 7, 8}};
  const int chainCount[9] = {1, 2, 3, 2, 3, 3, 4, 3, 4};
  Vec2 joint[9];
  for (int index = 0; index < 9; ++index) {
    double rotation = 0;
    for (int step = 0; step < chainCount[index]; ++step) {
      const int node = chains[index][step];
      rotation += angle[node];
      joint[index].x += length[node] * std::cos(rotation);
      joint[index].y += length[node] * std::sin(rotation);
    }
  }
  const double ground = y - joint[4].y;
  const auto toGlobal = [&](Vec2 point) {
    return Vec2{(flip ? -point.x : point.x) + x, point.y + ground};
  };
  const auto sleeve = [&](double t) {
    return scale * 8.0 *
           (std::sin(0.5 * t * kPi) * std::pow(std::sin(t * kPi), 0.1) + (1.0 - t) * 0.4);
  };
  const auto body = [&](double t) {
    return scale * 11.0 *
           (std::sin(0.5 * t * kPi) * std::pow(std::sin(t * kPi), 0.1) + (1.0 - t) * 0.5);
  };
  const auto head = [&](double t) {
    return scale * 7.0 * std::pow(0.25 - std::pow(t - 0.5, 2), 0.3);
  };
  if (pole) {
    addStick(inks, toGlobal(joint[8]), toGlobal(joint[6]), flip, prng, noise);
  }
  addCloth(inks, {joint[1], joint[7], joint[8]}, sleeve, toGlobal, prng, noise);
  addCloth(inks, {joint[1], joint[0], joint[3], joint[4]}, body, toGlobal, prng, noise);
  addCloth(inks, {joint[1], joint[5], joint[6]}, sleeve, toGlobal, prng, noise);
  addCloth(inks, {joint[1], joint[2]}, head, toGlobal, prng, noise);

  Ribbon hair = expandRibbon(bendCurve({joint[1], joint[2]}, 2), head, prng);
  const int dropLeft = static_cast<int>(std::floor(static_cast<double>(hair.left.size()) * 0.1));
  const int dropRight = static_cast<int>(std::floor(static_cast<double>(hair.right.size()) * 0.95));
  if (dropLeft > 0 && dropLeft < static_cast<int>(hair.left.size())) {
    hair.left.erase(hair.left.begin(), hair.left.begin() + dropLeft);
  }
  if (dropRight > 0 && dropRight < static_cast<int>(hair.right.size())) {
    hair.right.erase(hair.right.begin(), hair.right.begin() + dropRight);
  }
  std::vector<Vec2> scalp = mapPoints(hair.left, toGlobal);
  const auto underside = mapPoints(hair.right, toGlobal);
  scalp.insert(scalp.end(), underside.rbegin(), underside.rend());
  Ink shade;
  shade.polygon = std::move(scalp);
  shade.color = gray(0.6);
  if (shade.polygon.size() >= 3) {
    inks.push_back(std::move(shade));
  }
  if (wideHat) {
    addWideHat(inks, toGlobal(joint[1]), toGlobal(joint[2]), flip, prng);
  } else {
    addHat(inks, toGlobal(joint[1]), toGlobal(joint[2]), flip, prng, noise);
  }
}

void addPavilion(std::vector<Ink>& inks, double x, double y, double seed, double width, double height, double perspective,
                 Prng& prng, Noise& noise) {
  const double split = 0.4 + prng.next() * 0.2;
  const double roof = height * split;
  const double wall = height * (1.0 - split);
  addHut(inks, x, y - height, roof, width, prng, noise);
  const double boxWidth = width * 2.0 / 3.0;
  constexpr double rotation = 0.7;
  const double mid = -boxWidth * 0.5 + boxWidth * rotation;
  const double back = -boxWidth * 0.5 + boxWidth * (1.0 - rotation);
  std::vector<std::vector<Vec2>> walls = {
      subdivide({{-boxWidth * 0.5, -wall}, {-boxWidth * 0.5, 0}}, 5),
      subdivide({{boxWidth * 0.5, -wall}, {boxWidth * 0.5, 0}}, 5),
      subdivide({{mid, -wall}, {mid, perspective}}, 5),
      subdivide({{back, -wall}, {back, -perspective}}, 5),
  };
  strokePolylines(inks, walls, x, y, 3, 1, unitWidth, gray(0.4), prng, noise);
  const int backSegments = static_cast<int>(3.0 + prng.next() * 3.0);
  addRail(inks, x, y, seed, 10, width, rotation, perspective * 2.0, backSegments, 1, true, prng, noise, false);
  const int crowd[] = {0, 1, 1, 2};
  const int people = randChoice(prng, crowd, 4);
  if (people == 1) {
    const double px = x + normRand(prng, -width / 3.0, width / 3.0);
    const int faces[] = {1, 0};
    addPerson(inks, px, y, 0.42, randChoice(prng, faces, 2) != 0, prng, noise);
  } else if (people == 2) {
    addPerson(inks, x + normRand(prng, -width / 4.0, -width / 5.0), y, 0.42, false, prng, noise);
    addPerson(inks, x + normRand(prng, width / 5.0, width / 4.0), y, 0.42, true, prng, noise);
  }
  const int frontSegments = static_cast<int>(3.0 + prng.next() * 3.0);
  addRail(inks, x, y, seed, 10, width, rotation, perspective * 2.0, frontSegments, 1, false, prng, noise, true);
}

void addFlatDecoration(std::vector<Ink>& inks, double x, double y, const Bounds& bounds, Prng& prng, Noise& noise) {
  const int themes[] = {0, 0, 1, 2, 3, 4};
  const int theme = randChoice(prng, themes, 6);
  addFlatRocks(inks, x, y, bounds, 5, 10, 10, 10, 10, 2, prng, noise);
  const int clumps[] = {0, 0, 1, 2};
  for (int i = 0; i < randChoice(prng, clumps, 4); ++i) {
    const double rx = x + normRand(prng, bounds.xmin, bounds.xmax);
    const double ry = y + (bounds.ymin + bounds.ymax) / 2.0 + normRand(prng, -5, 5) + 20;
    for (int k = 0; k < 2.0 + prng.next() * 3.0; ++k) {
      const double jitter = std::min(std::max(normRand(prng, -30, 30), bounds.xmin), bounds.xmax);
      addTree08(inks, rx + jitter, ry, 60.0 + prng.next() * 40.0, prng, noise);
    }
  }
  if (theme == 0) {
    addFlatRocks(inks, x, y, bounds, 3, 5, 20, 50, 40, 5, prng, noise);
  } else if (theme == 1) {
    const double pmin = prng.next() * 0.5;
    const double pmax = prng.next() * 0.5 + 0.5;
    const double xmin = bounds.xmin * (1.0 - pmin) + bounds.xmax * pmin;
    const double xmax = bounds.xmin * (1.0 - pmax) + bounds.xmax * pmax;
    const double ground = y + (bounds.ymin + bounds.ymax) / 2.0 + 20.0;
    for (double at = xmin; at < xmax; at += 30.0) {
      addTree05(inks, x + at + 20.0 * normRand(prng, -1, 1), ground, 100.0 + prng.next() * 200.0, prng, noise);
    }
    addFlatRocks(inks, x, y, bounds, 4, 5, 20, 50, 40, 5, prng, noise);
  } else if (theme == 2) {
    const int counts[] = {1, 1, 1, 1, 2, 2, 3};
    for (int i = 0; i < randChoice(prng, counts, 7); ++i) {
      const double rx = normRand(prng, bounds.xmin, bounds.xmax);
      const double ry = (bounds.ymin + bounds.ymax) / 2.0;
      addTree04(inks, x + rx, y + ry + 20.0, prng, noise);
      for (int j = 0; j < prng.next() * 2.0; ++j) {
        const double rockX = std::max(bounds.xmin, std::min(bounds.xmax, rx + normRand(prng, -50, 50)));
        addRock(inks, x + rockX, y + ry + normRand(prng, -5, 5) + 20.0, j * i * prng.next() * 100.0,
                50.0 + prng.next() * 20.0, 40.0 + prng.next() * 20.0, prng, noise, 5);
      }
    }
  } else if (theme == 3) {
    const int counts[] = {1, 1, 1, 1, 2, 2, 3};
    for (int i = 0; i < randChoice(prng, counts, 7); ++i) {
      addTree06(inks, x + normRand(prng, bounds.xmin, bounds.xmax), y + (bounds.ymin + bounds.ymax) / 2.0,
                60.0 + prng.next() * 60.0, prng, noise);
    }
  } else if (theme == 4) {
    const double pmin = prng.next() * 0.5;
    const double pmax = prng.next() * 0.5 + 0.5;
    const double xmin = bounds.xmin * (1.0 - pmin) + bounds.xmax * pmin;
    const double xmax = bounds.xmin * (1.0 - pmax) + bounds.xmax * pmax;
    for (double at = xmin; at < xmax; at += 20.0) {
      addTree07(inks, x + at + 20.0 * normRand(prng, -1, 1),
                y + (bounds.ymin + bounds.ymax) / 2.0 + normRand(prng, -1, 1), normRand(prng, 40, 80), prng, noise);
    }
  }
  for (int i = 0; i < 50.0 * prng.next(); ++i) {
    appendInks(inks, tree02(x + normRand(prng, bounds.xmin, bounds.xmax), y + normRand(prng, bounds.ymin, bounds.ymax),
                            prng, noise));
  }
  const int structures[] = {0, 0, 0, 0, 1};
  if (randChoice(prng, structures, 5) == 1 && theme != 4) {
    const double px = x + normRand(prng, bounds.xmin, bounds.xmax);
    const double py = y + (bounds.ymin + bounds.ymax) / 2.0 + 20.0;
    const double structureSeed = prng.next();
    const double structureWidth = normRand(prng, 160, 200);
    const double structureHeight = normRand(prng, 80, 100);
    const double structurePerspective = prng.next();
    addPavilion(inks, px, py, structureSeed, structureWidth, structureHeight, structurePerspective, prng, noise);
  }
}

}  // namespace

std::vector<Ink> hutInk(double x, double y, double height, double width, Prng& prng, Noise& noise) {
  std::vector<Ink> inks;
  addHut(inks, x, y, height, width, prng, noise);
  return inks;
}

std::vector<Ink> flatMountInk(double x, double y, double seed, Prng& prng, Noise& noise, double height, double width,
                              double cho) {
  constexpr int kRows = 5;
  constexpr int kColumns = 50;
  std::vector<std::vector<Vec2>> layers;
  std::vector<std::vector<Vec2>> flats;
  double hoff = 0;
  for (int row = 0; row < kRows; ++row) {
    hoff += (prng.next() * y) / 100.0;
    std::vector<Vec2> layer;
    std::vector<Vec2> flat;
    for (int column = 0; column < kColumns; ++column) {
      const double angle = (static_cast<double>(column) / kColumns - 0.5) * kPi;
      double rise = std::cos(angle * 2.0) + 1.0;
      rise *= noise.sample(angle + 10.0, row * 0.1, seed);
      const double scale = 1.0 - (static_cast<double>(row) / kRows) * 0.6;
      const double nx = (angle / kPi) * width * scale;
      double ny = -rise * height * scale + hoff;
      constexpr double clip = 100.0;
      if (ny < -clip * cho + hoff) {
        ny = -clip * cho + hoff;
        if (flat.size() % 2 == 0) {
          flat.push_back({nx, ny});
        }
      } else if (flat.size() % 2 == 1) {
        flat.push_back(layer.back());
      }
      layer.push_back({nx, ny});
    }
    layers.push_back(std::move(layer));
    flats.push_back(std::move(flat));
  }

  std::vector<Ink> inks;
  std::vector<Vec2> body = place(layers.front(), x, y);
  body.push_back({x, y + kRows * 4.0});
  addWhite(inks, body);
  StrokeStyle outline;
  outline.width = 3;
  outline.noise = 1;
  addMovedRibbon(inks, layers.front(), x, y, outline, gray(0.3), prng, noise);
  appendTexture(inks, layers, x, y, 80, 0, 2, flatHatch, flatSpread, prng, noise);

  std::vector<Vec2> west;
  std::vector<Vec2> east;
  for (int row = 0; row < static_cast<int>(flats.size()); row += 2) {
    if (flats[static_cast<std::size_t>(row)].size() >= 2) {
      west.push_back(flats[static_cast<std::size_t>(row)].front());
      east.push_back(flats[static_cast<std::size_t>(row)].back());
    }
  }
  if (west.empty()) {
    return inks;
  }
  const double westEdge = west.front().x;
  const double eastEdge = east.front().x;
  double westY = west.front().y;
  double eastY = east.front().y;
  for (int i = 0; i < 3; ++i) {
    const double p = 0.8 - i * 0.2;
    west.insert(west.begin(), {westEdge * p, westY - 5.0});
    east.insert(east.begin(), {eastEdge * p, eastY - 5.0});
    westY -= 5.0;
    eastY -= 5.0;
  }
  const double westFoot = west.back().x;
  const double eastFoot = east.back().x;
  double westFootY = west.back().y;
  double eastFootY = east.back().y;
  for (int i = 0; i < 3; ++i) {
    const double p = 0.6 - i * i * 0.1;
    westFootY += 1.0;
    eastFootY += 1.0;
    west.push_back({westFoot * p, westFootY});
    east.push_back({eastFoot * p, eastFootY});
  }
  west = subdivide(west, 5);
  east = subdivide(east, 5);
  std::reverse(west.begin(), west.end());
  std::vector<Vec2> crest = west;
  crest.insert(crest.end(), east.begin(), east.end());
  std::vector<int> alias(crest.size());
  for (int i = 0; i < static_cast<int>(alias.size()); ++i) {
    alias[static_cast<std::size_t>(i)] = i;
  }
  crest.push_back(crest.front());
  alias.push_back(0);
  constexpr int divisions = 5;
  for (int i = 0; i < static_cast<int>(alias.size()); ++i) {
    Vec2& point = crest[static_cast<std::size_t>(alias[static_cast<std::size_t>(i)])];
    const double v = (1.0 - std::abs((i % divisions) - divisions / 2.0) / (divisions / 2.0)) * 0.12;
    point.x *= 1.0 - v + noise.sample(point.y * 0.5) * v;
  }
  std::vector<Vec2> plateau;
  Bounds bounds;
  bool measured = false;
  for (int id : alias) {
    const Vec2& point = crest[static_cast<std::size_t>(id)];
    plateau.push_back({point.x + x, point.y + y});
    if (!measured || point.x < bounds.xmin) {
      bounds.xmin = point.x;
    }
    if (!measured || point.x > bounds.xmax) {
      bounds.xmax = point.x;
    }
    if (!measured || point.y < bounds.ymin) {
      bounds.ymin = point.y;
    }
    if (!measured || point.y > bounds.ymax) {
      bounds.ymax = point.y;
    }
    measured = true;
  }
  addWhite(inks, plateau);
  StrokeStyle rim;
  rim.width = 3;
  addMovedRibbon(inks, plateau, 0, 0, rim, gray(0.2), prng, noise);
  addFlatDecoration(inks, x, y, bounds, prng, noise);
  return inks;
}

std::vector<Piece> realizeItem(const PlanItem& item, std::size_t indexInChunk, Prng& prng, Noise& noise) {
  std::vector<Piece> pieces;
  if (item.tag == PlanTag::Mount) {
    const double seed = static_cast<double>(indexInChunk) * 2.0 * prng.next();
    const double height = 100.0 + prng.next() * 400.0;
    const double width = 400.0 + prng.next() * 200.0;
    Piece mountain;
    mountain.tag = "mount";
    mountain.x = item.x;
    mountain.y = item.y;
    mountain.sortY = item.y;
    mountain.inks = mountainInk(item.x, item.y, seed, prng, noise, height, width);
    Piece water;
    water.tag = "mount";
    water.x = item.x;
    water.y = item.y;
    water.sortY = item.y - 10000.0;
    water.inks = waterInk(item.x, item.y, prng, noise);
    pieces.push_back(std::move(mountain));
    pieces.push_back(std::move(water));
  } else if (item.tag == PlanTag::Boat) {
    const double scale = item.y / 800.0;
    const bool flip = prng.next() < 0.5;
    Piece boat;
    boat.tag = "boat";
    boat.x = item.x;
    boat.y = item.y;
    boat.sortY = item.y;
    boat.inks = boatHull(item.x, item.y, scale, flip, &prng, &noise);
    pieces.push_back(std::move(boat));
  } else if (item.tag == PlanTag::DistMount) {
    const double seed = prng.next() * 100.0;
    const int lengths[] = {500, 1000, 1500};
    const int length = randChoice(prng, lengths, 3);
    Piece ridge;
    ridge.tag = "distmount";
    ridge.x = item.x;
    ridge.y = item.y;
    ridge.sortY = item.y;
    ridge.inks = distMountInk(item.x, item.y, seed, noise, 150, length);
    pieces.push_back(std::move(ridge));
  } else if (item.tag == PlanTag::FlatMount) {
    const double seed = 2.0 * prng.next() * kPi;
    const double width = 600.0 + prng.next() * 400.0;
    const double cho = 0.5 + prng.next() * 0.2;
    Piece flat;
    flat.tag = "flatmount";
    flat.x = item.x;
    flat.y = item.y;
    flat.sortY = item.y;
    flat.inks = flatMountInk(item.x, item.y, seed, prng, noise, 100, width, cho);
    pieces.push_back(std::move(flat));
  }
  return pieces;
}

namespace {

int paperChannel(double value) {
  const long rounded = std::lround(value);
  if (rounded < 0) {
    return 0;
  }
  if (rounded > 255) {
    return 255;
  }
  return static_cast<int>(rounded);
}

Canvas buildPaper(const std::string& seed) {
  constexpr int resolution = 512;
  Canvas tile(resolution, resolution, {245, 236, 220, 1});
  Prng prng;
  prng.seed(seed);
  Noise noise(prng);
  const int half = resolution / 2 + 1;
  for (int i = 0; i < half; ++i) {
    for (int j = 0; j < half; ++j) {
      double tone = 245.0 + noise.sample(i * 0.1, j * 0.1) * 10.0;
      tone -= prng.next() * 20.0;
      const Rgba color{static_cast<std::uint8_t>(paperChannel(tone)),
                       static_cast<std::uint8_t>(paperChannel(tone * 0.95)),
                       static_cast<std::uint8_t>(paperChannel(tone * 0.85)), 1};
      const int mirrors[4][2] = {{i, j}, {resolution - i, j}, {i, resolution - j}, {resolution - i, resolution - j}};
      for (const auto& pixel : mirrors) {
        if (pixel[0] < 0 || pixel[1] < 0 || pixel[0] >= resolution || pixel[1] >= resolution) {
          continue;
        }
        tile.setPixel(pixel[0], pixel[1], color);
      }
    }
  }
  return tile;
}

}  // namespace

const Canvas& paperTile(const std::string& seed) {
  static std::string cached;
  static Canvas tile(1, 1, {245, 236, 220, 1});
  if (cached != seed) {
    cached = seed;
    tile = buildPaper(seed);
  }
  return tile;
}

void tilePaper(Canvas& dest, const Canvas& tile) {
  if (tile.width() <= 0 || tile.height() <= 0) {
    return;
  }
  for (int y = 0; y < dest.height(); y += tile.height()) {
    for (int x = 0; x < dest.width(); x += tile.width()) {
      dest.blit(x, y, tile);
    }
  }
}

void paintPieces(Canvas& canvas, const std::vector<Piece>& pieces, double originX, double originY,
                 const std::string& seed) {
  tilePaper(canvas, paperTile(seed));
  std::vector<const Piece*> order;
  order.reserve(pieces.size());
  for (const Piece& piece : pieces) {
    order.push_back(&piece);
  }
  std::stable_sort(order.begin(), order.end(),
                   [](const Piece* a, const Piece* b) { return a->sortY < b->sortY; });
  Canvas inkLayer = Canvas::transparent(canvas.width(), canvas.height());
  for (const Piece* piece : order) {
    for (const Ink& ink : piece->inks) {
      std::vector<Vec2> shifted;
      shifted.reserve(ink.polygon.size());
      for (const Vec2& point : ink.polygon) {
        shifted.push_back({point.x - originX, point.y - originY});
      }
      inkLayer.fillPolygon(shifted, ink.color);
    }
  }
  canvas.blendMultiply(inkLayer);
}

void drawPlanned(Canvas& canvas, const std::vector<PlanItem>& items, double originX, double originY, Prng& prng,
                 Noise& noise) {
  std::vector<Piece> pieces;
  for (std::size_t index = 0; index < items.size(); ++index) {
    const PlanItem& item = items[index];
    if (item.x < originX - 400.0 || item.x > originX + canvas.width() + 400.0) {
      continue;
    }
    auto made = realizeItem(item, index, prng, noise);
    pieces.insert(pieces.end(), std::make_move_iterator(made.begin()), std::make_move_iterator(made.end()));
  }
  paintPieces(canvas, pieces, originX, originY);
}

void drawStill(Canvas& canvas) {
  tilePaper(canvas, paperTile("1"));
  Prng mountainPrng;
  mountainPrng.seed("1");
  Noise mountainNoise(mountainPrng);
  auto inks = mountainInk(320, 200, 0, mountainPrng, mountainNoise, 140, 260);

  Prng waterPrng;
  waterPrng.seed("1");
  Noise waterNoise(waterPrng);
  auto waves = waterInk(80, 250, waterPrng, waterNoise);
  inks.insert(inks.end(), waves.begin(), waves.end());

  Prng boatPrng;
  boatPrng.seed("1");
  Noise boatNoise(boatPrng);
  auto boat = boatHull(470, 250, 0.6, false, &boatPrng, &boatNoise);
  inks.insert(inks.end(), boat.begin(), boat.end());

  Prng treePrng;
  treePrng.seed("1");
  Noise treeNoise(treePrng);
  auto tree = tree02(120, 210, treePrng, treeNoise);
  inks.insert(inks.end(), tree.begin(), tree.end());

  Canvas inkLayer = Canvas::transparent(canvas.width(), canvas.height());
  for (const Ink& ink : inks) {
    inkLayer.fillPolygon(ink.polygon, ink.color);
  }
  canvas.blendMultiply(inkLayer);
}
