#include "multi_buffered_buffer.hh"

#include <phantasm-renderer/Context.hh>

void inc::pre::multi_buffered_buffer::create(pr::Context& ctx, const pr::buffer_info& info)
{
    buffers.emplace(ctx.get_num_backbuffers());
    for (auto& buf : buffers)
    {
        buf = ctx.make_buffer(info);
    }
}
