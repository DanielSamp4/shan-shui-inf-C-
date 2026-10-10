#pragma once

#include "geom.hpp"

#include <cstdint>
#include <string>
#include <vector>

struct Rgba {
  std::uint8_t r = 0;
  std::uint8_t g = 0;
  std::uint8_t b = 0;
  double a = 1;
};

class Canvas {
 public:
  Canvas(int width, int height, Rgba paper = {255, 255, 255, 1});
  static Canvas transparent(int width, int height);

  void setPixel(int x, int y, Rgba color);
  void fillPolygon(const std::vector<Vec2>& polygon, Rgba color);
  void strokePolygon(const std::vector<Vec2>& polygon, Rgba color, double width);
  void blit(int destX, int destY, const Canvas& source);
  void blendMultiply(const Canvas& ink);
  Rgba pixel(int x, int y) const;
  void writeBmp(const std::string& path) const;
  void copyBgr(std::vector<std::uint8_t>& dest) const;
  std::uint64_t checksum() const;

  int width() const { return width_; }
  int height() const { return height_; }

 private:
  Canvas(int width, int height, bool clear);

  void blend(int x, int y, std::uint8_t r, std::uint8_t g, std::uint8_t b, double alpha);

  int width_ = 0;
  int height_ = 0;
  std::vector<std::uint8_t> rgb_;
  std::vector<std::uint8_t> alpha_;
};
