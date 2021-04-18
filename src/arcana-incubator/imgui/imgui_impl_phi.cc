#include "imgui_impl_phi.hh"

#include <rich-log/log.hh>

#include <clean-core/array.hh>
#include <clean-core/bit_cast.hh>
#include <clean-core/utility.hh>

#include <phantasm-hardware-interface/Backend.hh>
#include <phantasm-hardware-interface/arguments.hh>
#include <phantasm-hardware-interface/commands.hh>
#include <phantasm-hardware-interface/types.hh>
#include <phantasm-hardware-interface/util.hh>
#include <phantasm-hardware-interface/window_handle.hh>

#include <dxc-wrapper/compiler.hh>

#include <arcana-incubator/phi-util/texture_util.hh>

namespace
{
[[nodiscard]] phi::handle::shader_view imgui_to_sv(ImTextureID itd)
{
    phi::handle::shader_view res;
    std::memcpy(&res, &itd, sizeof(res));
    return res;
}

[[nodiscard]] ImTextureID sv_to_imgui(phi::handle::shader_view sv)
{
    ImTextureID res;
    std::memcpy(&res, &sv, sizeof(sv));
    return res;
}

phi::Backend* g_backend = nullptr;
int g_num_frames_in_flight = 0;
phi::format g_backbuf_format = phi::format::bgra8un;
phi::handle::pipeline_state g_pipeline_state = phi::handle::null_pipeline_state;
phi::handle::resource g_font_texture = phi::handle::null_resource;
phi::handle::shader_view g_font_texture_sv = phi::handle::null_shader_view;


struct ImGui_ImplPHI_RenderBuffers
{
    phi::handle::resource index_buf = phi::handle::null_resource;
    int index_buf_size = 0; ///< size of the index buffer, in number of indices
    phi::handle::resource vertex_buf = phi::handle::null_resource;
    int vertex_buf_size = 0; ///< size of the vertex buffer, in number of vertices
    phi::handle::resource mvp_buffer = phi::handle::null_resource;

    void destroy()
    {
        if (index_buf.is_valid())
            g_backend->free(index_buf);
        if (vertex_buf.is_valid())
            g_backend->free(vertex_buf);
        if (mvp_buffer.is_valid())
            g_backend->free(mvp_buffer);

        index_buf = phi::handle::null_resource;
        vertex_buf = phi::handle::null_resource;
        mvp_buffer = phi::handle::null_resource;
    }
};

struct ImGuiViewportDataPHI
{
    // only valid for non-main viewports
    phi::handle::swapchain swapchain = phi::handle::null_swapchain;

    // Render buffers
    unsigned frame_index = 0;
    cc::array<ImGui_ImplPHI_RenderBuffers> frame_render_buffers;

    ImGuiViewportDataPHI() { frame_render_buffers = frame_render_buffers.defaulted(g_num_frames_in_flight); }
};
}

// Forward Declarations
static void ImGui_ImplPHI_InitPlatformInterface();
static void ImGui_ImplPHI_ShutdownPlatformInterface();

bool ImGui_ImplPHI_Init(phi::Backend* backend, int num_frames_in_flight, phi::format target_format)
{
    constexpr char const* imgui_code
        = R"(cbuffer _0:register(b0,space0){float4x4 prj;};struct vit{float2 pos:POSITION;float2 uv:TEXCOORD0;float4 col:COLOR0;};struct pit{float4 pos:SV_POSITION;float4 col:COLOR0;float2 uv:TEXCOORD0;};struct vgt{float4x4 prj;};ConstantBuffer<vgt> s:register(b0,space0);SamplerState p:register(s0,space0);Texture2D v:register(t0,space0);pit r(vit p){pit v;v.pos=mul(s.prj,float4(p.pos.xy,0.f,1.f));v.col=p.col;v.uv=p.uv;return v;}float4 u(pit s):SV_TARGET{float4 u=s.col*v.Sample(p,s.uv);return u;})";

    auto const output = backend->getBackendType() == phi::backend_type::d3d12 ? dxcw::output::dxil : dxcw::output::spirv;

    dxcw::compiler comp;
    comp.initialize();
    auto const vs = comp.compile_shader(imgui_code, "r", dxcw::target::vertex, output);
    auto const ps = comp.compile_shader(imgui_code, "u", dxcw::target::pixel, output);
    comp.destroy();

    bool const res = ImGui_ImplPHI_InitWithShaders(backend, num_frames_in_flight, target_format, {ps.data, ps.size}, {vs.data, vs.size});

    dxcw::destroy_blob(vs.internal_blob);
    dxcw::destroy_blob(ps.internal_blob);

    return res;
}


