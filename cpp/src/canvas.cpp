#include "canvas.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <stdexcept>

Canvas::Canvas(int width, int height, bool clear) : width_(width), height_(height) {
  rgb_.assign(static_cast<std::size_t>(width * height * 3), 0);
  alpha_.assign(static_cast<std::size_t>(width * height), clear ? 0 : 255);
}

Canvas::Canvas(int width, int height, Rgba paper) : Canvas(width, height, false) {
  for (int i = 0; i < width * height; ++i) {
    rgb_[static_cast<std::size_t>(i * 3 + 0)] = paper.r;
    rgb_[static_cast<std::size_t>(i * 3 + 1)] = paper.g;
    rgb_[static_cast<std::size_t>(i * 3 + 2)] = paper.b;
  }
}

Canvas Canvas::transparent(int width, int height) {
  return Canvas(width, height, true);
}

void Canvas::blend(int x, int y, std::uint8_t r, std::uint8_t g, std::uint8_t b, double srcA) {
  if (x < 0 || y < 0 || x >= width_ || y >= height_ || srcA <= 0.0) {
    return;
  }
  if (srcA > 1.0) {
    srcA = 1.0;
  }
  const std::size_t index = static_cast<std::size_t>(y * width_ + x);
  const double dstA = alpha_[index] / 255.0;
  const double outA = srcA + dstA * (1.0 - srcA);
  std::uint8_t* px = &rgb_[index * 3];
  if (outA <= 0.0) {
    alpha_[index] = 0;
    return;
  }
  const auto channel = [&](std::uint8_t src, std::uint8_t dst) {
    return static_cast<std::uint8_t>(std::lround((src * srcA + dst * dstA * (1.0 - srcA)) / outA));
  };
  px[0] = channel(r, px[0]);
  px[1] = channel(g, px[1]);
  px[2] = channel(b, px[2]);
  alpha_[index] = static_cast<std::uint8_t>(std::lround(outA * 255.0));
}

void Canvas::setPixel(int x, int y, Rgba color) {
  blend(x, y, color.r, color.g, color.b, color.a);
}

void Canvas::fillPolygon(const std::vector<Vec2>& polygon, Rgba color) {
  if (polygon.size() < 3) {
    return;
  }
  std::size_t count = polygon.size();
  if (polygon.front().x == polygon.back().x && polygon.front().y == polygon.back().y) {
    --count;
  }

  double minY = polygon[0].y;
  double maxY = polygon[0].y;
  for (std::size_t i = 1; i < count; ++i) {
    minY = std::min(minY, polygon[i].y);
    maxY = std::max(maxY, polygon[i].y);
  }

  const int y0 = std::max(0, static_cast<int>(std::floor(minY)));
  const int y1 = std::min(height_ - 1, static_cast<int>(std::ceil(maxY)));
  for (int y = y0; y <= y1; ++y) {
    const double scan = static_cast<double>(y) + 0.5;
    std::vector<double> crossings;
    for (std::size_t i = 0; i < count; ++i) {
      const Vec2& a = polygon[i];
      const Vec2& b = polygon[(i + 1) % count];
      const bool crosses = (a.y <= scan && b.y > scan) || (b.y <= scan && a.y > scan);
      if (!crosses) {
        continue;
      }
      const double t = (scan - a.y) / (b.y - a.y);
      crossings.push_back(a.x + t * (b.x - a.x));
    }
    std::sort(crossings.begin(), crossings.end());
    for (std::size_t i = 0; i + 1 < crossings.size(); i += 2) {
      int x0 = std::max(0, static_cast<int>(std::ceil(crossings[i])));
      int x1 = std::min(width_ - 1, static_cast<int>(std::floor(crossings[i + 1])));
      for (int x = x0; x <= x1; ++x) {
        blend(x, y, color.r, color.g, color.b, color.a);
      }
    }
  }
}

void Canvas::blendMultiply(const Canvas& ink) {
  const int width = std::min(width_, ink.width_);
  const int height = std::min(height_, ink.height_);
  for (int y = 0; y < height; ++y) {
    for (int x = 0; x < width; ++x) {
      const std::size_t src = static_cast<std::size_t>(y * ink.width_ + x);
      const double alpha = ink.alpha_[src] / 255.0;
      if (alpha <= 0.0) {
        continue;
      }
      const std::size_t dst = static_cast<std::size_t>(y * width_ + x);
      for (int channel = 0; channel < 3; ++channel) {
        const double paper = rgb_[dst * 3 + channel];
        const double color = ink.rgb_[src * 3 + channel] / 255.0;
        rgb_[dst * 3 + channel] =
            static_cast<std::uint8_t>(std::lround(paper * (1.0 - alpha) + paper * color * alpha));
      }
      alpha_[dst] = 255;
    }
  }
}

