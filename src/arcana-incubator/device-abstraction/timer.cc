#include "timer.hh"

#include <clean-core/macros.hh>

#ifdef CC_OS_WINDOWS

#include <clean-core/native/win32_sanitized.hh>

#include <ctime>

inc::da::Timer::Timer()
{
    ::LARGE_INTEGER frequency, timestamp;
    ::QueryPerformanceFrequency(&frequency);
    ::QueryPerformanceCounter(&timestamp);

    mResolution = frequency.QuadPart;
    mStartTime = timestamp.QuadPart;
}

void inc::da::Timer::restart()
{
    ::LARGE_INTEGER timestamp;
    ::QueryPerformanceCounter(&timestamp);
    mStartTime = timestamp.QuadPart;
}

double inc::da::Timer::elapsedSecondsD() const
{
    ::LARGE_INTEGER currentTime = {};
    ::QueryPerformanceCounter(&currentTime);
    return static_cast<double>(currentTime.QuadPart - mStartTime) / static_cast<double>(mResolution);
}

#elif defined(CC_OS_LINUX)

#include <time.h>

inc::da::Timer::Timer()
{
    ::timespec spec;
    ::clock_getres(CLOCK_REALTIME, &spec);
    mResolution = spec.tv_nsec;
    ::clock_gettime(CLOCK_REALTIME, &spec);
    mStartTime = spec.tv_nsec;
}

void inc::da::Timer::restart()
{
    ::timespec spec;
    ::clock_gettime(CLOCK_REALTIME, &spec);
    mStartTime = spec.tv_nsec;
}

double inc::da::Timer::elapsedSecondsD() const
{
    ::timespec spec;
    ::clock_gettime(CLOCK_REALTIME, &spec);
    return static_cast<double>(spec.tv_nsec - mStartTime) / static_cast<double>(mResolution);
}


#endif
