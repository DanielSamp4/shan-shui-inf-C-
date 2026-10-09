#include "stroke.hpp"

#include <cmath>
#include <vector>

std::vector<Vec2> strokeRibbon(const std::vector<Vec2>& points, Prng& prng, Noise& noise,
                               const StrokeStyle& style) {
  std::vector<Vec2> ribbon;
  if (points.empty()) {
    return ribbon;
  }

  std::vector<Vec2> left;
  std::vector<Vec2> right;
  const double n0 = prng.next() * 10.0;
  const double count = static_cast<double>(points.size());
  for (std::size_t i = 1; i + 1 < points.size(); ++i) {
    const double t = static_cast<double>(i) / count;
    const double shaped = style.widthFn ? style.widthFn(t) : std::sin(t * 3.14159265358979323846);
    double w = style.width * shaped;
    w = w * (1.0 - style.noise) + w * style.noise * noise.sample(static_cast<double>(i) * 0.5, n0);
    const double a1 = std::atan2(points[i].y - points[i - 1].y, points[i].x - points[i - 1].x);
    const double a2 = std::atan2(points[i].y - points[i + 1].y, points[i].x - points[i + 1].x);
    double a = (a1 + a2) / 2.0;
    if (a < a2) {
      a += 3.14159265358979323846;
    }
    left.push_back({points[i].x + w * std::cos(a), points[i].y + w * std::sin(a)});
    right.push_back({points[i].x - w * std::cos(a), points[i].y - w * std::sin(a)});
  }

  ribbon.push_back({points.front().x + style.xOffset, points.front().y + style.yOffset});
  for (const Vec2& p : left) {
    ribbon.push_back({p.x + style.xOffset, p.y + style.yOffset});
  }
  ribbon.push_back({points.back().x + style.xOffset, points.back().y + style.yOffset});
  for (auto it = right.rbegin(); it != right.rend(); ++it) {
    ribbon.push_back({it->x + style.xOffset, it->y + style.yOffset});
  }
  ribbon.push_back(ribbon.front());
  return ribbon;
}

std::vector<Vec2> solidRibbon(const std::vector<Vec2>& points, double width) {
  std::vector<Vec2> ribbon;
  if (points.size() < 2) {
    return ribbon;
  }
  const double half = width * 0.5;
  std::vector<Vec2> left;
  std::vector<Vec2> right;
  constexpr double kPi = 3.14159265358979323846;
  for (std::size_t i = 1; i + 1 < points.size(); ++i) {
    const double a1 = std::atan2(points[i].y - points[i - 1].y, points[i].x - points[i - 1].x);
    const double a2 = std::atan2(points[i].y - points[i + 1].y, points[i].x - points[i + 1].x);
    double a = (a1 + a2) / 2.0;
    if (a < a2) {
      a += kPi;
    }
    left.push_back({points[i].x + half * std::cos(a), points[i].y + half * std::sin(a)});
    right.push_back({points[i].x - half * std::cos(a), points[i].y - half * std::sin(a)});
  }
  ribbon.push_back(points.front());
  ribbon.insert(ribbon.end(), left.begin(), left.end());
  ribbon.push_back(points.back());
  for (auto it = right.rbegin(); it != right.rend(); ++it) {
    ribbon.push_back(*it);
  }
  ribbon.push_back(ribbon.front());
  return ribbon;
}
