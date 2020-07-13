#pragma once

#include "lib/imgui.h"

#include <clean-core/capped_array.hh>

#include <phantasm-hardware-interface/fwd.hh>
#include <phantasm-hardware-interface/types.hh>

#include <arcana-incubator/phi-util/growing_writer.hh>


namespace inc
{
class ImGuiPhantasmImpl
{
public:
    ImGuiPhantasmImpl() : mCmdWriter(1024 * 10) {}

    void initialize(phi::Backend* backend, phi::handle::swapchain main_swapchain, const std::byte* ps_data, size_t ps_size, std::byte const* vs_data, size_t vs_size);

    /// initializes using live-compiled included shaders
    void initialize_with_contained_shaders(phi::Backend* backend, phi::handle::swapchain main_swapchain);

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
    } mGlobalResources;

    growing_writer mCmdWriter;

    cc::capped_array<per_frame_resources, 10> mPerFrameResources;
    unsigned mCurrentFrame = 0;
};

}
