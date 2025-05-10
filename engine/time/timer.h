#pragma once

#include <chrono>

struct TTimer {

  using TTimeStamp = std::chrono::steady_clock::time_point;

  TTimeStamp start_ticks;

  static TTimeStamp timeStamp() {
    return std::chrono::high_resolution_clock::now();
  }
  TTimer() : start_ticks(timeStamp()) {
  }
  double reset() {
    auto delta = elapsed();
    start_ticks = timeStamp();
    return delta;
  }
  static double asSeconds(const std::chrono::steady_clock::duration dur) {
    return (double)std::chrono::duration_cast<std::chrono::nanoseconds>(dur).count() * ( 1.0 / 1000000000.0f );
  }
  double elapsed() {
    auto now = timeStamp();
    auto delta_ticks = now - start_ticks;
    start_ticks = now;
    return asSeconds(delta_ticks);
  }
  double elapsedSinceStart() const {
    return (double) std::chrono::duration_cast<std::chrono::microseconds>(timeStamp() - start_ticks).count() * ( 1.0f / 1000000 );
  }
};