Rgba Canvas::pixel(int x, int y) const {
  const std::size_t index = static_cast<std::size_t>(y * width_ + x);
  return {rgb_[index * 3], rgb_[index * 3 + 1], rgb_[index * 3 + 2], alpha_[index] / 255.0};
}

void Canvas::blit(int destX, int destY, const Canvas& source) {
  for (int y = 0; y < source.height_; ++y) {
    const int targetY = destY + y;
    if (targetY < 0 || targetY >= height_) {
      continue;
    }
    for (int x = 0; x < source.width_; ++x) {
      const int targetX = destX + x;
      if (targetX < 0 || targetX >= width_) {
        continue;
      }
      const std::size_t index = static_cast<std::size_t>(y * source.width_ + x);
      if (source.alpha_[index] == 0) {
        continue;
      }
      const std::uint8_t* px = &source.rgb_[index * 3];
      blend(targetX, targetY, px[0], px[1], px[2], source.alpha_[index] / 255.0);
    }
  }
}

void Canvas::copyBgr(std::vector<std::uint8_t>& dest) const {
  dest.resize(static_cast<std::size_t>(width_ * height_ * 3));
  for (int i = 0; i < width_ * height_; ++i) {
    dest[static_cast<std::size_t>(i * 3 + 0)] = rgb_[static_cast<std::size_t>(i * 3 + 2)];
    dest[static_cast<std::size_t>(i * 3 + 1)] = rgb_[static_cast<std::size_t>(i * 3 + 1)];
    dest[static_cast<std::size_t>(i * 3 + 2)] = rgb_[static_cast<std::size_t>(i * 3 + 0)];
  }
}

std::uint64_t Canvas::checksum() const {
  std::uint64_t hash = 14695981039346656037ull;
  for (std::uint8_t byte : rgb_) {
    hash ^= byte;
    hash *= 1099511628211ull;
  }
  return hash;
}

void Canvas::writeBmp(const std::string& path) const {
  const int rowStride = (width_ * 3 + 3) & ~3;
  const auto pixelBytes = static_cast<std::uint32_t>(rowStride * height_);
  const std::uint32_t fileSize = 54 + pixelBytes;
  std::ofstream out(path, std::ios::binary);
  if (!out) {
    throw std::runtime_error("nao foi possivel criar " + path);
  }
  const unsigned char header[54] = {
      'B', 'M',
      static_cast<unsigned char>(fileSize), static_cast<unsigned char>(fileSize >> 8),
      static_cast<unsigned char>(fileSize >> 16), static_cast<unsigned char>(fileSize >> 24),
      0, 0, 0, 0, 54, 0, 0, 0, 40, 0, 0, 0,
      static_cast<unsigned char>(width_), static_cast<unsigned char>(width_ >> 8),
      static_cast<unsigned char>(width_ >> 16), static_cast<unsigned char>(width_ >> 24),
      static_cast<unsigned char>(height_), static_cast<unsigned char>(height_ >> 8),
      static_cast<unsigned char>(height_ >> 16), static_cast<unsigned char>(height_ >> 24),
      1, 0, 24, 0, 0, 0, 0, 0,
      static_cast<unsigned char>(pixelBytes), static_cast<unsigned char>(pixelBytes >> 8),
      static_cast<unsigned char>(pixelBytes >> 16), static_cast<unsigned char>(pixelBytes >> 24),
      0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
  out.write(reinterpret_cast<const char*>(header), 54);
  std::vector<char> row(static_cast<std::size_t>(rowStride), 0);
  for (int y = height_ - 1; y >= 0; --y) {
    for (int x = 0; x < width_; ++x) {
      const std::size_t i = static_cast<std::size_t>((y * width_ + x) * 3);
      row[static_cast<std::size_t>(x * 3 + 0)] = static_cast<char>(rgb_[i + 2]);
      row[static_cast<std::size_t>(x * 3 + 1)] = static_cast<char>(rgb_[i + 1]);
      row[static_cast<std::size_t>(x * 3 + 2)] = static_cast<char>(rgb_[i + 0]);
    }
    out.write(row.data(), rowStride);
  }
}
