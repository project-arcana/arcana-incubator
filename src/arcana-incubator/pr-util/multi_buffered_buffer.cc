#include "multi_buffered_buffer.hh"

#include <phantasm-renderer/Context.hh>

void inc::pre::multi_buffered_buffer::initialize(pr::Context& ctx, phi::resource_heap heap, uint32_t size_bytes, uint32_t stride_bytes, char const* debug_name, uint32_t num_backbuffers)
{
    destroy(ctx);
    buffers.emplace(num_backbuffers);

    this->info = pr::buffer_info::create(size_bytes, stride_bytes, heap, false);

    char namebuf[512];
    for (auto i = 0u; i < buffers.size(); ++i)
    {
        std::snprintf(namebuf, sizeof(namebuf), "%s [multi %u/%u]", debug_name, i, num_backbuffers);
        buffers[i] = ctx.make_buffer(info, debug_name).disown().res;
    }
}

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

void inc::pre::multi_buffered_buffer::destroy(pr::Context& ctx)
{
    for (auto& buf : buffers)
    {
        ctx.free_untyped(buf);
    }
    buffers = {};
}
