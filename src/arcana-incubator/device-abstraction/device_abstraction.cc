#include "device_abstraction.hh"

#include <clean-core/assert.hh>

#include <SDL2/SDL.h>

void inc::da::initialize()
{
    SDL_SetMainReady(); // we use SDL_MAIN_HANDLED (in CMakeLists.txt), see https://wiki.libsdl.org/SDL_SetMainReady
    if (SDL_Init(SDL_INIT_EVERYTHING) != 0)
    {
        SDL_Log("Unable to initialize SDL: %s\n", SDL_GetError());
        return;
    }

    SDL_JoystickEventState(SDL_ENABLE);
}

void inc::da::shutdown() { SDL_Quit(); }

void inc::da::SDLWindow::initialize(const char* title, int width, int height, bool enable_vk)
{
    CC_ASSERT(mWindow == nullptr && "double init");

    uint32_t flags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI;
    if (enable_vk)
        flags |= SDL_WINDOW_VULKAN;

    mWindow = SDL_CreateWindow(title, SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, width, height, flags);

    if (mWindow == nullptr)
    {
        SDL_Log("Failed to create window: %s\n", SDL_GetError());
    }

    onResizeEvent(width, height, false);
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
            int new_w, new_h;
            SDL_GetWindowSize(mWindow, &new_w, &new_h);
            bool const is_minimized = SDL_GetWindowFlags(mWindow) & SDL_WINDOW_MINIMIZED;
            onResizeEvent(new_w, new_h, is_minimized);
        }
    }

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
