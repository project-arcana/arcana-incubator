#include "device_abstraction.hh"

#include <SDL2/SDL.h>

void inc::da::initialize()
{
    if (SDL_Init(SDL_INIT_EVERYTHING) != 0)
    {
        SDL_Log("Unable to initialize SDL: %s\n", SDL_GetError());
    }
}

void inc::da::shutdown()
{
    SDL_Quit();
}

void inc::da::SDLWindow::initialize(const char *title, int width, int height, bool enable_vk)
{
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
        SDL_DestroyWindow(mWindow);
}

void inc::da::SDLWindow::pollEvents()
{
    SDL_Event e;

    while (SDL_PollEvent(&e) != 0)
    {
        if (mEventCallback && mEventCallback(&e))
        {
            continue;
        }

        if (e.type == SDL_QUIT)
        {
            mIsRequestingClose = true;
        }
        else if (e.type == SDL_WINDOWEVENT || e.type == SDL_DISPLAYEVENT)
        {
            int new_w, new_h;
            SDL_GetWindowSize(mWindow, &new_w, &new_h);
            onResizeEvent(new_w, new_h, false);
        }
    }
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
