#pragma once

#include "canvas.hpp"
#include "geom.hpp"
#include "noise.hpp"
#include "planner.hpp"
#include "prng.hpp"

#include <string>
#include <vector>

struct Ink {
  std::vector<Vec2> polygon;
  Rgba color;
};

std::vector<Ink> tree02(double x, double y, Prng& prng, Noise& noise, int clusters = 5, double height = 16,
                        double width = 8, double alpha = 0.5);
std::vector<Ink> waterInk(double x, double y, Prng& prng, Noise& noise);
std::vector<Ink> boatHull(double x, double y, double scale = 1, bool flip = false, Prng* prng = nullptr,
                          Noise* noise = nullptr);
std::vector<Ink> mountainInk(double x, double y, double seed, Prng& prng, Noise& noise, double height,
                             double width);
std::vector<Ink> distMountInk(double x, double y, double seed, Noise& noise, double height, int length);
std::vector<Ink> flatMountInk(double x, double y, double seed, Prng& prng, Noise& noise, double height, double width,
                              double cho);
std::vector<Ink> hutInk(double x, double y, double height, double width, Prng& prng, Noise& noise);

struct Piece {
  const char* tag = "";
  double x = 0;
  double y = 0;
  double sortY = 0;
  std::vector<Ink> inks;
};

std::vector<Piece> realizeItem(const PlanItem& item, std::size_t indexInChunk, Prng& prng, Noise& noise);
void paintPieces(Canvas& canvas, const std::vector<Piece>& pieces, double originX, double originY,
                 const std::string& seed = "1");
const Canvas& paperTile(const std::string& seed);
void tilePaper(Canvas& dest, const Canvas& tile);

void drawStill(Canvas& canvas);
void drawPlanned(Canvas& canvas, const std::vector<PlanItem>& items, double originX, double originY, Prng& prng,
                 Noise& noise);
Vec2 mountainRidgePoint(Prng& prng, Noise& noise, double yOffset, double height, double width);
std::vector<std::vector<Vec2>> textureCenters(const std::vector<std::vector<Vec2>>& layers, int count, Prng& prng,
                                             Noise& noise, double (*spread)(Prng&) = nullptr, double span = 0.2,
                                             double wobble = -1);
std::vector<std::vector<Vec2>> footLines(const std::vector<std::vector<Vec2>>& layers, double xOffset, Prng& prng,
                                        Noise& noise);
