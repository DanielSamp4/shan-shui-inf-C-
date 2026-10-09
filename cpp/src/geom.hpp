#pragma once

struct Vec2 {
  double x = 0;
  double y = 0;
};

inline Vec2 midPoint(Vec2 a, Vec2 b) {
  return {(a.x + b.x) / 2.0, (a.y + b.y) / 2.0};
}