bool ImGui_ImplPHI_InitWithShaders(phi::Backend* backend, int num_frames_in_flight, phi::format target_format, cc::span<const std::byte> ps_data, cc::span<const std::byte> vs_data)
{
    CC_ASSERT(g_backend == nullptr && "double init");
    CC_ASSERT(backend != nullptr);

    // Setup back-end capabilities flags
    ImGuiIO& io = ImGui::GetIO();
    io.BackendRendererName = "imgui_impl_phi";
    io.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset; // We can honor the ImDrawCmd::VtxOffset field, allowing for large meshes.
    io.BackendFlags |= ImGuiBackendFlags_RendererHasViewports; // We can create multi-viewports on the Renderer side (optional) // FIXME-VIEWPORT: Actually unfinished.

    g_backend = backend;
    g_num_frames_in_flight = num_frames_in_flight;
    g_backbuf_format = target_format;

    // PSO
    {
        phi::vertex_attribute_info vert_attrs[] = {phi::vertex_attribute_info{"POSITION", unsigned(IM_OFFSETOF(ImDrawVert, pos)), phi::format::rg32f},
                                                   phi::vertex_attribute_info{"TEXCOORD", unsigned(IM_OFFSETOF(ImDrawVert, uv)), phi::format::rg32f},
                                                   phi::vertex_attribute_info{"COLOR", unsigned(IM_OFFSETOF(ImDrawVert, col)), phi::format::rgba8un}};

        phi::arg::vertex_format vert_format;
        vert_format.attributes = vert_attrs;
        vert_format.vertex_sizes_bytes[0] = sizeof(ImDrawVert);

        phi::arg::framebuffer_config fb_format;
        fb_format.add_render_target(target_format);
        {
            auto& rt = fb_format.render_targets.back();
            rt.blend_enable = true;
            rt.state.blend_color_src = phi::blend_factor::src_alpha;
            rt.state.blend_color_dest = phi::blend_factor::inv_src_alpha;
            rt.state.blend_op_color = phi::blend_op::op_add;
            rt.state.blend_alpha_src = phi::blend_factor::inv_src_alpha;
            rt.state.blend_alpha_dest = phi::blend_factor::zero;
            rt.state.blend_op_alpha = phi::blend_op::op_add;
        }

        phi::arg::shader_arg_shape shader_shape;
        shader_shape.has_cbv = true;
        shader_shape.num_srvs = 1;
        shader_shape.num_samplers = 1;

        phi::arg::graphics_shader shader_stages[] = {phi::arg::graphics_shader{{vs_data.data(), vs_data.size()}, phi::shader_stage::vertex},
                                                     phi::arg::graphics_shader{{ps_data.data(), ps_data.size()}, phi::shader_stage::pixel}};

        phi::pipeline_config config;
        config.depth = phi::depth_function::none;
        config.cull = phi::cull_mode::none;

        g_pipeline_state = backend->createPipelineState(vert_format, fb_format, cc::span{shader_shape}, false, shader_stages, config);
    }

    // Font tex
    {
        unsigned char* pixels;
        int width, height;

        io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
        g_font_texture = backend->createTexture(phi::format::rgba8un, {width, height}, 1);

        phi::resource_view tex_sve;
        tex_sve.init_as_tex2d(g_font_texture, phi::format::rgba8un);

        phi::sampler_config sampler;
        sampler.init_default(phi::sampler_filter::min_mag_mip_linear);

        g_font_texture_sv = backend->createShaderView(cc::span{tex_sve}, {}, cc::span{sampler});

        bool const is_d3d12 = backend->getBackendType() == phi::backend_type::d3d12;

        uint32_t const upbuff_size = phi::util::get_texture_size_bytes({width, height, 1}, phi::format::rgba8un, 1, is_d3d12);
        phi::handle::resource temp_upload_buffer = backend->createBuffer(upbuff_size, 0, phi::resource_heap::upload, false);


        {
            std::byte buffer[2 * sizeof(phi::cmd::transition_resources) + sizeof(phi::cmd::copy_buffer_to_texture)];
            phi::command_stream_writer writer(buffer, sizeof(buffer));

            auto& tcmd = writer.emplace_command<phi::cmd::transition_resources>();
            tcmd.add(g_font_texture, phi::resource_state::copy_dest);

            inc::copy_data_to_texture(writer, temp_upload_buffer, backend->mapBuffer(temp_upload_buffer), g_font_texture, phi::format::rgba8un,
                                      unsigned(width), unsigned(height), reinterpret_cast<std::byte const*>(pixels), is_d3d12);
            backend->unmapBuffer(temp_upload_buffer);

            auto& tcmd2 = writer.emplace_command<phi::cmd::transition_resources>();
            tcmd2.add(g_font_texture, phi::resource_state::shader_resource, phi::shader_stage::pixel);

            auto const cmdl = backend->recordCommandList(writer.buffer(), writer.size());
            backend->submit(cc::span{cmdl});
        }

        io.Fonts->TexID = sv_to_imgui(g_font_texture_sv);

        backend->flushGPU();
        backend->free(temp_upload_buffer);
    }

    // Create a dummy ImGuiViewportDataPHI holder for the main viewport,
    // Since this is created and managed by the application, we will only use the ->Resources[] fields.
    ImGuiViewport* main_viewport = ImGui::GetMainViewport();

    ImGuiViewportDataPHI* const main_viewport_renderdata = IM_NEW(ImGuiViewportDataPHI)();
    main_viewport->RendererUserData = main_viewport_renderdata;


    // Setup back-end capabilities flags
    io.BackendFlags |= ImGuiBackendFlags_RendererHasViewports; // We can create multi-viewports on the Renderer side (optional)
    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
        ImGui_ImplPHI_InitPlatformInterface();

    return true;
}


