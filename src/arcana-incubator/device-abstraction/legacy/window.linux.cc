#include "window.hh"

#include <clean-core/macros.hh>

#ifdef CC_OS_LINUX

#include <X11/Xlib.h>

#include <clean-core/array.hh>
#include <clean-core/bit_cast.hh>
#include <clean-core/capped_vector.hh>

namespace
{
::Display* s_display = nullptr;
::Window s_window;
::Window s_root_window;

int s_screen;

::Atom s_atom_wm_delete_window;
::Atom s_atom_wm_ping;
::Atom s_atom_wm_protocols;
}

inc::da::Window::~Window()
{
    CC_RUNTIME_ASSERT(s_display != nullptr);

    // A requested close is triggered by a WM_DELETE_WINDOW message, which already deletes everything, so this would be a double close
    if (!mIsRequestingClose)
        ::XCloseDisplay(s_display);

    s_display = nullptr;
}

void inc::da::Window::initialize(const char* title, int width, int height)
{
    CC_RUNTIME_ASSERT(s_display == nullptr && "Only one window allowed");
    s_display = ::XOpenDisplay(nullptr);
    CC_RUNTIME_ASSERT(s_display != nullptr && "Failed to open display");

    s_screen = DefaultScreen(s_display);
    s_root_window = RootWindow(s_display, s_screen);

    mWidth = width;
    mHeight = height;
    mPendingResize = true;

    {
        ::Visual* const visual = DefaultVisual(s_display, s_screen);
        int const depth = DefaultDepth(s_display, s_screen);
        ::Colormap const colormap = ::XCreateColormap(s_display, s_root_window, visual, AllocNone);

        XSetWindowAttributes window_attribs;
        window_attribs.colormap = colormap;
        window_attribs.border_pixel = 0;
        window_attribs.event_mask = StructureNotifyMask | PropertyChangeMask; /*| KeyPressMask | KeyReleaseMask | PointerMotionMask | ButtonPressMask
                                    | ButtonReleaseMask | ExposureMask | FocusChangeMask | VisibilityChangeMask | EnterWindowMask | LeaveWindowMask*/

        auto const window_attrib_mask = CWBorderPixel | CWColormap | CWEventMask;

        // Create a window with the given initial size
        // position arguments are ignored by most WMs
        s_window = XCreateWindow(s_display, s_root_window, 0, 0, unsigned(mWidth), unsigned(mHeight), 0, depth, InputOutput, visual,
                                 window_attrib_mask, &window_attribs);

        CC_RUNTIME_ASSERT(!!s_window && "Failed to create window");
    }

    {
        cc::capped_vector<::Atom, 5> atoms;

        // WM window close event
        atoms.push_back(s_atom_wm_delete_window = ::XInternAtom(s_display, "WM_DELETE_WINDOW", 0));
        // WM ping (checks if the window is still alive)
        atoms.push_back(s_atom_wm_ping = ::XInternAtom(s_display, "_NET_WM_PING", 0));

        // Subscribe to the atom events
        ::XSetWMProtocols(s_display, s_window, atoms.data(), int(atoms.size()));

        // Store the WM protocols atom for discerning events
        s_atom_wm_protocols = ::XInternAtom(s_display, "WM_PROTOCOLS", 0);
    }

    // Set the window title
    ::XStoreName(s_display, s_window, title);

    // Map the window and flush pending outgoing X messages
    ::XMapWindow(s_display, s_window);
    ::XFlush(s_display);

    // Block until the window is mapped
    while (true)
    {
        ::XEvent e;
        ::XNextEvent(s_display, &e);
        if (e.type == MapNotify)
            break;
    }

    // Fetch and update the initial size
    {
        ::XWindowAttributes attribs;
        ::XGetWindowAttributes(s_display, s_window, &attribs);
        mWidth = attribs.width;
        mHeight = attribs.height;
    }
}

void inc::da::Window::pollEvents()
{
    ::XPending(s_display);
    while (::XQLength(s_display) > 0)
    {
        ::XEvent e;
        ::XNextEvent(s_display, &e);
        if (e.type == ConfigureNotify)
        {
            XConfigureEvent const& xce = e.xconfigure;
            onResizeEvent(xce.width, xce.height, false);
        }
        else if (e.type == ClientMessage)
        {
            if (e.xclient.message_type == None)
                continue;

            if (e.xclient.message_type == s_atom_wm_protocols)
            {
                auto const& atom = e.xclient.data.l[0];

                if (atom == None)
                    continue;

                if (atom == long(s_atom_wm_delete_window))
                    // Window closed by the WM
                    onCloseEvent();
                else if (atom == long(s_atom_wm_ping))
                {
                    // WM checks if this window is still responding
                    ::XEvent reply = e;
                    reply.xclient.window = s_window;
                    ::XSendEvent(s_display, s_window, 0, SubstructureNotifyMask | SubstructureRedirectMask, &reply);
                }
            }
        }
    }

    ::XFlush(s_display);
}

void* inc::da::Window::getNativeHandleA() const { return cc::bit_cast<void*>(s_window); }

void* inc::da::Window::getNativeHandleB() const { return static_cast<void*>(s_display); }

void inc::da::Window::onCloseEvent() { mIsRequestingClose = true; }

void inc::da::Window::onResizeEvent(int w, int h, bool minimized)
{
    if (mWidth == w && mHeight == h && mIsMinimized == minimized)
        return;

    mWidth = w;
    mHeight = h;
    mIsMinimized = minimized;
    mPendingResize = true;
}

#endif // CC_OS_LINUX
