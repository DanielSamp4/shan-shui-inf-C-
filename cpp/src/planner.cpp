#include "planner.hpp"

#include <cmath>
#include <iostream>
#include <iterator>
#include <limits>

namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr double kStep = 5;
constexpr double kMountainWidth = 200;

bool jsFalsy(double value) {
  return value == 0.0 || std::isnan(value);
}

struct Expected {
  PlanTag tag;
  double x;
  double y;
  double h;
};

constexpr Expected kSeed1[] = {
    {PlanTag::DistMount, 0.0000000000000000, 259.09540653536129, 0.18617680548194304},
    {PlanTag::FlatMount, -345.92702853954358, 700.00000000000000, 0.0000000000000000},
    {PlanTag::FlatMount, -262.09293209480745, 650.00000000000000, 0.0000000000000000},
    {PlanTag::FlatMount, 128.41464954608369, 700.00000000000000, 0.0000000000000000},
    {PlanTag::FlatMount, 893.11353297225628, 650.00000000000000, 0.0000000000000000},
    {PlanTag::Mount, 361.35722399750136, 300.00000000000000, 0.37847643627625338},
    {PlanTag::Mount, 454.62173548264235, 330.00000000000000, 0.37847643627625338},
    {PlanTag::Mount, 60.825777325817569, 360.00000000000000, 0.37847643627625338},
    {PlanTag::Mount, 513.89855913677945, 390.00000000000000, 0.37847643627625338},
    {PlanTag::Mount, 405.81062082071082, 420.00000000000000, 0.37847643627625338},
    {PlanTag::Mount, 867.63971210030638, 450.00000000000000, 0.37847643627625338},
    {PlanTag::Mount, 979.55709545491936, 480.00000000000000, 0.37847643627625338},
    {PlanTag::DistMount, 1002.0000000000000, 230.12022774531141, 0.092602136216718867},
    {PlanTag::Boat, 1049.0000000000000, 328.78450662394562, 0},
    {PlanTag::Boat, 1474.0000000000000, 649.67188130568672, 0},
    {PlanTag::DistMount, 2001.0000000000000, 250.26540842418535, 0.0000000000000000},
    {PlanTag::Boat, 1591.0000000000000, 646.77838702490703, 0},
    {PlanTag::Mount, 2152.7284744374956, 300.00000000000000, 0.33317105236662958},
    {PlanTag::Mount, 2500.8361610373117, 330.00000000000000, 0.33317105236662958},
    {PlanTag::Mount, 2364.9186373749353, 390.00000000000000, 0.33317105236662958},
    {PlanTag::Mount, 2376.3845665541508, 420.00000000000000, 0.33317105236662958},
    {PlanTag::Mount, 2017.8076360968053, 450.00000000000000, 0.33317105236662958},
    {PlanTag::Mount, 1910.1918727492171, 480.00000000000000, 0.33317105236662958},
    {PlanTag::Mount, 2237.9123007324843, 510.00000000000000, 0.33317105236662958},
    {PlanTag::Mount, 1874.1568549104036, 540.00000000000000, 0.33317105236662958},
    {PlanTag::Mount, 2460.1445990068905, 300.00000000000000, 0.36749827810442093},
    {PlanTag::Mount, 2745.0629941691909, 330.00000000000000, 0.36749827810442093},
    {PlanTag::Mount, 2576.6353013079911, 360.00000000000000, 0.36749827810442093},
    {PlanTag::Mount, 2495.7540935819720, 390.00000000000000, 0.36749827810442093},
    {PlanTag::Mount, 2533.6775658173783, 420.00000000000000, 0.36749827810442093},
    {PlanTag::DistMount, 3000.0000000000000, 266.09809527688350, 0.15822014306050169},
    {PlanTag::Mount, 3411.6554739013063, 300.00000000000000, 0.43799554214891856},
    {PlanTag::Mount, 3161.0729624563292, 330.00000000000000, 0.43799554214891856},
    {PlanTag::Mount, 3439.4205056756018, 360.00000000000000, 0.43799554214891856},
    {PlanTag::Mount, 3465.9971848799232, 390.00000000000000, 0.43799554214891856},
    {PlanTag::Mount, 2839.3281276504695, 420.00000000000000, 0.43799554214891856},
    {PlanTag::Mount, 2924.2041601259730, 300.00000000000000, 0.47632391461731372},
    {PlanTag::Mount, 3004.5424924449203, 360.00000000000000, 0.47632391461731372},
    {PlanTag::Mount, 3079.6821972328771, 390.00000000000000, 0.47632391461731372},
    {PlanTag::Mount, 2725.9040933092269, 420.00000000000000, 0.47632391461731372},
    {PlanTag::Mount, 3052.5737582888751, 450.00000000000000, 0.47632391461731372},
    {PlanTag::Mount, 3360.6940581437907, 300.00000000000000, 0.34582507818107477},
    {PlanTag::Mount, 3751.0895172768664, 330.00000000000000, 0.34582507818107477},
    {PlanTag::Mount, 3875.2768233881638, 360.00000000000000, 0.34582507818107477},
    {PlanTag::Mount, 3851.9499098207762, 390.00000000000000, 0.34582507818107477},
    {PlanTag::Mount, 3513.1142563339081, 450.00000000000000, 0.34582507818107477},
    {PlanTag::Mount, 3197.6182836315775, 480.00000000000000, 0.34582507818107477},
    {PlanTag::Mount, 3542.5757485320937, 510.00000000000000, 0.34582507818107477},
    {PlanTag::FlatMount, 4246.2674364450104, 700.00000000000000, 0.0000000000000000},
    {PlanTag::FlatMount, 4053.1048159526358, 700.00000000000000, 0.0000000000000000},
    {PlanTag::FlatMount, 3760.4402514962676, 650.00000000000000, 0.0000000000000000},
    {PlanTag::FlatMount, 3818.8751772159840, 600.00000000000000, 0.0000000000000000},
    {PlanTag::FlatMount, 3112.4969252591864, 550.00000000000000, 0.0000000000000000},
};

