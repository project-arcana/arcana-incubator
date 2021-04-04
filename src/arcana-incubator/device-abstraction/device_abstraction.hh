#pragma once

#include <clean-core/assert.hh>

#include <typed-geometry/types/size.hh>

union SDL_Event;
struct SDL_Window;

namespace inc::da
{
void initialize(bool enable_controllers = false);

void shutdown();

class SDLWindow
{
public:
    void initialize(char const* title, tg::isize2 size = {1600, 900}, bool enable_vulkan = true);
    void destroy();

    SDLWindow() = default;
    SDLWindow(char const* title, tg::isize2 size = {1600, 900}, bool enable_vulkan = true) { initialize(title, size, enable_vulkan); }

    SDLWindow(SDLWindow const&) = delete;
    SDLWindow(SDLWindow&&) noexcept = delete;
    SDLWindow& operator=(SDLWindow const&) = delete;
    SDLWindow& operator=(SDLWindow&&) noexcept = delete;

    ~SDLWindow() { destroy(); }

    //
    // events

    /// poll events by the WM/OS
    void pollEvents();

    /// poll a single SDL event, use while(pollSingleEvent(e)) {..}
    bool pollSingleEvent(SDL_Event& out_event);

    /// whether a close event has been fired
    [[nodiscard]] bool isRequestingClose()
    {
#ifdef CC_ENABLE_ASSERTIONS
        CC_ASSERT(mSafetyState.polled_since_last_close_test && "forgot to poll window events in while loop body?");
        mSafetyState.polled_since_last_close_test = false;
#endif
        return mIsRequestingClose;
    }

    /// whether a resize occured since the last ::clearPendingResize()
    /// clears pending resizes
    [[nodiscard]] bool clearPendingResize()
    {
        auto const res = mPendingResize;
        mPendingResize = false;
        return res;
    }

    //
    // info and getters

    int getWidth() const { return mWidth; }
    int getHeight() const { return mHeight; }
    tg::isize2 getSize() const { return {mWidth, mHeight}; }
    tg::ivec2 getPosition() const;
    bool isMinimized() const { return mIsMinimized; }

    SDL_Window* getSdlWindow() const { return mWindow; }

    //
    // fullscreen mode

    /// set the window to display in proper fullscreen
    /// to change settings, use setDisplayMode/setDesktopDisplayMode BEFORE calling this function
    void setFullscreen();

    /// set the window to display in pseudo fullscreen without a display mode change
    /// target_monitor_index == -1: current monitor of the window
    void setBorderlessFullscreen(int target_monitor_index = -1);

    /// set the window to display in windowed mode
    void setWindowed();

    //
    // display mode

    /// set the display mode, only affects fullscreen
    /// returns true on success
    bool setDisplayMode(tg::isize2 resolution, int refresh_rate);

    /// set the display mode to the natively specified desktop display mode, only affects fullscreen
    void setDesktopDisplayMode();

    /// return the amount of physical monitors
    static int getNumMonitors();

    /// returns the amount of available display modes available on the given monitor
    static int getNumDisplayModes(int monitor_index);

    /// gives information about a specified display mode
    static bool getDisplayMode(int monitor_index, int mode_index, tg::isize2& out_resolution, int& out_refreshrate);

    static bool getDesktopDisplayMode(int monitor_index, tg::isize2& out_resolution, int& out_refreshrate);

    /// gives the best matching display mode
    static bool getClosestDisplayMode(int monitor_index, tg::isize2 resolution, int refreshrate, tg::isize2& out_resolution, int& out_refreshrate);

    //
    // mouse capture

    /// enables relative mouse mode, returns false if already in captured mode
    bool captureMouse();

    /// disabled relative mouse mode, returns false if not in captured mode
    bool uncaptureMouse();

    bool isMouseCaptured() const { return mMouseCaptureState.captured; }

public:
    using event_callback = bool (*)(SDL_Event const* e);

    void setEventCallback(event_callback callback) { mEventCallback = callback; }

private:
    void queryAndTriggerResize();

    void onResizeEvent(int w, int h, bool minimized);

    void restoreFromBorderless();

private:
    SDL_Window* mWindow = nullptr;
    event_callback mEventCallback = nullptr;

    int mWidth = 0;
    int mHeight = 0;
    bool mIsMinimized = false;
    bool mPendingResize = false;
    bool mIsRequestingClose = false;

    // fullscreen/borderless aux state
    struct
    {
        bool is_in_borderless = false;
        tg::isize2 prev_size;
        tg::ivec2 prev_pos;
    } mBorderlessState;

    struct
    {
        int x_precap = 0;
        int y_precap = 0;
        bool captured = false;
    } mMouseCaptureState;

#ifdef CC_ENABLE_ASSERTIONS
    struct
    {
        bool polled_since_last_close_test = true;
    } mSafetyState;
#endif
};
}
