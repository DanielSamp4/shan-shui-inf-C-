#include "raster.hpp"

#include <algorithm>
#include <cmath>
#include <vector>

namespace {

Vec2 quantizeForSvg(Vec2 point) {
  return {std::round(point.x * 10.0) / 10.0, std::round(point.y * 10.0) / 10.0};
}

}  // namespace

RasterizedPiece rasterizePiece(const Piece& piece) {
  double minX = 0;
  double minY = 0;
  double maxX = 0;
  double maxY = 0;
  bool any = false;
  for (const Ink& ink : piece.inks) {
    for (const Vec2& rawPoint : ink.polygon) {
      const Vec2 point = quantizeForSvg(rawPoint);
      if (!any) {
        minX = maxX = point.x;
        minY = maxY = point.y;
        any = true;
      } else {
        minX = std::min(minX, point.x);
        minY = std::min(minY, point.y);
        maxX = std::max(maxX, point.x);
        maxY = std::max(maxY, point.y);
      }
    }
  }
  RasterizedPiece sprite;
  sprite.sortY = piece.sortY;
  if (!any) return sprite;

  const double left = minX - 2.0;
  const double top = minY - 2.0;
  const int width = std::max(1, static_cast<int>(std::ceil((maxX - minX + 4.0) * kRasterWorldScale)));
  const int height = std::max(1, static_cast<int>(std::ceil((maxY - minY + 4.0) * kRasterWorldScale)));
  sprite.left = left * kRasterWorldScale;
  sprite.top = top * kRasterWorldScale;
  sprite.image = Canvas::transparent(width, height);
  for (const Ink& ink : piece.inks) {
    std::vector<Vec2> shifted;
    shifted.reserve(ink.polygon.size());
    for (const Vec2& rawPoint : ink.polygon) {
      const Vec2 point = quantizeForSvg(rawPoint);
      shifted.push_back({(point.x - left) * kRasterWorldScale, (point.y - top) * kRasterWorldScale});
    }
    sprite.image.fillPolygon(shifted, ink.color);
    // SVG scales stroke-width together with its viewBox; mirror that scale in
    // the pre-rasterized texture.
    sprite.image.strokePolygon(shifted, ink.color, ink.outlineWidth * kRasterWorldScale);
  }
  return sprite;
}