void ImGui_ImplPHI_Shutdown()
{
    if (g_backend == nullptr)
    {
        return;
    }

    // Manually delete main viewport render resources in-case we haven't initialized for viewports
    ImGuiViewport* main_viewport = ImGui::GetMainViewport();
    if (ImGuiViewportDataPHI* data = (ImGuiViewportDataPHI*)main_viewport->RendererUserData)
    {
        for (auto i = 0; i < g_num_frames_in_flight; i++)
        {
            data->frame_render_buffers[i].destroy();
        }
        IM_DELETE(data);
        main_viewport->RendererUserData = nullptr;
    }

    // Clean up windows and device objects
    ImGui_ImplPHI_ShutdownPlatformInterface();

    g_backend->free(g_pipeline_state);
    g_backend->free(g_font_texture);
    g_backend->free(g_font_texture_sv);
    g_backend = nullptr;

    ImGuiIO& io = ImGui::GetIO();
    io.Fonts->TexID = nullptr;
}

void ImGui_ImplPHI_NewFrame()
{ // nothing has to be done here
}

void ImGui_ImplPHI_RenderDrawData(const ImDrawData* draw_data, cc::span<std::byte> out_cmdbuffer)
{
    CC_ASSERT(g_backend != nullptr && "missing ImGui_ImplPHI_Init call");
    // Avoid rendering when minimized
    if (draw_data->DisplaySize.x <= 0.0f || draw_data->DisplaySize.y <= 0.0f)
    {
        return;
    }

    ImGuiViewportDataPHI* const render_data = (ImGuiViewportDataPHI*)draw_data->OwnerViewport->RendererUserData;
    render_data->frame_index = cc::wrapped_increment(render_data->frame_index, unsigned(g_num_frames_in_flight));
    ImGui_ImplPHI_RenderBuffers& frame_res = render_data->frame_render_buffers[render_data->frame_index];

    if (!frame_res.index_buf.is_valid() || frame_res.index_buf_size < draw_data->TotalIdxCount)
    {
        if (frame_res.index_buf.is_valid())
            g_backend->free(frame_res.index_buf);

        frame_res.index_buf_size = draw_data->TotalIdxCount + 10000;
        frame_res.index_buf = g_backend->createUploadBuffer(frame_res.index_buf_size * sizeof(ImDrawIdx), sizeof(ImDrawIdx));
    }

    if (!frame_res.vertex_buf.is_valid() || frame_res.vertex_buf_size < draw_data->TotalVtxCount)
    {
        if (frame_res.vertex_buf.is_valid())
            g_backend->free(frame_res.vertex_buf);

        frame_res.vertex_buf_size = draw_data->TotalVtxCount + 5000;
        frame_res.vertex_buf = g_backend->createUploadBuffer(frame_res.vertex_buf_size * sizeof(ImDrawVert), sizeof(ImDrawVert));
    }

    if (!frame_res.mvp_buffer.is_valid())
    {
        frame_res.mvp_buffer = g_backend->createUploadBuffer(sizeof(float[4][4]));
    }

    // upload vertices and indices
    {
        auto* const vertex_map = g_backend->mapBuffer(frame_res.vertex_buf);
        auto* const index_map = g_backend->mapBuffer(frame_res.index_buf);

        ImDrawVert* vtx_dst = (ImDrawVert*)vertex_map;
        ImDrawIdx* idx_dst = (ImDrawIdx*)index_map;

        for (auto n = 0; n < draw_data->CmdListsCount; ++n)
        {
            ImDrawList const* const cmd_list = draw_data->CmdLists[n];
            std::memcpy(vtx_dst, cmd_list->VtxBuffer.Data, cmd_list->VtxBuffer.Size * sizeof(ImDrawVert));
            std::memcpy(idx_dst, cmd_list->IdxBuffer.Data, cmd_list->IdxBuffer.Size * sizeof(ImDrawIdx));

            vtx_dst += cmd_list->VtxBuffer.Size;
            idx_dst += cmd_list->IdxBuffer.Size;
        }

        g_backend->unmapBuffer(frame_res.vertex_buf);
        g_backend->unmapBuffer(frame_res.index_buf);
    }

    // upload VP matrix
    {
        float L = draw_data->DisplayPos.x;
        float R = draw_data->DisplayPos.x + draw_data->DisplaySize.x;
        float T = draw_data->DisplayPos.y;
        float B = draw_data->DisplayPos.y + draw_data->DisplaySize.y;
        float mvp[4][4] = {
            {2.0f / (R - L), 0.0f, 0.0f, 0.0f},
            {0.0f, 2.0f / (T - B), 0.0f, 0.0f},
            {0.0f, 0.0f, 0.5f, 0.0f},
            {(R + L) / (L - R), (T + B) / (B - T), 0.5f, 1.0f},
        };

        auto* const const_buffer_map = g_backend->mapBuffer(frame_res.mvp_buffer);
        std::memcpy(const_buffer_map, mvp, sizeof(mvp));
        g_backend->unmapBuffer(frame_res.mvp_buffer);
    }

    auto global_vtx_offset = 0;
    auto global_idx_offset = 0;
    ImVec2 const clip_offset = draw_data->DisplayPos;

    auto writer = phi::command_stream_writer(out_cmdbuffer.data(), out_cmdbuffer.size());

    for (auto n = 0; n < draw_data->CmdListsCount; ++n)
    {
        ImDrawList* const cmd_list = draw_data->CmdLists[n];
        for (auto cmd_i = 0; cmd_i < cmd_list->CmdBuffer.Size; ++cmd_i)
        {
            ImDrawCmd& pcmd = cmd_list->CmdBuffer[cmd_i];
            if (pcmd.UserCallback != nullptr)
            {
                if (pcmd.UserCallback == ImDrawCallback_ResetRenderState)
                {
                    LOG_WARN("Imgui reset render state callback not implemented");
                }
                else
                {
                    pcmd.UserCallback(cmd_list, &pcmd);
                }
            }
            else
            {
                phi::cmd::draw dcmd;
                dcmd.init(g_pipeline_state, pcmd.ElemCount, frame_res.vertex_buf, frame_res.index_buf, pcmd.IdxOffset + global_idx_offset,
                          pcmd.VtxOffset + global_vtx_offset);

                auto const shader_view = imgui_to_sv(pcmd.TextureId);
                dcmd.add_shader_arg(frame_res.mvp_buffer, 0, shader_view);

                dcmd.set_scissor(int(pcmd.ClipRect.x - clip_offset.x), int(pcmd.ClipRect.y - clip_offset.y), int(pcmd.ClipRect.z - clip_offset.x),
                                 int(pcmd.ClipRect.w - clip_offset.y));

                writer.add_command(dcmd);
            }
        }

        global_idx_offset += cmd_list->IdxBuffer.Size;
        global_vtx_offset += cmd_list->VtxBuffer.Size;
    }
}

