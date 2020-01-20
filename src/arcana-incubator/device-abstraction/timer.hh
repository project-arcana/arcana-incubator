#pragma once

#include <cstdint>

namespace inc::da
{
class Timer
{
public:
    Timer();

    /// Restart the timer
    void restart();

    /// Get the duration since last restart
    float elapsedSeconds() const { return static_cast<float>(elapsedSecondsD()); }
    double elapsedSecondsD() const;

    /// Get the duration since last restart in milliseconds
    float elapsedMilliseconds() const { return static_cast<float>(elapsedMillisecondsD()); }
    double elapsedMillisecondsD() const { return elapsedSecondsD() * 1000.0; }

private:
    uint64_t mResolution;
    uint64_t mStartTime;
};
}
