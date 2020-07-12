#include "imgui_impl_pr.hh"

#include <rich-log/log.hh>

#include <clean-core/bit_cast.hh>

#include <phantasm-hardware-interface/Backend.hh>
#include <phantasm-hardware-interface/commands.hh>

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
    ImTextureID res = nullptr;
    std::memset(&res, 0, sizeof(res));
    std::memcpy(&res, &sv, sizeof(sv));
    return res;
}
}

void inc::ImGuiPhantasmImpl::initialize(phi::Backend* backend, std::byte const* ps_data, size_t ps_size, std::byte const* vs_data, size_t vs_size)
{
    using namespace phi;
    mBackend = backend;

    auto const num_frames_in_flight = backend->getNumBackbuffers();
    bool const d3d12_alignment = backend->getBackendType() == phi::backend_type::d3d12;

    ImGuiIO& io = ImGui::GetIO();
    io.BackendRendererName = "ImGuiPhantasmImpl";
    io.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset; // we honor vertex offsets, allowing imgui to use larger vertex buffers

    // PSO
    {
        cc::capped_vector<vertex_attribute_info, 3> vert_attrs;
        vert_attrs.push_back(vertex_attribute_info{"POSITION", static_cast<unsigned>(IM_OFFSETOF(ImDrawVert, pos)), format::rg32f});
        vert_attrs.push_back(vertex_attribute_info{"TEXCOORD", static_cast<unsigned>(IM_OFFSETOF(ImDrawVert, uv)), format::rg32f});
        vert_attrs.push_back(vertex_attribute_info{"COLOR", static_cast<unsigned>(IM_OFFSETOF(ImDrawVert, col)), format::rgba8un});

        arg::vertex_format vert_format = {vert_attrs, sizeof(ImDrawVert)};

        arg::framebuffer_config fb_format;
        fb_format.add_render_target(mBackend->getBackbufferFormat());
        {
            auto& rt = fb_format.render_targets.back();
            rt.blend_enable = true;
            rt.state.blend_color_src = blend_factor::src_alpha;
            rt.state.blend_color_dest = blend_factor::inv_src_alpha;
            rt.state.blend_op_color = blend_op::op_add;
            rt.state.blend_alpha_src = blend_factor::inv_src_alpha;
            rt.state.blend_alpha_dest = blend_factor::zero;
            rt.state.blend_op_alpha = blend_op::op_add;
        }

        arg::shader_arg_shape shader_shape;
        shader_shape.has_cbv = true;
        shader_shape.num_srvs = 1;
        shader_shape.num_samplers = 1;

        arg::graphics_shader shader_stages[]
            = {arg::graphics_shader{{vs_data, vs_size}, shader_stage::vertex}, arg::graphics_shader{{ps_data, ps_size}, shader_stage::pixel}};

        phi::pipeline_config config;
        config.depth = phi::depth_function::none;
        config.cull = phi::cull_mode::none;

        mGlobalResources.pso = mBackend->createPipelineState(vert_format, fb_format, cc::span{shader_shape}, false, shader_stages, config);
    }

    // Font tex
    phi::handle::resource temp_upload_buffer;
    {
        unsigned char* pixels;
        int width, height;

        io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
        mGlobalResources.font_tex = mBackend->createTexture(format::rgba8un, {width, height}, 1);

        resource_view tex_sve;
        tex_sve.init_as_tex2d(mGlobalResources.font_tex, format::rgba8un);

        sampler_config sampler;
        sampler.init_default(sampler_filter::min_mag_mip_linear);

        mGlobalResources.font_tex_sv = mBackend->createShaderView(cc::span{tex_sve}, {}, cc::span{sampler});

        temp_upload_buffer = mBackend->createBuffer(
            inc::get_mipmap_upload_size(format::rgba8un, assets::image_size{unsigned(width), unsigned(height), 1, 1}, true), 0, resource_heap::upload, false);


        {
            mCmdWriter.reset();
            cmd::transition_resources tcmd;
            tcmd.add(mGlobalResources.font_tex, resource_state::copy_dest);
            mCmdWriter.add_command(tcmd);

            mCmdWriter.accomodate_t<cmd::copy_buffer_to_texture>();
            inc::copy_data_to_texture(mCmdWriter.raw_writer(), temp_upload_buffer, mBackend->mapBuffer(temp_upload_buffer), mGlobalResources.font_tex,
                                      format::rgba8un, width, height, reinterpret_cast<std::byte const*>(pixels), d3d12_alignment);

            cmd::transition_resources tcmd2;
            tcmd2.add(mGlobalResources.font_tex, resource_state::shader_resource, shader_stage::pixel);
            mCmdWriter.add_command(tcmd2);

            auto const cmdl = mBackend->recordCommandList(mCmdWriter.buffer(), mCmdWriter.size());
            mBackend->unmapBuffer(temp_upload_buffer);
            mBackend->submit(cc::span{cmdl});
        }

        io.Fonts->TexID = sv_to_imgui(mGlobalResources.font_tex_sv);
    }

    mGlobalResources.const_buffer = mBackend->createUploadBuffer(sizeof(float[4][4]));

    // per frame res
    CC_RUNTIME_ASSERT(num_frames_in_flight <= mPerFrameResources.max_size() && "too many in-flight frames");
    mPerFrameResources.emplace(num_frames_in_flight);

    mBackend->flushGPU();
    mBackend->free(temp_upload_buffer);
}

