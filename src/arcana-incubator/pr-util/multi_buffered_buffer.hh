#pragma once

#include <clean-core/capped_array.hh>

#include <phantasm-renderer/resource_types.hh>

namespace inc::pre
{
/// multi-buffered upload/readback buffer to avoid teared writes/reads
/// easy way to avoid this issue: use cached buffers
struct multi_buffered_buffer
{
    multi_buffered_buffer() = default;
    multi_buffered_buffer(pr::Context& ctx, pr::swapchain sc, unsigned size, unsigned stride = 0, pr::resource_heap heap = pr::resource_heap::upload, bool allow_uav = false)
    {
        create(ctx, sc, pr::buffer_info::create(size, stride, heap, allow_uav));
    }

    void create(pr::Context& ctx, pr::swapchain sc, pr::buffer_info const& info);

    cc::capped_array<pr::auto_buffer, 5> buffers;
    unsigned head_index = 0;
};
}
