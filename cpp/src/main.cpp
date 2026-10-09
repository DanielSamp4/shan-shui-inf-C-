#include "canvas.hpp"
#include "generator.hpp"
#include "scroll.hpp"
#include "noise.hpp"
#include "planner.hpp"
#include "prng.hpp"
#include "scenery.hpp"
#include "stroke.hpp"

#include <cmath>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace {

bool near(double got, double expected) {
  return std::abs(got - expected) <= 1e-12;
}

bool samePieces(const std::vector<Piece>& got, const std::vector<Piece>& want) {
  if (got.size() != want.size()) {
    std::cout << "pieces " << got.size() << " want " << want.size() << "\n";
    return false;
  }
  for (std::size_t i = 0; i < got.size(); ++i) {
    const Piece& a = got[i];
    const Piece& b = want[i];
    if (std::string(a.tag) != b.tag || a.x != b.x || a.y != b.y || a.sortY != b.sortY ||
        a.inks.size() != b.inks.size()) {
      std::cout << "piece " << i << " " << a.tag << " " << a.x << " " << a.y << "\n";
      return false;
    }
    for (std::size_t k = 0; k < a.inks.size(); ++k) {
      if (a.inks[k].polygon.size() != b.inks[k].polygon.size()) {
        return false;
      }
      for (std::size_t p = 0; p < a.inks[k].polygon.size(); ++p) {
        if (a.inks[k].polygon[p].x != b.inks[k].polygon[p].x || a.inks[k].polygon[p].y != b.inks[k].polygon[p].y) {
          std::cout << "piece " << i << " ink " << k << " drifted\n";
          return false;
        }
      }
    }
  }
  return true;
}

}  // namespace

