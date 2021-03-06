#pragma once

#include <clean-core/function_ref.hh>

#include <phantasm-hardware-interface/config.hh>

#include <phantasm-renderer/Context.hh>

#include <arcana-incubator/device-abstraction/device_abstraction.hh>
#include <arcana-incubator/device-abstraction/freefly_camera.hh>
#include <arcana-incubator/device-abstraction/input.hh>
#include <arcana-incubator/device-abstraction/timer.hh>

namespace inc::pre
{
// simple out of the box mainloop with timing and imgui
// intended as a starting/reference point, a more elaborate project would copypaste parts
// not intended to bloat into a GlfwApp 2.0
struct quick_app
{
    pr::Context context;
    da::SDLWindow window;
    pr::swapchain main_swapchain;

    da::input_manager input;
    da::smooth_fps_cam camera;
    da::Timer timer;

    quick_app() = default;
    explicit quick_app(pr::backend backend_type, phi::backend_config const& config = {}) { initialize(backend_type, config); }
    quick_app(quick_app const&) = delete;
    quick_app(quick_app&&) = delete;

    ~quick_app() { destroy(); }

    void initialize(pr::backend backend_type = pr::backend::vulkan, phi::backend_config const& config = {});

    void destroy();

    /// canonical main loop with a provided lambda, receives delta time in seconds
    void main_loop(cc::function_ref<void(double)> func);

    /// draw a window containing camera state, frametime and control info
    void perform_default_imgui(double dt) const;

    // utility
public:
    tg::vec2 get_normalized_mouse_pos() const;

private:
    /// perform start-of-frame event handling, called in main_loop
    bool _on_frame_start();
};

}
