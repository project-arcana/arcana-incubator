#pragma once

#include <clean-core/capped_array.hh>

#include <phantasm-renderer/backend/fwd.hh>
#include <phantasm-renderer/backend/types.hh>

#include <arcana-incubator/pr-util/growing_writer.hh>

#include "lib/imgui.hh"

namespace inc
{
class ImGuiPhantasmImpl
{
public:
    ImGuiPhantasmImpl() : mCmdWriter(1024 * 10) {}

    void init(pr::backend::Backend* backend, unsigned num_frames_in_flight, std::byte* ps_src, size_t ps_size, std::byte* vs_src, size_t vs_size, bool d3d12_alignment);

    void shutdown();

    [[nodiscard]] pr::backend::handle::command_list render(ImDrawData* draw_data, pr::backend::handle::resource backbuffer, bool transition_to_present = true);

private:
    pr::backend::Backend* mBackend;

    struct per_frame_resources
    {
        pr::backend::handle::resource index_buf = pr::backend::handle::null_resource;
        int index_buf_size = 0; ///< size of the index buffer, in number of indices
        pr::backend::handle::resource vertex_buf = pr::backend::handle::null_resource;
        int vertex_buf_size = 0; ///< size of the vertex buffer, in number of vertices
    };

    struct global_resources
    {
        pr::backend::handle::pipeline_state pso;

        pr::backend::handle::resource font_tex;
        pr::backend::handle::shader_view font_tex_sv;

        pr::backend::handle::resource const_buffer;
        std::byte* const_buffer_map;
    } mGlobalResources;

    growing_writer mCmdWriter;

    cc::capped_array<per_frame_resources, 10> mPerFrameResources;
    unsigned mCurrentFrame = 0;
};

}
