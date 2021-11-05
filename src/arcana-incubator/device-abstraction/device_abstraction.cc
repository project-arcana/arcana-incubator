#include "device_abstraction.hh"

#include <cstdio>

#include <clean-core/assert.hh>
#include <clean-core/native/win32_fwd.hh>

#include <rich-log/log.hh>

#include <SDL2/SDL.h>

namespace
{
void verify_failure_handler(const char* expression, const char* filename, int line)
{
    fprintf(stderr, "[da] verify on `%s' failed.\n", expression);
    fprintf(stderr, "  error: %s\n", SDL_GetError());
    fprintf(stderr, "  file %s:%d\n", filename, line);
    fprintf(stderr, "[da] breaking - resume possible\n");
    fflush(stderr);
}

#define DA_SDL_VERIFY(_expr_)                                    \
    do                                                           \
    {                                                            \
        int const op_res = (_expr_);                             \
        if (CC_UNLIKELY(op_res < 0))                             \
        {                                                        \
            verify_failure_handler(#_expr_, __FILE__, __LINE__); \
            CC_DEBUG_BREAK();                                    \
        }                                                        \
    } while (0)

} // namespace

void inc::da::initialize(bool enable_controllers)
{
    // NOTE: On Windows, some faulty drivers can cause SDL_Init with SDL_INIT_JOYSTICK to take extremely long (20s+)
    // USB DACs seem to be the most frequent reason
    // see: https://stackoverflow.com/questions/10967795/directinput8-enumdevices-sometimes-painfully-slow

    unsigned init_flags = SDL_INIT_TIMER | SDL_INIT_VIDEO | SDL_INIT_EVENTS;

    if (enable_controllers)
    {
        // enable controllers and everything else
        init_flags |= SDL_INIT_JOYSTICK | SDL_INIT_GAMECONTROLLER | SDL_INIT_HAPTIC | SDL_INIT_SENSOR;
    }

    // we use SDL_MAIN_HANDLED (in CMakeLists.txt),
    // see https://wiki.libsdl.org/SDL_SetMainReady
    SDL_SetMainReady();

    int const initResult = SDL_Init(init_flags);
    if (initResult < 0)
    {
        LOG_INFO(R"(SDL_Init returned an error: "{}")", SDL_GetError());
        LOG_INFO("Execution can likely continue");
    }

    DA_SDL_VERIFY(SDL_JoystickEventState(SDL_ENABLE));

    SDL_version version;
    SDL_GetVersion(&version);
    LOG_INFO("Initialized SDL {}.{}.{} on {}", version.major, version.minor, version.patch, SDL_GetPlatform());
}

bool inc::da::setProcessHighDPIAware()
{
#ifdef CC_OS_WINDOWS
    typedef enum PROCESS_DPI_AWARENESS
    {
        PROCESS_DPI_UNAWARE = 0,
        PROCESS_SYSTEM_DPI_AWARE = 1,
        PROCESS_PER_MONITOR_DPI_AWARE = 2
    } PROCESS_DPI_AWARENESS;

    BOOL(WINAPI * fptr_SetProcessDPIAware)(void);                                      // Vista and later
    HRESULT(WINAPI * fptr_SetProcessDpiAwareness)(PROCESS_DPI_AWARENESS dpiAwareness); // Windows 8.1 and later

    void* dllUser32 = SDL_LoadObject("USER32.DLL");
    if (dllUser32)
    {
        fptr_SetProcessDPIAware = (BOOL(WINAPI*)(void))SDL_LoadFunction(dllUser32, "SetProcessDPIAware");
    }

    void* dllShcore = SDL_LoadObject("SHCORE.DLL");
    if (dllShcore)
    {
        fptr_SetProcessDpiAwareness = (HRESULT(WINAPI*)(PROCESS_DPI_AWARENESS))SDL_LoadFunction(dllShcore, "SetProcessDpiAwareness");
    }

    bool success = false;

    if (fptr_SetProcessDpiAwareness)
    {
        HRESULT res = fptr_SetProcessDpiAwareness(PROCESS_PER_MONITOR_DPI_AWARE);
        success = (res == ((HRESULT)0L));
    }
    else if (fptr_SetProcessDPIAware)
    {
        // Vista to Windows 8 version, has constant scale factor for all monitors
        BOOL res = fptr_SetProcessDPIAware();
        success = !!res;
    }

    if (dllUser32)
        SDL_UnloadObject(dllUser32);

    if (dllShcore)
        SDL_UnloadObject(dllShcore);

    return success;
#else
    return true;
#endif
}

void inc::da::shutdown() { SDL_Quit(); }

void inc::da::SDLWindow::initialize(const char* title, tg::isize2 size, bool enable_vulkan)
{
    CC_ASSERT(mWindow == nullptr && "double init");

    uint32_t flags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI;
    if (enable_vulkan)
        flags |= SDL_WINDOW_VULKAN;

    mWindow = SDL_CreateWindow(title, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, size.width, size.height, flags);

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
    mSafetyState.num_close_tests_since_poll = 0;
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
                queryAndTriggerResize();
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
    DA_SDL_VERIFY(SDL_GetCurrentDisplayMode(display_index, &mode));
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

bool inc::da::SDLWindow::setDisplayMode(tg::isize2 resolution, int refresh_rate)
{
    SDL_DisplayMode mode = {SDL_PIXELFORMAT_BGRA8888, resolution.width, resolution.height, refresh_rate, nullptr};

    auto const res = SDL_SetWindowDisplayMode(mWindow, &mode);
    return res == 0;
}

void inc::da::SDLWindow::setDesktopDisplayMode()
{
    SDL_DisplayMode mode;
    DA_SDL_VERIFY(SDL_GetDesktopDisplayMode(SDL_GetWindowDisplayIndex(mWindow), &mode));
    // LOG_TRACE("Desktop Display Mode: {}x{}@{}Hz", mode.w, mode.h, mode.refresh_rate);
    DA_SDL_VERIFY(SDL_SetWindowDisplayMode(mWindow, &mode));
}

int inc::da::SDLWindow::getNumDisplays() { return SDL_GetNumVideoDisplays(); }

int inc::da::SDLWindow::getNumDisplayModes(int monitor_index) { return SDL_GetNumDisplayModes(monitor_index); }

bool inc::da::SDLWindow::getDisplayMode(int monitor_index, int mode_index, tg::isize2& out_resolution, int& out_refreshrate)
{
    SDL_DisplayMode mode;
    auto const res = SDL_GetDisplayMode(monitor_index, mode_index, &mode);
    if (res != 0)
        return false;

    out_resolution = {mode.w, mode.h};
    out_refreshrate = mode.refresh_rate;

    return true;
}

bool inc::da::SDLWindow::getDesktopDisplayMode(int monitor_index, tg::isize2& out_resolution, int& out_refreshrate)
{
    SDL_DisplayMode mode;
    auto const res = SDL_GetDesktopDisplayMode(monitor_index, &mode);
    if (res != 0)
    {
        return false;
    }

    out_resolution = {mode.w, mode.h};
    out_refreshrate = mode.refresh_rate;
    return true;
}

bool inc::da::SDLWindow::getCurrentDisplayMode(int monitor_index, tg::isize2& out_resolution, int& out_refreshrate)
{
    SDL_DisplayMode mode;
    auto const res = SDL_GetCurrentDisplayMode(monitor_index, &mode);
    if (res != 0)
    {
        return false;
    }

    out_resolution = {mode.w, mode.h};
    out_refreshrate = mode.refresh_rate;
    return true;
}

bool inc::da::SDLWindow::getClosestDisplayMode(int monitor_index, tg::isize2 resolution, int refreshrate, tg::isize2& out_resolution, int& out_refreshrate)
{
    SDL_DisplayMode mode = {SDL_PIXELFORMAT_BGRA8888, resolution.width, resolution.height, refreshrate, nullptr};
    SDL_DisplayMode closest_match;
    auto const res = SDL_GetClosestDisplayMode(monitor_index, &mode, &closest_match);

    out_resolution = {closest_match.w, closest_match.h};
    out_refreshrate = closest_match.refresh_rate;

    return res != nullptr;
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

void inc::da::SDLWindow::queryAndTriggerResize()
{
    auto const window_flags = SDL_GetWindowFlags(mWindow);

    // bool const is_fullscreen = window_flags & SDL_WINDOW_FULLSCREEN;
    bool const is_minimized = window_flags & SDL_WINDOW_MINIMIZED;

    int new_w, new_h;
    SDL_GetWindowSize(mWindow, &new_w, &new_h);

    onResizeEvent(new_w, new_h, is_minimized);
}

void inc::da::SDLWindow::onResizeEvent(int w, int h, bool minimized)
{
    // LOG_TRACE("{} {}x{} ({})", __FUNCTION__, w, h, minimized);
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
