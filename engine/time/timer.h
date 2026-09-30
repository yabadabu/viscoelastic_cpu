#pragma once

#include <chrono>

struct TTimer {
    using Clock = std::chrono::steady_clock;
    using TTimeStamp = Clock::time_point;

    TTimeStamp start_ticks;

    static TTimeStamp timeStamp() {
        return Clock::now();
    }

    TTimer()
        : start_ticks(timeStamp())
    {
    }

    double reset() {
        auto delta = elapsed();
        start_ticks = timeStamp();
        return delta;
    }

    static double asSeconds(Clock::duration dur) {
        return std::chrono::duration<double>(dur).count();
    }

    double elapsed() {
        auto now = timeStamp();
        auto delta_ticks = now - start_ticks;
        start_ticks = now;
        return asSeconds(delta_ticks);
    }

    double elapsedSinceStart() const {
        return std::chrono::duration<double>(
            timeStamp() - start_ticks
        ).count();
    }
};
