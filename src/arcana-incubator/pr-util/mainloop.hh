#pragma once

#include <phantasm-renderer/Context.hh>
#include <phantasm-renderer/Frame.hh>

#include <arcana-incubator/device-abstraction/device_abstraction.hh>
#include <arcana-incubator/device-abstraction/freefly_camera.hh>
#include <arcana-incubator/device-abstraction/input.hh>
#include <arcana-incubator/device-abstraction/timer.hh>

#include <arcana-incubator/imgui/imgui_impl_pr.hh>
#include <arcana-incubator/imgui/imgui_impl_sdl2.hh>

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

    void init()
    {
        // core
        da::initialize(); // SDL Init
        window.initialize("quick_app window");
        context.initialize({window.getSdlWindow()}, pr::backend::vulkan);

        // input + camera
        input.initialize();
        camera.setup_default_inputs(input);

        // imgui
        ImGui::SetCurrentContext(ImGui::CreateContext(nullptr));
        ImGui_ImplSDL2_Init(window.getSdlWindow());
        imgui.initialize_with_contained_shaders(&context.get_backend());
    }

    /// perform start-of-frame event handling, automatically called in main_loop
    bool on_frame_start()
    {
        input.updatePrePoll();
        SDL_Event e;
        while (window.pollSingleEvent(e))
            input.processEvent(e);
        input.updatePostPoll();

        if (window.isMinimized())
            return false;

        if (window.clearPendingResize())
            context.on_window_resize(window.getSize());

        ImGui_ImplSDL2_NewFrame(window.getSdlWindow());
        ImGui::NewFrame();

        return true;
    }

    /// canonical main loop with a provided lambda
    template <class F>
    void main_loop(F&& func)
    {
        timer.restart();
        while (!window.isRequestingClose())
        {
            if (!on_frame_start())
                continue;

            auto const dt = timer.elapsedSeconds();
            timer.restart();
            func(dt);
        }

        context.flush();
    }

    /// use to render imgui, given a frame and an already acquired backbuffer
    void render_imgui(pr::raii::Frame& frame, pr::render_target const& backbuffer)
    {
        ImGui::Render();
        auto* const drawdata = ImGui::GetDrawData();
        auto const framesize = imgui.get_command_size(drawdata);

        frame.begin_debug_label("imgui");
        imgui.write_commands(drawdata, backbuffer.res.handle, frame.write_raw_bytes(framesize), framesize);
        frame.end_debug_label();
    }

    void destroy()
    {
        context.flush();

        imgui.destroy();
        ImGui_ImplSDL2_Shutdown();

        context.destroy();
        window.destroy();
        da::shutdown();
    }
};

}