const char* tagName(PlanTag tag) {
  switch (tag) {
    case PlanTag::Mount:
      return "mount";
    case PlanTag::DistMount:
      return "distmount";
    case PlanTag::FlatMount:
      return "flatmount";
    case PlanTag::Boat:
      return "boat";
  }
  return "?";
}

}  // namespace

void ChunkPlan::touch(int index) {
  const auto found = mask_.find(index);
  if (found == mask_.end() || jsFalsy(found->second)) {
    mask_[index] = 0;
  }
}

void ChunkPlan::bump(int index) {
  const auto found = mask_.find(index);
  if (found == mask_.end()) {
    mask_[index] = std::numeric_limits<double>::quiet_NaN();
    return;
  }
  found->second += 1;
}

void ChunkPlan::planChunk(double xmin, double xmax, Prng& prng, Noise& noise) {
  std::vector<PlanItem> placed;
  auto place = [&](PlanItem item, double mind) {
    for (const PlanItem& previous : placed) {
      if (std::abs(previous.x - item.x) < mind) {
        return false;
      }
    }
    placed.push_back(item);
    return true;
  };

  auto field = [&](double x, double y) {
    (void)y;
    return std::max(noise.sample(x * 0.03) - 0.55, 0.0) * 2.0;
  };
  auto horizon = [&](double x) { return noise.sample(x * 0.01, kPi); };

  auto localMax = [&](double x, double y) {
    const double height = field(x, y);
    if (height <= 0.3) {
      return false;
    }
    for (double ix = x - 2; ix < x + 2; ix += 1) {
      for (double iy = y - 2; iy < y + 2; iy += 1) {
        if (field(ix, iy) > height) {
          return false;
        }
      }
    }
    return true;
  };

  for (double i = xmin; i < xmax; i += kStep) {
    touch(static_cast<int>(std::floor(i / kStep)));
  }

  for (double i = xmin; i < xmax; i += kStep) {
    double j = 0;
    const double limit = horizon(i) * 480.0;
    for (; j < limit; j += 30) {
      if (!localMax(i, j)) {
        continue;
      }
      const double x = i + 2.0 * (prng.next() - 0.5) * 500.0;
      const double y = j + 300.0;
      PlanItem item{PlanTag::Mount, x, y, field(i, j)};
      if (place(item, 10)) {
        const int begin = static_cast<int>(std::floor((x - kMountainWidth) / kStep));
        const double end = (x + kMountainWidth) / kStep;
        for (int k = begin; k < end; ++k) {
          bump(k);
        }
      }
    }
    if (std::fmod(std::abs(i), 1000.0) < 4.0) {
      PlanItem item{PlanTag::DistMount, i, 280.0 - prng.next() * 50.0, field(i, j)};
      place(item, 10);
    }
  }

  for (double i = xmin; i < xmax; i += kStep) {
    const auto cell = mask_.find(static_cast<int>(std::floor(i / kStep)));
    if (cell == mask_.end() || cell->second != 0.0) {
      continue;
    }
    if (prng.next() >= 0.01) {
      continue;
    }
    double count = 0;
    while (count < 4.0 * prng.next()) {
      PlanItem item{PlanTag::FlatMount, i + 2.0 * (prng.next() - 0.5) * 700.0, 700.0 - count * 50.0,
                    field(i, count)};
      place(item, 10);
      count += 1;
    }
  }

  for (double i = xmin; i < xmax; i += kStep) {
    if (prng.next() >= 0.2) {
      continue;
    }
    PlanItem item{PlanTag::Boat, i, 300.0 + prng.next() * 390.0, 0};
    place(item, 400);
  }

  items_.insert(items_.end(), placed.begin(), placed.end());
}

bool ChunkPlan::extendOne(double targetMax, Prng& prng, Noise& noise) {
  if (!(targetMax > xmax_ - kChunkWidth)) {
    return false;
  }
  const double xmin = xmax_;
  xmax_ += kChunkWidth;
  planChunk(xmin, xmax_, prng, noise);
  return true;
}

void ChunkPlan::cover(double targetMax, Prng& prng, Noise& noise) {
  while (extendOne(targetMax, prng, noise)) {
  }
}

bool ChunkPlan::matchesSeed1() const {
  if (xmax_ != 4096.0 || items_.size() != std::size(kSeed1)) {
    std::cout << "plan size " << items_.size() << " xmax " << xmax_ << "\n";
    return false;
  }
  for (std::size_t i = 0; i < items_.size(); ++i) {
    const PlanItem& got = items_[i];
    const Expected& want = kSeed1[i];
    if (got.tag != want.tag || std::abs(got.x - want.x) > 1e-6 || std::abs(got.y - want.y) > 1e-6 ||
        std::abs(got.h - want.h) > 1e-6) {
      std::cout << "plan " << i << " got " << tagName(got.tag) << " " << got.x << " " << got.y << " " << got.h
                << "\n";
      return false;
    }
  }
  return true;
}
