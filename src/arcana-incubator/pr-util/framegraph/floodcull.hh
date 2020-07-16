#pragma once

#include <clean-core/allocator.hh>
#include <clean-core/function_ptr.hh>
#include <clean-core/span.hh>

namespace inc::frag
{
struct floodcull_relation
{
    unsigned producer_index;
    unsigned resource_index;
};

void run_floodcull(cc::span<int> inout_producer_refcounts,
                   cc::span<int> inout_resource_refcounts,
                   cc::span<floodcull_relation const> writes,
                   cc::span<floodcull_relation const> reads,
                   cc::allocator* alloc = cc::system_allocator);
}