int main(int argc, char** argv) {
  const std::string seed = argc > 1 ? argv[1] : "1";
  Prng prng;
  const double initial = prng.seed(seed);

  double randoms[5];
  for (double& value : randoms) {
    value = prng.next();
  }

  Prng noisePrng;
  noisePrng.seed(seed);
  Noise noise(noisePrng);
  const double samples[4] = {
      noise.sample(0.1, 0.2),
      noise.sample(1, 2, 3),
      noise.sample(10),
      noise.sample(-4.5, 0.25, 1.5),
  };

  std::cout << "int seed " << static_cast<long long>(initial) << "\n";
  for (double value : randoms) {
    std::cout << value << "\n";
  }
  for (double value : samples) {
    std::cout << value << "\n";
  }

  if (seed == "1") {
    const double expectRandom[5] = {
        0.685960962923703, 0.3766400183202357, 0.18146060929237168, 0.31050111786563,
        0.34512043355926725};
    const double expectNoise[4] = {
        0.69029834023201, 0.5412861555979861, 0.5810559735121189, 0.5801790647774808};
    bool ok = initial == 221345097.0;
    for (int i = 0; i < 5; ++i) {
      ok = ok && near(randoms[i], expectRandom[i]);
    }
    for (int i = 0; i < 4; ++i) {
      ok = ok && near(samples[i], expectNoise[i]);
    }
    std::cout << (ok ? "match\n" : "mismatch\n");
    if (!ok) {
      return 1;
    }
  }

  Canvas noiseImage(128, 64);
  for (int y = 0; y < noiseImage.height(); ++y) {
    for (int x = 0; x < noiseImage.width(); ++x) {
      const double n = noise.sample(x * 0.08, y * 0.08);
      const auto shade = static_cast<std::uint8_t>(std::lround(n * 255.0));
      noiseImage.setPixel(x, y, {shade, shade, shade, 1});
    }
  }
  noiseImage.writeBmp("noise-check.bmp");

  Prng strokePrng;
  strokePrng.seed("1");
  Noise strokeNoise(strokePrng);
  const std::vector<Vec2> spine = {{10, 40}, {30, 20}, {50, 35}, {80, 25}, {110, 40}};
  const std::vector<Vec2> ribbon = strokeRibbon(spine, strokePrng, strokeNoise);
  const char* expect =
      "10.0,40.0 29.9,19.1 50.2,33.7 80.1,23.6 110.0,40.0 79.9,26.4 49.8,36.3 30.1,20.9 10.0,40.0";
  std::ostringstream got;
  got << std::fixed << std::setprecision(1);
  for (std::size_t i = 0; i < ribbon.size(); ++i) {
    if (i) {
      got << " ";
    }
    got << ribbon[i].x << "," << ribbon[i].y;
  }
  const bool strokeOk = got.str() == expect;
  std::cout << got.str() << "\n";
  std::cout << (strokeOk ? "stroke match\n" : "stroke mismatch\n");

  Canvas picture(160, 80, {245, 236, 220, 1});
  picture.fillPolygon(ribbon, {200, 200, 200, 0.9});
  picture.writeBmp("stroke-check.bmp");
  std::cout << "ok stroke-check.bmp\n";

  Prng ridgePrng;
  ridgePrng.seed("1");
  Noise ridgeNoise(ridgePrng);
  const Vec2 ridge = mountainRidgePoint(ridgePrng, ridgeNoise, 100, 120, 220);
  const bool ridgeOk = std::abs(ridge.x) < 1e-9 && std::abs(ridge.y - (-61.060464293852576)) < 1e-9;
  std::cout << "ridge " << ridge.x << " " << ridge.y << "\n";
  std::cout << (ridgeOk ? "mountain match\n" : "mountain mismatch\n");

  std::vector<std::vector<Vec2>> grid(10);
  for (int layer = 0; layer < 10; ++layer) {
    for (int i = 0; i < 50; ++i) {
      grid[static_cast<std::size_t>(layer)].push_back({i * 3.0, layer * 4.0 - 10.0});
    }
  }
  Prng texturePrng;
  texturePrng.seed("1");
  Noise textureNoise(texturePrng);
  const auto texture = textureCenters(grid, 200, texturePrng, textureNoise);
  const bool textureOk = texture[0].size() == 2 && texture[1].size() == 14 && texture[7].size() == 6 &&
                         std::abs(texture[0].front().x - 12.200016850408034) < 1e-9 &&
                         std::abs(texture[0].front().y - (-13.982341224069819)) < 1e-9 &&
                         std::abs(texture[0].back().x - 17.860947423167346) < 1e-9 &&
                         std::abs(texture[1].front().x - 83.98575402364747) < 1e-9 &&
                         std::abs(texture[7].back().y - (-10.597695676577711)) < 1e-9;
  std::cout << (textureOk ? "texture match\n" : "texture mismatch\n");

  Prng footPrng;
  footPrng.seed("1");
  Noise footNoise(footPrng);
  const auto feet = footLines(grid, 0, footPrng, footNoise);
  const bool footOk = feet.size() == 12 && feet.front().size() == 17 &&
                      std::abs(feet.front().front().x - 20.61240283774229) < 1e-9 &&
                      std::abs(feet.front().front().y - (-10.0)) < 1e-9 &&
                      std::abs(feet.front().back().x) < 1e-9 &&
                      std::abs(feet.front().back().y - (-0.2022238061170647)) < 1e-9;
  std::cout << (footOk ? "foot match\n" : "foot mismatch\n");

  Prng distantPrng;
  distantPrng.seed("1");
  Noise distantNoise(distantPrng);
  const auto distant = distMountInk(0, 280, 0, distantNoise, 150, 500);
  const bool distantOk = distant.size() == 417 && distant.front().polygon.size() == 10 &&
                         std::abs(distant.front().polygon.front().x - 60.0) < 1e-9 &&
                         std::abs(distant.front().polygon.front().y - 285.1714175149395) < 1e-9 &&
                         std::abs(distant.front().polygon.back().x - 50.0) < 1e-9 &&
                         std::abs(distant.front().polygon.back().y - 237.3426585479178) < 1e-9;
  std::cout << (distantOk ? "distant match\n" : "distant mismatch\n");

  Prng flatPrng;
  flatPrng.seed("1");
  Noise flatNoise(flatPrng);
  const auto flat = flatMountInk(0, 280, 0, flatPrng, flatNoise, 100, 600, 0.5);
  const Ink* plateau = nullptr;
  for (const Ink& ink : flat) {
    if (ink.polygon.size() == 83) {
      plateau = &ink;
      break;
    }
  }
  const bool flatOk = !flat.empty() && flat.front().polygon.size() == 51 &&
                      std::abs(flat.front().polygon.front().x - (-300.0)) < 1e-9 &&
                      std::abs(flat.front().polygon.front().y - 281.92069069618637) < 1e-9 &&
                      std::abs(flat.front().polygon.back().x) < 1e-9 &&
                      std::abs(flat.front().polygon.back().y - 300.0) < 1e-9 && plateau != nullptr &&
                      std::abs(plateau->polygon.front().x - (-3.6225363536755597)) < 1e-9 &&
                      std::abs(plateau->polygon.front().y - 238.86449679438456) < 1e-9;
  std::cout << (flatOk ? "flat match\n" : "flat mismatch\n");

  Prng hutPrng;
  hutPrng.seed("1");
  Noise hutNoise(hutPrng);
  const auto hut = hutInk(0, 200, 40, 180, hutPrng, hutNoise);
  const bool hutOk = !hut.empty() && hut.front().polygon.size() > 5 &&
                     std::abs(hut.front().polygon[5].x - (-59.64193922626064)) < 1e-9 &&
                     std::abs(hut.front().polygon[5].y - 225.27093761299423) < 1e-9;
  std::cout << (hutOk ? "pavilion match\n" : "pavilion mismatch\n");

  Prng boatPrng;
  boatPrng.seed("1");
  Noise boatNoise(boatPrng);
  const auto ridden = boatHull(0, 250, 1, false, &boatPrng, &boatNoise);
  const Ink* rider = nullptr;
  for (const Ink& ink : ridden) {
    if (ink.color.r == 255 && ink.color.g == 255 && ink.color.b == 255 && ink.color.a == 1) {
      rider = &ink;
      break;
    }
  }
  const bool boatOk = rider != nullptr && rider->polygon.size() == 42 &&
                      std::abs(rider->polygon.front().x - 18.621479708864104) < 1e-9 &&
                      std::abs(rider->polygon.front().y - 228.12988409395874) < 1e-9 &&
                      std::abs(rider->polygon.back().x - 21.378520291135896) < 1e-9 &&
                      std::abs(rider->polygon.back().y - 226.5054681288481) < 1e-9;
  std::cout << (boatOk ? "boat match\n" : "boat mismatch\n");

  const Canvas& grain = paperTile("1");
  const Rgba grainOrigin = grain.pixel(0, 0);
  const Rgba grainInner = grain.pixel(10, 20);
  const Rgba grainMirror = grain.pixel(400, 20);
  const bool paperOk = grainOrigin.r == 243 && grainOrigin.g == 231 && grainOrigin.b == 207 &&
                       grainInner.r == 244 && grainInner.g == 232 && grainInner.b == 208 &&
                       grainMirror.r == 235 && grainMirror.g == 223 && grainMirror.b == 200;
  std::cout << (paperOk ? "paper match\n" : "paper mismatch\n");
  if (!paperOk) {
    std::cout << "paper " << static_cast<int>(grainOrigin.r) << " " << static_cast<int>(grainOrigin.g) << " "
              << static_cast<int>(grainOrigin.b) << "\n";
  }
  if (!boatOk && rider != nullptr && !rider->polygon.empty()) {
    std::cout << "boat " << rider->polygon.size() << " " << rider->polygon.front().x << " " << rider->polygon.front().y
              << " " << rider->polygon.back().x << " " << rider->polygon.back().y << "\n";
  }
  if (!hutOk && !hut.empty() && hut.front().polygon.size() > 5) {
    std::cout << "pavilion " << hut.front().polygon[5].x << " " << hut.front().polygon[5].y << "\n";
  }
  if (!flatOk && !flat.empty() && !flat.front().polygon.empty()) {
    std::cout << "flat body " << flat.front().polygon.size() << " " << flat.front().polygon.front().x << " "
              << flat.front().polygon.front().y << "\n";
    if (plateau != nullptr) {
      std::cout << "flat top " << plateau->polygon.front().x << " " << plateau->polygon.front().y << "\n";
    }
  }
  if (!distantOk && !distant.empty() && !distant.front().polygon.empty()) {
    std::cout << "distant " << distant.size() << " " << distant.front().polygon.front().x << " "
              << distant.front().polygon.front().y << "\n";
  }
  if (!footOk && !feet.empty() && !feet.front().empty()) {
    std::cout << "foot " << feet.front().front().x << " " << feet.front().front().y << " n " << feet.front().size()
              << "\n";
  }
  if (!textureOk && !texture.empty() && !texture[0].empty()) {
    std::cout << "texture " << texture[0].front().x << " " << texture[0].front().y << "\n";
  }

  Canvas multiplyBackdrop(2, 1, {245, 236, 220, 1});
  Canvas multiplyInk = Canvas::transparent(2, 1);
  multiplyInk.setPixel(0, 0, {255, 255, 255, 1});
  multiplyInk.setPixel(1, 0, {100, 100, 100, 1});
  multiplyBackdrop.blendMultiply(multiplyInk);
  const Rgba whiteOnPaper = multiplyBackdrop.pixel(0, 0);
  const Rgba grayOnPaper = multiplyBackdrop.pixel(1, 0);
  const bool multiplyOk = whiteOnPaper.r == 245 && whiteOnPaper.g == 236 && whiteOnPaper.b == 220 &&
                          grayOnPaper.r == 96 && grayOnPaper.g == 93 && grayOnPaper.b == 86;
  std::cout << (multiplyOk ? "multiply match\n" : "multiply mismatch\n");

  Canvas scene(640, 360, {245, 236, 220, 1});
  drawStill(scene);
  scene.writeBmp("scene-check.bmp");
  std::cout << "ok scene-check.bmp\n";

  Generator generator("1");
  generator.start(3584);
  ChunkPlan referencePlan;
  const std::vector<Piece> reference = collectPieces("1", 3584, referencePlan);

  std::vector<Piece> received;
  GenEvent event;
  while (generator.next(event)) {
    if (event.kind == GenEvent::Kind::Timing) {
      std::ostringstream line;
      line << std::fixed << std::setprecision(1);
      line << "frame " << event.tag << " " << event.ms << " ms | xmax " << static_cast<long long>(event.xmax);
      std::cout << line.str() << "\n";
      continue;
    }
    received.push_back(std::move(event.piece));
  }

  const bool planOk = generator.plan().matchesSeed1();
  const bool threadOk = samePieces(received, reference);
  std::cout << "plan " << generator.plan().items().size() << " pieces, xmax " << generator.plan().xmax() << "\n";
  std::cout << "received " << received.size() << " drawn pieces\n";
  std::cout << (planOk ? "plan match\n" : "plan mismatch\n");
  std::cout << (threadOk ? "thread match\n" : "thread mismatch\n");

  Canvas strip(2000, 950, {245, 236, 220, 1});
  paintPieces(strip, received, -200, -150, seed);
  strip.writeBmp("chunk-check.bmp");
  std::cout << "ok chunk-check.bmp\n";

  const bool scrollOk = playScroll(received, seed, &generator);
  return strokeOk && ridgeOk && textureOk && footOk && distantOk && flatOk && hutOk && boatOk && paperOk &&
                 multiplyOk && planOk && threadOk && scrollOk
             ? 0
             : 1;
}
