#pragma once

#include <clean-core/typedefs.hh>
#include <clean-core/vector.hh>

#include <phantasm-renderer/common/hashable_storage.hh>
#include <phantasm-renderer/resource_types.hh>

namespace inc::frag
{
// key-value relation 1:1
struct resource_cache
{
public:
    void reserve(size_t num_elems)
    {
        _hashes.reserve(num_elems);
        _values.reserve(num_elems);
    }

    [[nodiscard]] pr::raw_resource acquire(cc::hash_t key)
    {
        for (auto i = 0u; i < _hashes.size(); ++i)
        {
            if (_hashes[i].hash == key && _hashes[i].used_frame < _current_frame)
            {
                _hashes[i].used_frame = _current_frame;
                return _values[i];
            }
        }

        return {phi::handle::null_resource, 0};
    }
    void add_elem(cc::hash_t key, pr::raw_resource val)
    {
        _hashes.push_back({key, _current_frame});
        _values.push_back(val);
    }

    void on_new_frame() { ++_current_frame; }
    void free_all()
    {
        _hashes.clear();
        _values.clear();
    }

public:
    struct map_element
    {
        cc::hash_t hash;
        uint64_t used_frame;
    };

    uint64_t _current_frame = 0;
    cc::vector<map_element> _hashes;
    cc::vector<pr::raw_resource> _values;
};

class GraphCache
{
public:
    GraphCache() = default;
    GraphCache(pr::Context* backend) { init(backend); }
    GraphCache(GraphCache const&) = delete;
    GraphCache(GraphCache&&) noexcept = default;

    void init(pr::Context* backend)
    {
        _backend = backend;
        _cache.reserve(512);
    }

    void onNewFrame() { _cache.on_new_frame(); }

    pr::raw_resource get(pr::hashable_storage<pr::generic_resource_info> const& info);

    void freeAll();

private:
    pr::Context* _backend;
    resource_cache _cache;
};
}
