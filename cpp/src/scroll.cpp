#include "scroll.hpp"

#include "canvas.hpp"
#include "generator.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

namespace {

constexpr int kViewWidth = 3000;
constexpr int kViewHeight = 800;
constexpr double kWorldScale = 1.142;
constexpr double kCameraStart = 0;
constexpr double kForgetMargin = 1600;
constexpr double kLookAhead = 1800;
constexpr Rgba kPaper{245, 236, 220, 1};

struct Sprite {
  double sortY = 0;
  double left = 0;
  double top = 0;
  Canvas image;
};

Vec2 quantizeForSvg(Vec2 point) {
  return {std::round(point.x * 10.0) / 10.0, std::round(point.y * 10.0) / 10.0};
}

Sprite rasterize(const Piece& piece) {
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
  Sprite sprite{piece.sortY, 0, 0, Canvas::transparent(1, 1)};
  if (!any) {
    return sprite;
  }
  const double left = minX - 2.0;
  const double top = minY - 2.0;
  const int width = std::max(1, static_cast<int>(std::ceil((maxX - minX + 4.0) * kWorldScale)));
  const int height = std::max(1, static_cast<int>(std::ceil((maxY - minY + 4.0) * kWorldScale)));
  sprite.left = left * kWorldScale;
  sprite.top = top * kWorldScale;
  sprite.image = Canvas::transparent(width, height);
  for (const Ink& ink : piece.inks) {
    std::vector<Vec2> shifted;
    shifted.reserve(ink.polygon.size());
    for (const Vec2& rawPoint : ink.polygon) {
      const Vec2 point = quantizeForSvg(rawPoint);
      shifted.push_back({(point.x - left) * kWorldScale, (point.y - top) * kWorldScale});
    }
    sprite.image.fillPolygon(shifted, ink.color);
    sprite.image.strokePolygon(shifted, ink.color, ink.outlineWidth);
  }
  return sprite;
}

void compose(Canvas& frame, const std::vector<Sprite>& sprites, double camera, const Canvas& paper) {
  tilePaper(frame, paper);
  Canvas ink = Canvas::transparent(frame.width(), frame.height());
  for (const Sprite& sprite : sprites) {
    const int x = static_cast<int>(std::lround(sprite.left - camera * kWorldScale));
    const int y = static_cast<int>(std::lround(sprite.top));
    ink.blit(x, y, sprite.image);
  }
  frame.blendMultiply(ink);
}

void forgetBehind(std::vector<Sprite>& sprites, double camera) {
  const double limit = (camera - kForgetMargin) * kWorldScale;
  sprites.erase(std::remove_if(sprites.begin(), sprites.end(),
                               [&](const Sprite& sprite) {
                                 return sprite.left + sprite.image.width() < limit;
                               }),
                sprites.end());
}

LRESULT CALLBACK scrollProc(HWND window, UINT message, WPARAM wparam, LPARAM lparam) {
  if (message == WM_CLOSE) {
    DestroyWindow(window);
    return 0;
  }
  if (message == WM_DESTROY) {
    PostQuitMessage(0);
    return 0;
  }
  if (message == WM_KEYDOWN && wparam == VK_ESCAPE) {
    DestroyWindow(window);
    return 0;
  }
  return DefWindowProc(window, message, wparam, lparam);
}

void present(HWND window, const Canvas& frame, std::vector<std::uint8_t>& dib) {
  BITMAPINFO info{};
  info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
  info.bmiHeader.biWidth = frame.width();
  info.bmiHeader.biHeight = -frame.height();
  info.bmiHeader.biPlanes = 1;
  info.bmiHeader.biBitCount = 24;
  info.bmiHeader.biCompression = BI_RGB;
  HDC dc = GetDC(window);
  StretchDIBits(dc, 0, 0, frame.width(), frame.height(), 0, 0, frame.width(), frame.height(), dib.data(), &info,
                DIB_RGB_COLORS, SRCCOPY);
  ReleaseDC(window, dc);
}

}  // namespace

