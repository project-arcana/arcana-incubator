#include "deferred_destruction_queue.hh"

#include <clean-core/utility.hh>

#include <phantasm-hardware-interface/Backend.hh>

#include <phantasm-renderer/Context.hh>

void inc::pre::deferred_destruction_queue::free(pr::Context& ctx, phi::handle::shader_view sv)
{
    auto const epoch_cpu = ctx.get_current_cpu_epoch();
    auto const epoch_gpu = ctx.get_current_gpu_epoch();

    if (epoch_gpu >= gpu_epoch_old)
    {
        // can free old
        if (pending_svs_old.size() > 0)
            ctx.get_backend().freeRange(pending_svs_old);

        cc::swap(pending_svs_old, pending_svs_new);
        pending_svs_new.clear();

        gpu_epoch_old = gpu_epoch_new;
    }

    CC_ASSERT(epoch_cpu >= gpu_epoch_new && ">400 year overflow or programmer error");
    gpu_epoch_new = epoch_cpu;
    pending_svs_new.push_back(sv);
}

void inc::pre::deferred_destruction_queue::destroy(pr::Context& ctx)
{
    ctx.get_backend().freeRange(pending_svs_old);
    ctx.get_backend().freeRange(pending_svs_new);
    pending_svs_old = {};
    pending_svs_new = {};
}
