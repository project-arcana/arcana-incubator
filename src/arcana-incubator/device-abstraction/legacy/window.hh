#pragma once

#include <clean-core/assert.hh>
#include <clean-core/macros.hh>

#ifdef CC_OS_WINDOWS
#include <clean-core/native/win32_fwd.hh>
#endif

namespace inc::da
{
/// Tentative precursor of eventual device-abstraction library
/// Only one allowed at the time
class Window
{
public:
    struct event_proxy;

    // reference type
public:
    Window() = default;
    Window(Window const&) = delete;
    Window(Window&&) noexcept = delete;
    Window& operator=(Window const&) = delete;
    Window& operator=(Window&&) noexcept = delete;
    ~Window();

    /// initialize the window with a given title and initial size
    void initialize(char const* title, int width = 1280, int height = 720);

    /// poll events by the WM/OS
    void pollEvents();

    /// whether a close event has been fired
    [[nodiscard]] bool isRequestingClose() const { return mIsRequestingClose; }

    /// whether a resize occured since the last ::clearPendingResize()
    [[nodiscard]] bool isPendingResize() const { return mPendingResize; }

    /// clears pending resizes
    void clearPendingResize() { mPendingResize = false; }

    [[nodiscard]] int getWidth() const { return mWidth; }
    [[nodiscard]] int getHeight() const { return mHeight; }
    [[nodiscard]] bool isMinimized() const { return mIsMinimized; }
    [[nodiscard]] float getScaleFactor() const { return mScaleFactor; }

    [[nodiscard]] void* getNativeHandleA() const;
    [[nodiscard]] void* getNativeHandleB() const;

public:
#ifdef CC_OS_WINDOWS
    using win32_event_callback = LRESULT (*)(HWND, UINT, WPARAM, LPARAM);
    void setEventCallback(win32_event_callback callback);
#endif

private:
    void onCloseEvent();
    void onResizeEvent(int w, int h, bool minimized);

    int mWidth = 0;
    int mHeight = 0;
    float mScaleFactor = 1.f;
    bool mIsMinimized = false;
    bool mPendingResize = false;
    bool mIsRequestingClose = false;
};

}