bool playScroll(const std::vector<Piece>& pieces, const std::string& seed, double speed, Generator* live) {
  std::vector<Sprite> sprites;
  sprites.reserve(pieces.size());
  for (const Piece& piece : pieces) {
    const auto started = std::chrono::steady_clock::now();
    sprites.push_back(rasterize(piece));
    const double ms =
        std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - started).count();
    std::cout << "render " << piece.tag << " " << ms << " ms | scroll " << speed << " px/s\n";
  }
  std::stable_sort(sprites.begin(), sprites.end(),
                   [](const Sprite& a, const Sprite& b) { return a.sortY < b.sortY; });

  const Canvas& paper = paperTile(seed);
  Canvas first(kViewWidth, kViewHeight, kPaper);
  compose(first, sprites, kCameraStart, paper);
  Canvas later(kViewWidth, kViewHeight, kPaper);
  compose(later, sprites, kCameraStart + speed, paper);
  const bool moved = first.checksum() != later.checksum();
  const bool scrollOk = speed == 0.0 ? !moved : moved;
  first.writeBmp("scroll-check.bmp");
  std::cout << (scrollOk ? "scroll match\n" : "scroll mismatch\n");
  std::cout << "ok scroll-check.bmp\n";

  WNDCLASSW windowClass{};
  windowClass.lpfnWndProc = scrollProc;
  windowClass.hInstance = GetModuleHandleW(nullptr);
  windowClass.lpszClassName = L"ShanShuiScroll";
  windowClass.hCursor = LoadCursor(nullptr, IDC_ARROW);
  RegisterClassW(&windowClass);

  RECT bounds{0, 0, kViewWidth, kViewHeight};
  AdjustWindowRect(&bounds, WS_OVERLAPPEDWINDOW, FALSE);
  HWND window = CreateWindowW(L"ShanShuiScroll", L"shan shui", WS_OVERLAPPEDWINDOW | WS_VISIBLE, CW_USEDEFAULT,
                              CW_USEDEFAULT, bounds.right - bounds.left, bounds.bottom - bounds.top, nullptr, nullptr,
                              windowClass.hInstance, nullptr);
  if (window == nullptr) {
    std::cout << "janela indisponivel\n";
    return scrollOk;
  }

  std::vector<std::uint8_t> dib(static_cast<std::size_t>(kViewWidth * kViewHeight * 3));
  double camera = kCameraStart;
  auto previous = std::chrono::steady_clock::now();
  bool quit = false;
  while (!quit) {
    MSG message;
    while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
      if (message.message == WM_QUIT) {
        quit = true;
        break;
      }
      TranslateMessage(&message);
      DispatchMessageW(&message);
    }
    if (quit) {
      break;
    }
    const auto now = std::chrono::steady_clock::now();
    double dt = std::chrono::duration<double>(now - previous).count();
    previous = now;
    if (dt > 0.034) {
      dt = 0.034;
    }
    camera += speed * dt;
    if (live != nullptr) {
      if (live->exhausted() && camera + kViewWidth / kWorldScale + kLookAhead > live->plan().xmax()) {
        live->extend(live->plan().xmax());
      }
      int drawn = 0;
      GenEvent event;
      while (drawn < 2 && live->poll(event)) {
        if (event.kind == GenEvent::Kind::Timing) {
          std::ostringstream line;
          line << std::fixed << std::setprecision(1);
          line << "frame " << event.tag << " " << event.ms << " ms | xmax " << static_cast<long long>(event.xmax);
          std::cout << line.str() << "\n";
          continue;
        }
        Sprite sprite = rasterize(event.piece);
        const auto at = std::lower_bound(sprites.begin(), sprites.end(), sprite.sortY,
                                          [](const Sprite& item, double sortY) { return item.sortY < sortY; });
        sprites.insert(at, std::move(sprite));
        drawn += 1;
      }
    }
    forgetBehind(sprites, camera);
    Canvas frame(kViewWidth, kViewHeight, kPaper);
    compose(frame, sprites, camera, paper);
    frame.copyBgr(dib);
    present(window, frame, dib);
    Sleep(1);
  }
  if (IsWindow(window)) {
    DestroyWindow(window);
  }
  return scrollOk;
}
