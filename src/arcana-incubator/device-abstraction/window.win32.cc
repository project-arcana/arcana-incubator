#include "window.hh"

#include <clean-core/macros.hh>
#ifdef CC_OS_WINDOWS

#include <clean-core/array.hh>
#include <clean-core/assert.hh>

// clang-format off
#include <clean-core/native/detail/win32_sanitize_before.inl>

#include <Windows.h>
#include <ShellScalingApi.h>

#include <clean-core/native/detail/win32_sanitize_after.inl>
// clang-format on

struct inc::da::Window::event_proxy
{
    inc::da::Window* window = nullptr;
    inc::da::Window::win32_event_callback callback = nullptr;

    void delegateEventOnClose() const { window->onCloseEvent(); }
    void delegateEventResize(int w, int h, bool minimized) const { window->onResizeEvent(w, h, minimized); }
};

namespace
{
inc::da::Window::event_proxy s_event_proxy;
::HWND s_window_handle;

::LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (s_event_proxy.callback && s_event_proxy.callback(hWnd, msg, wParam, lParam))
    {
        return true;
    }


    switch (msg)
    {
    case WM_QUIT:
    case WM_CLOSE:
        s_event_proxy.delegateEventOnClose();
        return 0;
    case WM_DESTROY:
        ::PostQuitMessage(0);
        return 0;
    case WM_SIZE:
    {
        RECT client_rect;
        ::GetClientRect(hWnd, &client_rect);
        s_event_proxy.delegateEventResize(client_rect.right - client_rect.left, client_rect.bottom - client_rect.top, (::IsIconic(hWnd) == TRUE));
        return 0;
    }
    case WM_SYSCOMMAND:
        if ((wParam & 0xfff0) == SC_KEYMENU) // Disable ALT application menu
            return 0;
        break;
    }

    return DefWindowProc(hWnd, msg, wParam, lParam);
}
}

inc::da::Window::~Window()
{
    CC_ASSERT(s_event_proxy.window == this);
    ::DestroyWindow(s_window_handle);
    s_event_proxy.window = nullptr;
}

void inc::da::Window::initialize(const char* title, int width, int height)
{
    // No two windows allowed at the same time
    CC_ASSERT(s_event_proxy.window == nullptr);
    s_event_proxy.window = this;

    // Set process DPI awareness
    {
        // PER_MONITOR_AWARE: No scaling by Windows, no lies by GetClientRect
        // Possible alternative is PER_MONITOR_AWARE_V2, with better per-window DPI access,
        // but it requires the Win10 Creators update.
        // While this setting is definitively a good idea, support for HighDPI should
        // possibly play a role in the future da::Window
        ::SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE);
    }

    // Retrieve primary monitor scale factor and adapt width/height accordingly
    {
        POINT const zero_point = {0, 0};
        auto const primary_monitor = ::MonitorFromPoint(zero_point, MONITOR_DEFAULTTOPRIMARY);

        DEVICE_SCALE_FACTOR scale_factor;
        auto const hres = ::GetScaleFactorForMonitor(primary_monitor, &scale_factor);

        if (hres == S_OK && scale_factor != DEVICE_SCALE_FACTOR_INVALID)
        {
            mScaleFactor = int(scale_factor) / 100.f;
            width = int(width * mScaleFactor);
            height = int(height * mScaleFactor);
        }
    }

    mWidth = width;
    mHeight = height;

    // Window Setup
    WNDCLASS windowClass = {};
    windowClass.lpfnWndProc = WndProc;
    windowClass.hInstance = GetModuleHandle(nullptr);
    windowClass.hCursor = LoadCursor(nullptr, IDC_ARROW);
    windowClass.lpszClassName = "MiniWindow";
    RegisterClass(&windowClass);

    s_window_handle = CreateWindowEx(0, windowClass.lpszClassName, title, WS_OVERLAPPEDWINDOW | WS_VISIBLE, CW_USEDEFAULT, CW_USEDEFAULT, mWidth,
                                     mHeight, nullptr, nullptr, windowClass.hInstance, nullptr);
}

void inc::da::Window::pollEvents()
{
    ::MSG msg = {};
    while (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
    {
        ::TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
}

void* inc::da::Window::getNativeHandleA() const { return static_cast<void*>(s_window_handle); }

void* inc::da::Window::getNativeHandleB() const { return nullptr; }

void inc::da::Window::onCloseEvent() { mIsRequestingClose = true; }

void inc::da::Window::onResizeEvent(int w, int h, bool minimized)
{
    mWidth = w;
    mHeight = h;
    mIsMinimized = minimized;
    mPendingResize = true;
}

void inc::da::Window::setEventCallback(inc::da::Window::win32_event_callback callback) { s_event_proxy.callback = callback; }

#endif // CC_OS_WINDOWS
