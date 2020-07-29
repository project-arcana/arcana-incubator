#include "device_abstraction.hh"

#include <cstdio>

#include <clean-core/assert.hh>

#include <rich-log/log.hh>

#include <SDL2/SDL.h>

namespace
{
void verify_failure_handler(const char* expression, const char* filename, int line)
{
    fprintf(stderr, "[da] verify on `%s' failed.\n", expression);
    fprintf(stderr, "  error: %s\n", SDL_GetError());
    fprintf(stderr, "  file %s:%d\n", filename, line);
    fflush(stderr);

#ifndef NDEBUG
    std::abort();
#endif
}

#define DA_SDL_VERIFY(_expr_)                                    \
    do                                                           \
    {                                                            \
        int const op_res = (_expr_);                             \
        if (CC_UNLIKELY(op_res < 0))                             \
        {                                                        \
            verify_failure_handler(#_expr_, __FILE__, __LINE__); \
        }                                                        \
    } while (0)

}

void inc::da::initialize()
{
    SDL_SetMainReady(); // we use SDL_MAIN_HANDLED (in CMakeLists.txt), see https://wiki.libsdl.org/SDL_SetMainReady
    DA_SDL_VERIFY(SDL_Init(SDL_INIT_EVERYTHING));
    DA_SDL_VERIFY(SDL_JoystickEventState(SDL_ENABLE));
}

void inc::da::shutdown() { SDL_Quit(); }

void inc::da::SDLWindow::initialize(const char* title, tg::isize2 size, bool enable_vulkan)
{
    CC_ASSERT(mWindow == nullptr && "double init");

    uint32_t flags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI;
    if (enable_vulkan)
        flags |= SDL_WINDOW_VULKAN;

    mWindow = SDL_CreateWindow(title, SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, size.width, size.height, flags);

    if (mWindow == nullptr)
    {
        SDL_Log("Failed to create window: %s\n", SDL_GetError());
    }

    onResizeEvent(size.width, size.height, false);
}

void inc::da::SDLWindow::destroy()
{
    if (mWindow)
    {
        SDL_DestroyWindow(mWindow);
        mWindow = nullptr;
    }
}

void inc::da::SDLWindow::pollEvents()
{
    SDL_Event e;

    while (pollSingleEvent(e))
    {
        // spin
    }
}

bool inc::da::SDLWindow::pollSingleEvent(SDL_Event& out_event)
{
#ifdef CC_ENABLE_ASSERTIONS
    mSafetyState.polled_since_last_close_test = true;
#endif
    if (SDL_PollEvent(&out_event) == 0)
        return false;

    if (mEventCallback && mEventCallback(&out_event))
    {
        // do not internally handle
    }
    else
    {
        if (out_event.type == SDL_QUIT)
        {
            mIsRequestingClose = true;
        }
        else if (out_event.type == SDL_WINDOWEVENT)
        {
            if (out_event.window.event == SDL_WINDOWEVENT_CLOSE)
            {
                mIsRequestingClose = true;
            }
            else if (out_event.window.event == SDL_WINDOWEVENT_RESIZED ||      //
                     out_event.window.event == SDL_WINDOWEVENT_SIZE_CHANGED || //
                     out_event.window.event == SDL_WINDOWEVENT_MINIMIZED ||    //
                     out_event.window.event == SDL_WINDOWEVENT_MAXIMIZED)
            {
                int new_w, new_h;
                SDL_GetWindowSize(mWindow, &new_w, &new_h);
                bool const is_minimized = SDL_GetWindowFlags(mWindow) & SDL_WINDOW_MINIMIZED;
                onResizeEvent(new_w, new_h, is_minimized);
            }
        }
    }

    return true;
}

tg::ivec2 inc::da::SDLWindow::getPosition() const
{
    tg::ivec2 res;
    SDL_GetWindowPosition(mWindow, &res.x, &res.y);
    return res;
}

void inc::da::SDLWindow::setFullscreen()
{
    restoreFromBorderless();
    DA_SDL_VERIFY(SDL_SetWindowFullscreen(mWindow, SDL_WINDOW_FULLSCREEN));
}

void inc::da::SDLWindow::setBorderlessFullscreen(int target_display_index)
{
    if (mBorderlessState.is_in_borderless)
        return;

    // NOTE: the official way to enter borderless is using SDL_WINDOW_FULLSCREEN_DESKTOP,
    // which behaves very poorly - blacks out on clicks on a different screen,
    // becomes invisible on alt-tab / windows menu, never visible in the background

    DA_SDL_VERIFY(SDL_SetWindowFullscreen(mWindow, 0));

    mBorderlessState.prev_size = getSize();
    mBorderlessState.prev_pos = getPosition();
    mBorderlessState.is_in_borderless = true;

    int const display_index = target_display_index >= 0 ? target_display_index : SDL_GetWindowDisplayIndex(mWindow);

    SDL_DisplayMode mode;
    DA_SDL_VERIFY(SDL_GetDesktopDisplayMode(display_index, &mode));
    SDL_Rect bounds;
    DA_SDL_VERIFY(SDL_GetDisplayBounds(display_index, &bounds));
    SDL_SetWindowSize(mWindow, mode.w, mode.h);
    SDL_SetWindowPosition(mWindow, bounds.x, bounds.y);

    SDL_SetWindowResizable(mWindow, SDL_FALSE);
    SDL_SetWindowBordered(mWindow, SDL_FALSE);
}

void inc::da::SDLWindow::setWindowed()
{
    restoreFromBorderless();
    DA_SDL_VERIFY(SDL_SetWindowFullscreen(mWindow, 0));
}

void inc::da::SDLWindow::setDisplayMode(int width, int height, int refresh_rate)
{
    SDL_DisplayMode mode = {SDL_PIXELFORMAT_BGRA8888, width, height, refresh_rate, nullptr};
    DA_SDL_VERIFY(SDL_SetWindowDisplayMode(mWindow, &mode));
}

void inc::da::SDLWindow::setDesktopDisplayMode()
{
    SDL_DisplayMode mode;
    DA_SDL_VERIFY(SDL_GetDesktopDisplayMode(SDL_GetWindowDisplayIndex(mWindow), &mode));
    DA_SDL_VERIFY(SDL_SetWindowDisplayMode(mWindow, &mode));
}

bool inc::da::SDLWindow::captureMouse()
{
    if (mMouseCaptureState.captured)
        return false;

    mMouseCaptureState.captured = true;
    SDL_GetMouseState(&mMouseCaptureState.x_precap, &mMouseCaptureState.y_precap);
    SDL_SetRelativeMouseMode(SDL_TRUE);
    return true;
}

bool inc::da::SDLWindow::uncaptureMouse()
{
    if (!mMouseCaptureState.captured)
        return false;

    mMouseCaptureState.captured = false;
    SDL_SetRelativeMouseMode(SDL_FALSE);
    SDL_WarpMouseInWindow(mWindow, mMouseCaptureState.x_precap, mMouseCaptureState.y_precap);
    return true;
}

void inc::da::SDLWindow::onResizeEvent(int w, int h, bool minimized)
{
    if (mWidth == w && mHeight == h && mIsMinimized == minimized)
        return;

    mWidth = w;
    mHeight = h;
    mIsMinimized = minimized;
    mPendingResize = true;
}

void inc::da::SDLWindow::restoreFromBorderless()
{
    if (!mBorderlessState.is_in_borderless)
        return;

    mBorderlessState.is_in_borderless = false;

    SDL_SetWindowResizable(mWindow, SDL_TRUE);
    SDL_SetWindowBordered(mWindow, SDL_TRUE);
    SDL_SetWindowPosition(mWindow, mBorderlessState.prev_pos.x, mBorderlessState.prev_pos.y);
    SDL_SetWindowSize(mWindow, mBorderlessState.prev_size.width, mBorderlessState.prev_size.height);
}
