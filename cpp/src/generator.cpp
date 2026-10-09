#include "generator.hpp"

#include "noise.hpp"
#include "prng.hpp"

#include <chrono>
#include <functional>
#include <utility>

namespace {

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

double millisSince(std::chrono::steady_clock::time_point start) {
  return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
}

void emitPiece(const PlanItem& item, std::size_t indexInChunk, double xmax, Prng& prng, Noise& noise,
               const std::function<void(GenEvent)>& emit) {
  const auto started = std::chrono::steady_clock::now();
  auto made = realizeItem(item, indexInChunk, prng, noise);
  GenEvent timing;
  timing.kind = GenEvent::Kind::Timing;
  timing.tag = tagName(item.tag);
  timing.ms = millisSince(started);
  timing.xmax = xmax;
  emit(timing);
  for (Piece& piece : made) {
    GenEvent event;
    event.kind = GenEvent::Kind::Piece;
    event.tag = piece.tag;
    event.xmax = xmax;
    event.piece = std::move(piece);
    emit(std::move(event));
  }
}

void generate(Prng& prng, Noise& noise, ChunkPlan& plan, bool& caughtUp, double targetMax,
              const std::function<void(GenEvent)>& emit) {
  if (!caughtUp) {
    std::vector<std::size_t> chunkAt;
    while (true) {
      const auto before = plan.items().size();
      const auto started = std::chrono::steady_clock::now();
      if (!plan.extendOne(targetMax, prng, noise)) {
        break;
      }
      chunkAt.push_back(before);
      GenEvent timing;
      timing.kind = GenEvent::Kind::Timing;
      timing.tag = "plano";
      timing.ms = millisSince(started);
      timing.xmax = plan.xmax();
      emit(timing);
    }
    chunkAt.push_back(plan.items().size());
    const auto& items = plan.items();
    for (std::size_t chunk = 0; chunk + 1 < chunkAt.size(); ++chunk) {
      for (std::size_t i = chunkAt[chunk]; i < chunkAt[chunk + 1]; ++i) {
        emitPiece(items[i], i - chunkAt[chunk], plan.xmax(), prng, noise, emit);
      }
    }
    caughtUp = true;
    return;
  }

  while (true) {
    const auto before = plan.items().size();
    const auto started = std::chrono::steady_clock::now();
    if (!plan.extendOne(targetMax, prng, noise)) {
      break;
    }
    GenEvent timing;
    timing.kind = GenEvent::Kind::Timing;
    timing.tag = "plano";
    timing.ms = millisSince(started);
    timing.xmax = plan.xmax();
    emit(timing);
    const auto& items = plan.items();
    for (std::size_t i = before; i < items.size(); ++i) {
      emitPiece(items[i], i - before, plan.xmax(), prng, noise, emit);
    }
  }
}

void produce(const std::string& seed, double targetMax, ChunkPlan& plan, const std::function<void(GenEvent)>& emit) {
  Prng prng;
  prng.seed(seed);
  Noise noise(prng);
  bool caughtUp = false;
  generate(prng, noise, plan, caughtUp, targetMax, emit);
}

}  // namespace

Generator::Generator(std::string seed) : seed_(std::move(seed)), noise_(prng_) {
  prng_.seed(seed_);
}

Generator::~Generator() {
  if (thread_.joinable()) {
    thread_.join();
  }
}

void Generator::push(GenEvent event) {
  {
    std::lock_guard<std::mutex> lock(mutex_);
    queue_.push_back(std::move(event));
  }
  ready_.notify_one();
}

void Generator::run(double targetMax) {
  try {
    generate(prng_, noise_, plan_, caughtUp_, targetMax, [this](GenEvent event) { push(std::move(event)); });
  } catch (...) {
    std::lock_guard<std::mutex> lock(mutex_);
    error_ = std::current_exception();
  }
  {
    std::lock_guard<std::mutex> lock(mutex_);
    finished_ = true;
  }
  ready_.notify_one();
}

void Generator::start(double targetMax) {
  extend(targetMax);
}

void Generator::extend(double targetMax) {
  {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!finished_ && thread_.joinable()) {
      return;
    }
    finished_ = false;
    error_ = nullptr;
  }
  if (thread_.joinable()) {
    thread_.join();
  }
  thread_ = std::thread([this, targetMax] { run(targetMax); });
}

bool Generator::poll(GenEvent& event) {
  std::lock_guard<std::mutex> lock(mutex_);
  if (queue_.empty()) {
    return false;
  }
  event = std::move(queue_.front());
  queue_.pop_front();
  return true;
}

bool Generator::exhausted() {
  std::lock_guard<std::mutex> lock(mutex_);
  return finished_ && queue_.empty();
}

bool Generator::next(GenEvent& event) {
  std::unique_lock<std::mutex> lock(mutex_);
  ready_.wait(lock, [this] { return finished_ || !queue_.empty(); });
  if (queue_.empty()) {
    const auto error = error_;
    lock.unlock();
    if (thread_.joinable()) {
      thread_.join();
    }
    if (error) {
      std::rethrow_exception(error);
    }
    return false;
  }
  event = std::move(queue_.front());
  queue_.pop_front();
  return true;
}

std::vector<Piece> collectPieces(const std::string& seed, double targetMax, ChunkPlan& plan) {
  std::vector<Piece> pieces;
  produce(seed, targetMax, plan, [&](GenEvent event) {
    if (event.kind == GenEvent::Kind::Piece) {
      pieces.push_back(std::move(event.piece));
    }
  });
  return pieces;
}
