#pragma once

#include "planner.hpp"
#include "scenery.hpp"

#include <condition_variable>
#include <deque>
#include <exception>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

struct GenEvent {
  enum class Kind { Timing, Piece };
  Kind kind = Kind::Timing;
  const char* tag = "";
  double ms = 0;
  double xmax = 0;
  Piece piece;
};

class Generator {
 public:
  explicit Generator(std::string seed);
  ~Generator();

  Generator(const Generator&) = delete;
  Generator& operator=(const Generator&) = delete;

  void start(double targetMax);
  void extend(double targetMax);
  bool next(GenEvent& event);
  bool poll(GenEvent& event);
  bool exhausted();
  const ChunkPlan& plan() const { return plan_; }

 private:
  void run(double targetMax);
  void push(GenEvent event);

  std::string seed_;
  Prng prng_;
  Noise noise_;
  ChunkPlan plan_;
  bool caughtUp_ = false;
  std::mutex mutex_;
  std::condition_variable ready_;
  std::deque<GenEvent> queue_;
  bool finished_ = false;
  std::exception_ptr error_;
  std::thread thread_;
};

std::vector<Piece> collectPieces(const std::string& seed, double targetMax, ChunkPlan& plan);