void inc::ImGuiPhantasmImpl::initialize_with_contained_shaders(phi::Backend* backend)
{
    constexpr char const* imgui_code
        = R"(cbuffer _0:register(b0,space0){float4x4 prj;};struct vit{float2 pos:POSITION;float2 uv:TEXCOORD0;float4 col:COLOR0;};struct pit{float4 pos:SV_POSITION;float4 col:COLOR0;float2 uv:TEXCOORD0;};struct vgt{float4x4 prj;};ConstantBuffer<vgt> s:register(b0,space0);SamplerState p:register(s0,space0);Texture2D v:register(t0,space0);pit r(vit p){pit v;v.pos=mul(s.prj,float4(p.pos.xy,0.f,1.f));v.col=p.col;v.uv=p.uv;return v;}float4 u(pit s):SV_TARGET{float4 u=s.col*v.Sample(p,s.uv);return u;})";

    auto const output = backend->getBackendType() == phi::backend_type::d3d12 ? dxcw::output::dxil : dxcw::output::spirv;

    dxcw::compiler comp;
    comp.initialize();
    auto const vs = comp.compile_binary(imgui_code, "r", dxcw::target::vertex, output);
    auto const ps = comp.compile_binary(imgui_code, "u", dxcw::target::pixel, output);
    comp.destroy();
    initialize(backend, ps.data, ps.size, vs.data, vs.size);
    dxcw::destroy_blob(vs.internal_blob);
    dxcw::destroy_blob(ps.internal_blob);
}

void inc::ImGuiPhantasmImpl::destroy()
{
    mBackend->free(mGlobalResources.pso);
    mBackend->free(mGlobalResources.font_tex);
    mBackend->free(mGlobalResources.font_tex_sv);
    mBackend->free(mGlobalResources.const_buffer);

    for (auto const& res : mPerFrameResources)
    {
        mBackend->free(res.index_buf);
        mBackend->free(res.vertex_buf);
    }
}

size_t inc::ImGuiPhantasmImpl::get_command_size(const ImDrawData* draw_data) const
{
    unsigned num_cmdbuffers = 0;
    for (auto n = 0; n < draw_data->CmdListsCount; ++n)
    {
        num_cmdbuffers += draw_data->CmdLists[n]->CmdBuffer.Size;
    }

    return sizeof(phi::cmd::begin_render_pass)       // begin
           + sizeof(phi::cmd::draw) * num_cmdbuffers // amount of ImGui "command buffers"
           + sizeof(phi::cmd::end_render_pass);      // end
}

void inc::ImGuiPhantasmImpl::write_commands(const ImDrawData* draw_data, phi::handle::resource backbuffer, std::byte* out_buffer, size_t out_buffer_size)
{
    phi::command_stream_writer writer(out_buffer, out_buffer_size);

    ++mCurrentFrame;
    if (mCurrentFrame >= mPerFrameResources.size())
        mCurrentFrame = 0;

    auto& frame_res = mPerFrameResources[mCurrentFrame];

    if (!frame_res.index_buf.is_valid() || frame_res.index_buf_size < draw_data->TotalIdxCount)
    {
        if (frame_res.index_buf.is_valid())
            mBackend->free(frame_res.index_buf);

        frame_res.index_buf_size = draw_data->TotalIdxCount + 10000;
        frame_res.index_buf = mBackend->createUploadBuffer(frame_res.index_buf_size * sizeof(ImDrawIdx), sizeof(ImDrawIdx));
    }

    if (!frame_res.vertex_buf.is_valid() || frame_res.vertex_buf_size < draw_data->TotalVtxCount)
    {
        if (frame_res.vertex_buf.is_valid())
            mBackend->free(frame_res.vertex_buf);

        frame_res.vertex_buf_size = draw_data->TotalVtxCount + 5000;
        frame_res.vertex_buf = mBackend->createUploadBuffer(frame_res.vertex_buf_size * sizeof(ImDrawVert), sizeof(ImDrawVert));
    }

    // upload vertices and indices
    {
        auto* const vertex_map = mBackend->mapBuffer(frame_res.vertex_buf);
        auto* const index_map = mBackend->mapBuffer(frame_res.index_buf);

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

        mBackend->unmapBuffer(frame_res.vertex_buf);
        mBackend->unmapBuffer(frame_res.index_buf);
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

        auto* const const_buffer_map = mBackend->mapBuffer(mGlobalResources.const_buffer);
        std::memcpy(const_buffer_map, mvp, sizeof(mvp));
        mBackend->unmapBuffer(mGlobalResources.const_buffer);
    }

    auto global_vtx_offset = 0;
    auto global_idx_offset = 0;
    ImVec2 const clip_offset = draw_data->DisplayPos;

    phi::cmd::begin_render_pass bcmd;
    bcmd.add_backbuffer_rt(backbuffer, false);
    bcmd.set_null_depth_stencil();
    bcmd.viewport.width = (int)draw_data->DisplaySize.x;
    bcmd.viewport.height = (int)draw_data->DisplaySize.y;
    writer.add_command(bcmd);

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
                dcmd.init(mGlobalResources.pso, pcmd.ElemCount, frame_res.vertex_buf, frame_res.index_buf, pcmd.IdxOffset + global_idx_offset,
                          pcmd.VtxOffset + global_vtx_offset);

                auto const shader_view = imgui_to_sv(pcmd.TextureId);
                dcmd.add_shader_arg(mGlobalResources.const_buffer, 0, shader_view);

                dcmd.set_scissor(int(pcmd.ClipRect.x - clip_offset.x), int(pcmd.ClipRect.y - clip_offset.y), int(pcmd.ClipRect.z - clip_offset.x),
                                 int(pcmd.ClipRect.w - clip_offset.y));

                writer.add_command(dcmd);
            }
        }

        global_idx_offset += cmd_list->IdxBuffer.Size;
        global_vtx_offset += cmd_list->VtxBuffer.Size;
    }

    writer.add_command(phi::cmd::end_render_pass{});
}
