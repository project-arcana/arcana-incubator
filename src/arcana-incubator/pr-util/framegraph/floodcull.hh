#pragma once

#include <stdint.h>

#include <clean-core/fwd.hh>
#include <clean-core/span.hh>

namespace inc::frag
{
struct floodcull_relation
{
    uint32_t producer_index;
    uint32_t resource_index;
};

// run a floodcull on a graph of producers which read and write resources
// determines unused producers and resources (will have refcount 0)
// initialize all refcounts as 0, or 1 for producers/resources which are not to be culled ("root")
void run_floodcull(cc::span<int32_t> inout_producer_refcounts,
                   cc::span<int32_t> inout_resource_refcounts,
                   cc::span<floodcull_relation const> writes,
                   cc::span<floodcull_relation const> reads,
                   cc::allocator* alloc = cc::system_allocator);

} // namespace inc::frag
