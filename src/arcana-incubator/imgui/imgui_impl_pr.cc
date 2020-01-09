#include "imgui_impl_pr.hh"

#include <rich-log/log.hh>

#include <phantasm-renderer/backend/Backend.hh>
#include <phantasm-renderer/backend/command_stream.hh>
#include <phantasm-renderer/primitive_pipeline_config.hh>

namespace
{
[[nodiscard]] pr::backend::handle::shader_view imgui_to_sv(ImTextureID itd)
{
    pr::backend::handle::shader_view res;
    std::memcpy(&res, &itd, sizeof(res));
    return res;
}

[[nodiscard]] ImTextureID sv_to_imgui(pr::backend::handle::shader_view sv)
{
    ImTextureID res = nullptr;
    std::memset(&res, 0, sizeof(res));
    std::memcpy(&res, &sv, sizeof(sv));
    return res;
}
}

void inc::ImGuiPhantasmImpl::init(pr::backend::Backend* backend, unsigned num_frames_in_flight, std::byte* ps_src, size_t ps_size, std::byte* vs_src, size_t vs_size)
{
    mBackend = backend;

    ImGuiIO& io = ImGui::GetIO();
    io.BackendRendererName = "ImGuiPhantasmImpl";
    io.BackendFlags |= ImGuiBackendFlags_RendererHasVtxOffset;

    // PSO
    {
        cc::capped_vector<pr::backend::vertex_attribute_info, 3> vert_attrs;
        vert_attrs.push_back(pr::backend::vertex_attribute_info{"POSITION", static_cast<unsigned>(IM_OFFSETOF(ImDrawVert, pos)), pr::backend::format::rg32f});
        vert_attrs.push_back(pr::backend::vertex_attribute_info{"TEXCOORD", static_cast<unsigned>(IM_OFFSETOF(ImDrawVert, uv)), pr::backend::format::rg32f});
        vert_attrs.push_back(pr::backend::vertex_attribute_info{"COLOR", static_cast<unsigned>(IM_OFFSETOF(ImDrawVert, col)), pr::backend::format::rgba8un});

        pr::backend::arg::vertex_format vert_format = {vert_attrs, sizeof(ImDrawVert)};

        pr::backend::format const backbuffer_format = mBackend->getBackbufferFormat();
        pr::backend::arg::framebuffer_format fb_format = {cc::span{backbuffer_format}, {}};

        pr::backend::arg::shader_argument_shape shader_shape;
        shader_shape.has_cb = true;
        shader_shape.num_srvs = 1;
        shader_shape.num_samplers = 1;

        cc::capped_vector<pr::backend::arg::shader_stage, 2> shader_stages;
        shader_stages.push_back(pr::backend::arg::shader_stage{vs_src, vs_size, pr::backend::shader_domain::vertex});
        shader_stages.push_back(pr::backend::arg::shader_stage{ps_src, ps_size, pr::backend::shader_domain::pixel});

        pr::primitive_pipeline_config config;
        config.depth = pr::depth_function::none;
        config.cull = pr::cull_mode::none;

        mGlobalResources.pso = mBackend->createPipelineState(vert_format, fb_format, cc::span{shader_shape}, false, shader_stages, config);
    }

    // Font tex
    {
        unsigned char* pixels;
        int width, height;

        io.Fonts->GetTexDataAsRGBA32(&pixels, &width, &height);
        mGlobalResources.font_tex = mBackend->createTexture(pr::backend::format::rgba8un, width, height, 1);

        pr::backend::shader_view_element tex_sve;
        tex_sve.init_as_tex2d(mGlobalResources.font_tex, pr::backend::format::rgba8un);

        pr::backend::sampler_config sampler;
        sampler.init_default(pr::backend::sampler_filter::min_mag_mip_linear);

        mGlobalResources.font_tex_sv = mBackend->createShaderView(cc::span{tex_sve}, {}, cc::span{sampler});

        auto const upbuff = mBackend->createMappedBuffer(width * height);
        auto* const upbuff_map = mBackend->getMappedMemory(upbuff);
        std::memcpy(upbuff_map, pixels, width * height);

        mBackend->flushMappedMemory(upbuff);

        {
            mCmdWriter.reset();
            pr::backend::cmd::transition_resources tcmd;
            tcmd.add(mGlobalResources.font_tex, pr::backend::resource_state::copy_dest);
            tcmd.add(upbuff, pr::backend::resource_state::copy_src);
            mCmdWriter.add_command(tcmd);

            pr::backend::cmd::copy_buffer_to_texture ccmd;
            ccmd.init(upbuff, mGlobalResources.font_tex, width, height);
            mCmdWriter.add_command(ccmd);

            pr::backend::cmd::transition_resources tcmd2;
            tcmd2.add(mGlobalResources.font_tex, pr::backend::resource_state::shader_resource, pr::backend::shader_domain_bits::pixel);
            mCmdWriter.add_command(tcmd2);

            auto const cmdl = mBackend->recordCommandList(mCmdWriter.buffer(), mCmdWriter.size());
            mBackend->submit(cc::span{cmdl});
        }
        mBackend->flushGPU();
        mBackend->free(upbuff);

        io.Fonts->TexID = sv_to_imgui(mGlobalResources.font_tex_sv);
    }

    mGlobalResources.const_buffer = mBackend->createMappedBuffer(sizeof(float[4][4]));
    mGlobalResources.const_buffer_map = mBackend->getMappedMemory(mGlobalResources.const_buffer);

    // per frame res
    CC_RUNTIME_ASSERT(num_frames_in_flight <= mPerFrameResources.max_size() && "too many in-flight frames");
    mPerFrameResources.emplace(num_frames_in_flight);

    mBackend->flushGPU();
}

