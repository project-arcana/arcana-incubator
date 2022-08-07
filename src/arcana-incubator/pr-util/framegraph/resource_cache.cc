#include "resource_cache.hh"

#include <phantasm-renderer/Context.hh>

#include <phantasm-hardware-interface/common/hash.hh>

pr::resource inc::frag::GraphCache::get(pr::generic_resource_info const& info, const char* debug_name)
{
    auto const hash = phi::ComputeHash(info);
    auto const lookup = _cache.acquire(hash);
    if (lookup.handle.is_valid())
        return lookup;

    auto const res = _backend->make_untyped_unlocked(info, debug_name);
    _cache.add_elem(hash, res);
    return res;
}

uint32_t inc::frag::GraphCache::freeAll()
{
    auto const res = (uint32_t)_cache._values.size();
    for (auto const& val : _cache._values)
    {
        _backend->free_deferred(val);
    }

    _cache.reset();
    return res;
}
