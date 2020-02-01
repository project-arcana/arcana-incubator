#pragma once

union SDL_Event;
struct SDL_Window;

namespace inc::da
{
void initialize();

void shutdown();

class SDLWindow
{
public:
    SDLWindow() = default;
    SDLWindow(SDLWindow const&) = delete;
    SDLWindow(SDLWindow&&) noexcept = delete;
    SDLWindow& operator=(SDLWindow const&) = delete;
    SDLWindow& operator=(SDLWindow&&) noexcept = delete;

    void initialize(char const* title, int width = 850, int height = 550, bool enable_vk = true);

    void destroy();

    /// poll events by the WM/OS
    void pollEvents();

    /// whether a close event has been fired
    [[nodiscard]] bool isRequestingClose() const { return mIsRequestingClose; }

    /// whether a resize occured since the last ::clearPendingResize()
    /// clears pending resizes
    [[nodiscard]] bool clearPendingResize()
    {
        auto const res = mPendingResize;
        mPendingResize = false;
        return res;
    }

    [[nodiscard]] int getWidth() const { return mWidth; }
    [[nodiscard]] int getHeight() const { return mHeight; }
    [[nodiscard]] bool isMinimized() const { return mIsMinimized; }
    [[nodiscard]] float getScaleFactor() const { return mScaleFactor; }

    [[nodiscard]] SDL_Window* getSdlWindow() const { return mWindow; }

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
};
}
