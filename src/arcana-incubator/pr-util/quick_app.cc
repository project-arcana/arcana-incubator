#include "quick_app.hh"

#include <phantasm-renderer/Frame.hh>

#include <arcana-incubator/imgui/imgui_impl_phi.hh>
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

void inc::pre::quick_app::initialize(pr::backend backend_type, const phi::backend_config& config)
{
    // core
    da::initialize(); // SDL Init
    window.initialize("quick_app window");

    context.initialize(backend_type, config);
    main_swapchain = context.make_swapchain({window.getSdlWindow()}, window.getSize()).unlock();

    // input + camera
    input.initialize();
    camera.setup_default_inputs(input);

    // imgui
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    auto& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // Enable keyboard controls
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;     // Enable docking
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;   // Enable multi-viewport

    if (context.get_backend_type() == pr::backend::d3d12)
        ImGui_ImplSDL2_InitForD3D(window.getSdlWindow());
    else
        ImGui_ImplSDL2_InitForVulkan(window.getSdlWindow());

    ImGui_ImplPHI_Init(&context.get_backend(), context.get_num_backbuffers(main_swapchain), context.get_backbuffer_format(main_swapchain));
}

bool inc::pre::quick_app::_on_frame_start()
{
    input.updatePrePoll();
    SDL_Event e;
    while (window.pollSingleEvent(e))
    {
        ImGui_ImplSDL2_ProcessEvent(&e);
        input.processEvent(e);
    }
    input.updatePostPoll();

    if (window.isMinimized())
        return false;

    if (window.clearPendingResize())
        context.on_window_resize(main_swapchain, window.getSize());

    // imgui new frame
    ImGui_ImplPHI_NewFrame();
    ImGui_ImplSDL2_NewFrame(window.getSdlWindow());
    ImGui::NewFrame();

    // imguizmo new frame
    ImGuiViewport const& viewport = *ImGui::GetMainViewport();
    ImGuizmo::BeginFrame();
    ImGuizmo::SetRect(viewport.Pos.x, viewport.Pos.y, viewport.Size.x, viewport.Size.y);

    return true;
}

void inc::pre::quick_app::destroy()
{
    if (context.is_initialized())
    {
        context.flush();
        ImGui_ImplPHI_Shutdown();
        ImGui_ImplSDL2_Shutdown();

        context.free(main_swapchain);
        context.destroy();
        window.destroy();
        da::shutdown();
    }
}

tg::vec2 inc::pre::quick_app::get_normalized_mouse_pos() const
{
    tg::isize2 const res = window.getSize();
    tg::ivec2 const mousepos = input.getMousePositionRelative();
    return tg::vec2(mousepos.x / float(res.width), mousepos.y / float(res.height));
}
