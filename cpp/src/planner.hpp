#pragma once

#include "noise.hpp"
#include "prng.hpp"

#include <map>
#include <vector>

enum class PlanTag { Mount, DistMount, FlatMount, Boat };

struct PlanItem {
  PlanTag tag = PlanTag::Mount;
  double x = 0;
  double y = 0;
  double h = 0;
};

class ChunkPlan {
 public:
  static constexpr double kChunkWidth = 512;

  void cover(double targetMax, Prng& prng, Noise& noise);
  bool extendOne(double targetMax, Prng& prng, Noise& noise);

  double xmax() const { return xmax_; }
  const std::vector<PlanItem>& items() const { return items_; }

  bool matchesSeed1() const;

 private:
  void planChunk(double xmin, double xmax, Prng& prng, Noise& noise);
  void touch(int index);
  void bump(int index);

  double xmax_ = 0;
  std::map<int, double> mask_;
  std::vector<PlanItem> items_;
};