unsigned ImGui_ImplPHI_GetDrawDataCommandSize(const ImDrawData* draw_data)
{
    // Avoid rendering when minimized
    if (draw_data->DisplaySize.x <= 0.0f || draw_data->DisplaySize.y <= 0.0f)
        return 0;

    unsigned num_cmdbuffers = 0;
    for (auto n = 0; n < draw_data->CmdListsCount; ++n)
    {
        num_cmdbuffers += draw_data->CmdLists[n]->CmdBuffer.Size;
    }

    return sizeof(phi::cmd::draw) * num_cmdbuffers; // amount of ImGui "command buffers"
}

//--------------------------------------------------------------------------------------------------------
// MULTI-VIEWPORT / PLATFORM INTERFACE SUPPORT
// This is an _advanced_ and _optional_ feature, allowing the back-end to create and handle multiple viewports simultaneously.
// If you are new to dear imgui or creating a new binding for dear imgui, it is recommended that you completely ignore this section first..
//--------------------------------------------------------------------------------------------------------


static void ImGui_ImplPHI_CreateWindow(ImGuiViewport* viewport)
{
    ImGuiViewportDataPHI* data = IM_NEW(ImGuiViewportDataPHI)();
    viewport->RendererUserData = data;

    // PlatformHandleRaw should always be a HWND, whereas PlatformHandle might be a higher-level handle (e.g. GLFWWindow*, SDL_Window*).
    // Some back-ends will leave PlatformHandleRaw NULL, in which case we assume PlatformHandle will contain the HWND.
    SDL_Window* const sdl_window = static_cast<SDL_Window*>(viewport->PlatformHandle);
    CC_ASSERT(sdl_window != nullptr);

    data->swapchain = g_backend->createSwapchain({sdl_window}, {int(viewport->Size.x), int(viewport->Size.y)});
}

