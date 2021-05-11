#include "resource_cache.hh"

#include <phantasm-renderer/Context.hh>

pr::resource inc::frag::GraphCache::get(const pr::hashable_storage<pr::generic_resource_info>& info, const char* debug_name)
{
    auto const hash = info.get_xxhash();
    auto const lookup = _cache.acquire(hash);
    if (lookup.handle.is_valid())
        return lookup;

    auto const res = _backend->make_untyped_unlocked(info.get(), debug_name);
    _cache.add_elem(hash, res);
    return res;
}

void inc::frag::GraphCache::freeAll()
{
    for (auto const& val : _cache._values)
    {
        _backend->free_untyped(val);
    }

    _cache.reset();
}
