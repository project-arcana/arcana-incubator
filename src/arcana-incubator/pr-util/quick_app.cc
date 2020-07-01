#include "quick_app.hh"

#include <phantasm-renderer/Frame.hh>

#include <arcana-incubator/imgui/imgui_impl_sdl2.hh>
#include <arcana-incubator/imgui/imguizmo/imguizmo.hh>

#include <phantasm-hardware-interface/config.hh>

void inc::pre::quick_app::perform_default_imgui(float dt) const
{
    ImGui::Begin("quick_app default window", nullptr, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoTitleBar);
    ImGui::SetWindowSize(ImVec2{210, 160}, ImGuiCond_Always);
    ImGui::Text("frametime: %.2f ms", double(dt * 1000.f));
    ImGui::Text("cam pos: %.2f %.2f %.2f", double(camera.physical.position.x), double(camera.physical.position.y), double(camera.physical.position.z));
    ImGui::Text("cam fwd: %.2f %.2f %.2f", double(camera.physical.forward.x), double(camera.physical.forward.y), double(camera.physical.forward.z));
    ImGui::Separator();
    ImGui::Text("WASD - move\nE/Q - raise/lower\nhold RMB - mouselook\nshift - speedup\nctrl - slowdown");
    ImGui::End();
}

void inc::pre::quick_app::render_imgui(pr::raii::Frame& frame, const pr::render_target& backbuffer)
{
    ImGui::Render();
    auto* const drawdata = ImGui::GetDrawData();
    auto const framesize = imgui.get_command_size(drawdata);

    frame.begin_debug_label("imgui");
    frame.transition(backbuffer, pr::state::render_target);
    imgui.write_commands(drawdata, backbuffer.res.handle, frame.write_raw_bytes(framesize), framesize);
    frame.end_debug_label();
}

void inc::pre::quick_app::_init(pr::backend backend, bool validate)
{
    // core
    da::initialize(); // SDL Init
    window.initialize("quick_app window");

    phi::backend_config conf;
    conf.validation = validate ? phi::validation_level::on_extended : phi::validation_level::off;
    context.initialize({window.getSdlWindow()}, backend, conf);

    // input + camera
    input.initialize();
    camera.setup_default_inputs(input);

    // imgui
    ImGui::SetCurrentContext(ImGui::CreateContext(nullptr));
    ImGui_ImplSDL2_Init(window.getSdlWindow());
    imgui.initialize_with_contained_shaders(&context.get_backend());
}

bool inc::pre::quick_app::_on_frame_start()
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

    // imgui new frame
    ImGui_ImplSDL2_NewFrame(window.getSdlWindow());
    ImGui::NewFrame();

    // imguizmo new frame
    ImGuizmo::BeginFrame();
    ImGuiIO& io = ImGui::GetIO();
    ImGuizmo::SetRect(0, 0, io.DisplaySize.x, io.DisplaySize.y);

    return true;
}

void inc::pre::quick_app::_destroy()
{
    context.flush();

    imgui.destroy();
    ImGui_ImplSDL2_Shutdown();

    context.destroy();
    window.destroy();
    da::shutdown();
}