static void ImGui_ImplPHI_DestroyWindow(ImGuiViewport* viewport)
{
    // The main viewport (owned by the application) will always have RendererUserData == NULL since we didn't create the data for it.
    if (ImGuiViewportDataPHI* const data = (ImGuiViewportDataPHI*)viewport->RendererUserData)
    {
        if (data->swapchain.is_valid())
        {
            g_backend->free(data->swapchain);
        }

        for (auto& render_buf : data->frame_render_buffers)
        {
            render_buf.destroy();
        }
        IM_DELETE(data);
    }
    viewport->RendererUserData = nullptr;
}

static void ImGui_ImplPHI_SetWindowSize(ImGuiViewport* viewport, ImVec2 size)
{
    ImGuiViewportDataPHI* const data = (ImGuiViewportDataPHI*)viewport->RendererUserData;
    g_backend->onResize(data->swapchain, {int(size.x), int(size.y)});
}

static void ImGui_ImplPHI_RenderWindow(ImGuiViewport* viewport, void*)
{
    ImGuiViewportDataPHI* const data = (ImGuiViewportDataPHI*)viewport->RendererUserData;

    auto const backbuffer = g_backend->acquireBackbuffer(data->swapchain);
    if (!backbuffer.is_valid())
        return;

    unsigned const drawdata_command_size = ImGui_ImplPHI_GetDrawDataCommandSize(viewport->DrawData);
    cc::array<std::byte> buffer = cc::array<std::byte>::uninitialized(2 * sizeof(phi::cmd::transition_resources) + sizeof(phi::cmd::begin_render_pass)
                                                                      + sizeof(phi::cmd::end_render_pass) + drawdata_command_size
                                                                      + sizeof(phi::cmd::begin_debug_label) + +sizeof(phi::cmd::end_debug_label));

    phi::command_stream_writer writer(buffer.data(), buffer.size_bytes());

    writer.add_command(phi::cmd::begin_debug_label("ImGui_ImplPHI_RenderWindow"));

    auto& tcmd = writer.emplace_command<phi::cmd::transition_resources>();
    tcmd.add(backbuffer, phi::resource_state::render_target);

    auto& bcmd = writer.emplace_command<phi::cmd::begin_render_pass>();
    bcmd.add_backbuffer_rt(backbuffer, true);
    bcmd.set_null_depth_stencil();
    bcmd.viewport.width = int(viewport->Size.x);
    bcmd.viewport.height = int(viewport->Size.y);

    ImGui_ImplPHI_RenderDrawData(viewport->DrawData, {writer.buffer_head(), size_t(writer.remaining_bytes())});
    writer.advance_cursor(drawdata_command_size);

    writer.add_command(phi::cmd::end_render_pass{});

    auto& tcmd2 = writer.emplace_command<phi::cmd::transition_resources>();
    tcmd2.add(backbuffer, phi::resource_state::present);

    writer.add_command(phi::cmd::end_debug_label());

    auto const cmdlist = g_backend->recordCommandList(writer.buffer(), writer.size());
    g_backend->submit(cc::span{cmdlist});
}

static void ImGui_ImplPHI_SwapBuffers(ImGuiViewport* viewport, void*)
{
    ImGuiViewportDataPHI* data = (ImGuiViewportDataPHI*)viewport->RendererUserData;

    g_backend->present(data->swapchain);
}


void ImGui_ImplPHI_InitPlatformInterface()
{
    ImGuiPlatformIO& platform_io = ImGui::GetPlatformIO();
    platform_io.Renderer_CreateWindow = ImGui_ImplPHI_CreateWindow;
    platform_io.Renderer_DestroyWindow = ImGui_ImplPHI_DestroyWindow;
    platform_io.Renderer_SetWindowSize = ImGui_ImplPHI_SetWindowSize;
    platform_io.Renderer_RenderWindow = ImGui_ImplPHI_RenderWindow;
    platform_io.Renderer_SwapBuffers = ImGui_ImplPHI_SwapBuffers;
}

void ImGui_ImplPHI_ShutdownPlatformInterface() { ImGui::DestroyPlatformWindows(); }
