#pragma once

#include <phantasm-renderer/Context.hh>

#include <arcana-incubator/device-abstraction/device_abstraction.hh>
#include <arcana-incubator/device-abstraction/freefly_camera.hh>
#include <arcana-incubator/device-abstraction/input.hh>
#include <arcana-incubator/device-abstraction/timer.hh>

#include <arcana-incubator/imgui/imgui_impl_pr.hh>

namespace inc::pre
{
// simple out of the box mainloop with timing and imgui
// intended as a starting/reference point, a more elaborate project would copypaste parts
// not intended to bloat into a GlfwApp 2.0
struct quick_app
{
    pr::Context context;
    da::SDLWindow window;

    da::input_manager input;
    da::smooth_fps_cam camera;
    da::Timer timer;

    ImGuiPhantasmImpl imgui;

    quick_app(pr::backend backend = pr::backend::vulkan, bool validate = true) { _init(backend, validate); }
    ~quick_app() { _destroy(); }
    quick_app(quick_app const&) = delete;
    quick_app(quick_app&&) = delete;

    /// canonical main loop with a provided lambda
    template <class F>
    void main_loop(F&& func)
    {
        timer.restart();
        while (!window.isRequestingClose())
        {
            if (!_on_frame_start())
                continue;

            auto const dt = timer.elapsedSeconds();
            timer.restart();

            camera.update_default_inputs(window.getSdlWindow(), input, dt);
            func(dt);
        }

        context.flush();
    }

    /// draw a window containing camera state, frametime and control info
    void perform_default_imgui(float dt) const;

    /// use to render imgui, given a frame and an already acquired backbuffer
    void render_imgui(pr::raii::Frame& frame, pr::render_target const& backbuffer);

private:
    void _init(pr::backend backend, bool validate);

    /// perform start-of-frame event handling, called in main_loop
    bool _on_frame_start();

    void _destroy();
};

}
