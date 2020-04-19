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

    void initialize(phi::Backend* backend, std::byte* ps_src, size_t ps_size, std::byte* vs_src, size_t vs_size);

    void destroy();

    /// returns the amount of bytes required to write the phi commands for a specified ImDrawData
    size_t get_command_size(ImDrawData const* draw_data) const;

    /// writes commands to draw the specified ImDrawData to memory
    /// the given buffer must be >= get_command_size(draw_data);
    void write_commands(ImDrawData const* draw_data, phi::handle::resource backbuffer, std::byte* out_buffer, size_t out_buffer_size);

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
