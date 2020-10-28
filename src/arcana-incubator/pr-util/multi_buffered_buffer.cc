#include "multi_buffered_buffer.hh"

#include <phantasm-renderer/Context.hh>

void inc::pre::multi_buffered_buffer::initialize(pr::Context& ctx, unsigned num_backbuffers, const pr::buffer_info& info)
{
    destroy(ctx);
    buffers.emplace(num_backbuffers);
    this->info = info;
    for (auto& buf : buffers)
    {
        buf = ctx.make_buffer(info).disown().res;
    }
}

void inc::pre::multi_buffered_buffer::initialize(pr::Context& ctx, pr::swapchain sc, const pr::buffer_info& info)
{
    initialize(ctx, ctx.get_num_backbuffers(sc), info);
}

void inc::pre::multi_buffered_buffer::destroy(pr::Context& ctx)
{
    for (auto& buf : buffers)
    {
        ctx.free_untyped(buf);
    }
    buffers = {};
}