void inc::ImGuiPhantasmImpl::shutdown()
{
    mBackend->free(mGlobalResources.pso);
    mBackend->free(mGlobalResources.font_tex);
    mBackend->free(mGlobalResources.font_tex_sv);

    for (auto const& res : mPerFrameResources)
    {
        mBackend->free(res.index_buf);
        mBackend->free(res.vertex_buf);
    }
}

void inc::ImGuiPhantasmImpl::render(ImDrawData* draw_data, pr::backend::handle::resource backbuffer)
{
    ++mCurrentFrame;
    if (mCurrentFrame >= mPerFrameResources.size())
        mCurrentFrame = 0;

    auto& frame_res = mPerFrameResources[mCurrentFrame];

    if (!frame_res.index_buf.is_valid() || frame_res.index_buf_size < draw_data->TotalIdxCount)
    {
        if (frame_res.index_buf.is_valid())
            mBackend->free(frame_res.index_buf);

        frame_res.index_buf_size = draw_data->TotalIdxCount + 10000;

        frame_res.index_buf = mBackend->createMappedBuffer(frame_res.index_buf_size * sizeof(ImDrawIdx), sizeof(ImDrawIdx));
    }

    if (!frame_res.vertex_buf.is_valid() || frame_res.vertex_buf_size < draw_data->TotalVtxCount)
    {
        if (frame_res.vertex_buf.is_valid())
            mBackend->free(frame_res.vertex_buf);

        frame_res.vertex_buf_size = draw_data->TotalVtxCount + 5000;

        frame_res.vertex_buf = mBackend->createMappedBuffer(frame_res.vertex_buf_size * sizeof(ImDrawVert), sizeof(ImDrawVert));
    }

    // upload vertices and indices
    {
        auto* const vertex_map = mBackend->getMappedMemory(frame_res.vertex_buf);
        auto* const index_map = mBackend->getMappedMemory(frame_res.index_buf);

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

        mBackend->flushMappedMemory(frame_res.vertex_buf);
        mBackend->flushMappedMemory(frame_res.index_buf);
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
        std::memcpy(mGlobalResources.const_buffer_map, mvp, sizeof(mvp));

        mBackend->flushMappedMemory(mGlobalResources.const_buffer);
    }

    auto global_vtx_offset = 0;
    auto global_idx_offset = 0;

    mCmdWriter.reset();

    pr::backend::cmd::begin_render_pass bcmd;
    bcmd.add_backbuffer_rt(backbuffer);
    bcmd.set_null_depth_stencil();
    bcmd.viewport.width = (int)draw_data->DisplaySize.x;
    bcmd.viewport.height = (int)draw_data->DisplaySize.y;
    mCmdWriter.add_command(bcmd);

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
                    LOG(warning)("Imgui reset render state callback not implemented");
                }
                else
                {
                    pcmd.UserCallback(cmd_list, &pcmd);
                }
            }
            else
            {
                pr::backend::cmd::draw dcmd;
                dcmd.init(mGlobalResources.pso, pcmd.ElemCount, frame_res.vertex_buf, frame_res.index_buf, pcmd.IdxOffset + global_idx_offset,
                          pcmd.VtxOffset + global_vtx_offset);
                dcmd.add_shader_arg(mGlobalResources.const_buffer, 0, imgui_to_sv(pcmd.TextureId));
                mCmdWriter.add_command(dcmd);
            }
        }

        global_idx_offset += cmd_list->IdxBuffer.Size;
        global_vtx_offset += cmd_list->VtxBuffer.Size;
    }

    pr::backend::cmd::end_render_pass ecmd;
    mCmdWriter.add_command(ecmd);

    auto const cmdlist = mBackend->recordCommandList(mCmdWriter.buffer(), mCmdWriter.size());
    mBackend->submit(cc::span{cmdlist});
    mCmdWriter.reset();
}
