#include "multi_buffered_buffer.hh"

#include <phantasm-renderer/Context.hh>

void inc::pre::multi_buffered_buffer::initialize(pr::Context& ctx, pr::swapchain sc, const pr::buffer_info& info)
{
    buffers.emplace(ctx.get_num_backbuffers(sc));
    for (auto& buf : buffers)
    {
        buf = ctx.make_buffer(info);
    }
}
