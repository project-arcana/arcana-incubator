#include "timer.hh"

#include <SDL2/SDL_timer.h>

inc::da::Timer::Timer()
{
    mResolution = SDL_GetPerformanceFrequency();
    mStartTime = SDL_GetPerformanceCounter();
}

void inc::da::Timer::restart()
{
    mStartTime = SDL_GetPerformanceCounter();
}

double inc::da::Timer::elapsedSecondsD() const
{
    auto const current_time = SDL_GetPerformanceCounter();
    return static_cast<double>(current_time - mStartTime) / static_cast<double>(mResolution);
}

