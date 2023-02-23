#include "multi_buffered_buffer.hh"

#include <stdio.h>

#include <phantasm-renderer/Context.hh>

void inc::pre::multi_buffered_buffer::initialize(pr::Context& ctx, phi::resource_heap heap, uint32_t size_bytes, uint32_t stride_bytes, char const* debug_name, uint32_t num_backbuffers)
{
    destroy(ctx);
    buffers.resize(num_backbuffers);

    auto const info = pr::buffer_info::create(size_bytes, stride_bytes, heap, false);

    char namebuf[512];
    for (auto i = 0u; i < num_backbuffers; ++i)
    {
        snprintf(namebuf, sizeof(namebuf), "%s [multi %u/%u]", debug_name, i + 1, num_backbuffers);
        buffers[i] = ctx.make_buffer(info, debug_name).disown();
    }
}

void inc::pre::multi_buffered_buffer::initialize(pr::Context& ctx, unsigned num_backbuffers, const pr::buffer_info& info)
{
    destroy(ctx);
    buffers.resize(num_backbuffers);

    for (auto& buf : buffers)
    {
        buf = ctx.make_buffer(info).disown();
    }
}

void inc::pre::multi_buffered_buffer::destroy(pr::Context& ctx)
{
    for (auto& buf : buffers)
    {
        ctx.free_deferred(buf);
    }

    buffers.clear();
}
