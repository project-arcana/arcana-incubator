#pragma once

#include <clean-core/alloc_vector.hh>

#include <phantasm-hardware-interface/handles.hh>

#include <phantasm-renderer/fwd.hh>

namespace inc::pre
{
/// persistent queue of (PHI) resources pending destruction
/// keeps track of GPU epochs and automatically frees older resources when enqueueing
struct deferred_destruction_queue
{
    void free(pr::Context& ctx, phi::handle::shader_view sv);

    void initialize(cc::allocator* alloc, unsigned num_reserved_svs = 128)
    {
        pending_svs_old.reset_reserve(alloc, num_reserved_svs);
        pending_svs_new.reset_reserve(alloc, num_reserved_svs);
    }
    void destroy(pr::Context& ctx);

    pr::gpu_epoch_t gpu_epoch_old = 0;
    pr::gpu_epoch_t gpu_epoch_new = 0;

    cc::alloc_vector<phi::handle::shader_view> pending_svs_old;
    cc::alloc_vector<phi::handle::shader_view> pending_svs_new;
};
}
