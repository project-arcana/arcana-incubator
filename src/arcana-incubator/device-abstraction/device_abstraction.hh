#pragma once

#include <clean-core/assert.hh>

#include <typed-geometry/types/size.hh>

union SDL_Event;
struct SDL_Window;

namespace inc::da
{
void initialize();

void shutdown();

class SDLWindow
{
public:
    void initialize(char const* title, int width = 850, int height = 550, bool enable_vk = true);
    void destroy();

    SDLWindow() = default;
    SDLWindow(char const* title, int width = 850, int height = 550, bool enable_vk = true) { initialize(title, width, height, enable_vk); }
    SDLWindow(SDLWindow const&) = delete;
    SDLWindow(SDLWindow&&) noexcept = delete;
    SDLWindow& operator=(SDLWindow const&) = delete;
    SDLWindow& operator=(SDLWindow&&) noexcept = delete;
    ~SDLWindow() { destroy(); }

    /// poll events by the WM/OS
    void pollEvents();

    /// poll a single SDL event, use while(pollSingleEvent(e)) {..}
    bool pollSingleEvent(SDL_Event& out_event);

    /// whether a close event has been fired
    [[nodiscard]] bool isRequestingClose()
    {
        CC_ASSERT(mSafetyState.polled_since_last_close_test && "forgot to poll window events in while loop body?");
#ifdef CC_ENABLE_ASSERTIONS
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

    int getWidth() const { return mWidth; }
    int getHeight() const { return mHeight; }
    tg::isize2 getSize() const { return {mWidth, mHeight}; }
    bool isMinimized() const { return mIsMinimized; }
    float getScaleFactor() const { return mScaleFactor; }

    SDL_Window* getSdlWindow() const { return mWindow; }

public:
    using event_callback = bool (*)(SDL_Event const* e);

    void setEventCallback(event_callback callback) { mEventCallback = callback; }

private:
    void onResizeEvent(int w, int h, bool minimized);

    SDL_Window* mWindow = nullptr;
    event_callback mEventCallback = nullptr;

    int mWidth = 0;
    int mHeight = 0;
    float mScaleFactor = 1.f;
    bool mIsMinimized = false;
    bool mPendingResize = false;
    bool mIsRequestingClose = false;

#ifdef CC_ENABLE_ASSERTIONS
    struct
    {
        bool polled_since_last_close_test = true;
    } mSafetyState;
#endif
};
}
