#pragma once

#include <clean-core/capped_array.hh>

#include <phantasm-hardware-interface/fwd.hh>
#include <phantasm-hardware-interface/types.hh>

#include <arcana-incubator/phi-util/growing_writer.hh>

#include "lib/imgui.hh"

namespace inc
{
class ImGuiPhantasmImpl
{
public:
    ImGuiPhantasmImpl() : mCmdWriter(1024 * 10) {}

    void init(phi::Backend* backend, unsigned num_frames_in_flight, std::byte* ps_src, size_t ps_size, std::byte* vs_src, size_t vs_size, bool d3d12_alignment);

    void shutdown();

    [[nodiscard]] phi::handle::command_list render(ImDrawData* draw_data, phi::handle::resource backbuffer, bool transition_to_present = true);

private:
    phi::Backend* mBackend;

    struct per_frame_resources
    {
        phi::handle::resource index_buf = phi::handle::null_resource;
        int index_buf_size = 0; ///< size of the index buffer, in number of indices
        phi::handle::resource vertex_buf = phi::handle::null_resource;
        int vertex_buf_size = 0; ///< size of the vertex buffer, in number of vertices
    };

    struct global_resources
    {
        phi::handle::pipeline_state pso;

        phi::handle::resource font_tex;
        phi::handle::shader_view font_tex_sv;

        phi::handle::resource const_buffer;
        std::byte* const_buffer_map;
    } mGlobalResources;

    growing_writer mCmdWriter;

    cc::capped_array<per_frame_resources, 10> mPerFrameResources;
    unsigned mCurrentFrame = 0;
};

}
