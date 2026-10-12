#pragma once

#include "canvas.hpp"
#include "scenery.hpp"

// Shared CPU representation of one GPU-ready scene layer.  It is created by
// the generator thread and consumed by the presentation thread.
struct RasterizedPiece {
  double sortY = 0;
  double left = 0;
  double top = 0;
  Canvas image = Canvas::transparent(1, 1);
};

constexpr double kRasterWorldScale = 1.142;

RasterizedPiece rasterizePiece(const Piece& piece);
