#pragma once

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

    [[nodiscard]] pr::resource acquire(uint64_t key)
    {
        for (auto i = 0u; i < _hashes.size(); ++i)
        {
            if (_hashes[i].hash == key && _hashes[i].used_frame < _current_frame)
            {
                _hashes[i].used_frame = _current_frame;
                return _values[i];
            }
        }

        return {phi::handle::null_resource};
    }
    void add_elem(uint64_t key, pr::resource val)
    {
        _hashes.push_back({key, _current_frame});
        _values.push_back(val);
    }

    void on_new_frame() { ++_current_frame; }
    void reset()
    {
        _hashes.clear();
        _values.clear();
    }

public:
    struct map_element
    {
        uint64_t hash;
        uint64_t used_frame;
    };

    uint64_t _current_frame = 0;
    cc::vector<map_element> _hashes;
    cc::vector<pr::resource> _values;
};

class GraphCache
{
public:
    GraphCache() = default;
    GraphCache(pr::Context* backend) { initialize(backend); }
    GraphCache(GraphCache const&) = delete;
    GraphCache(GraphCache&&) noexcept = default;

    ~GraphCache() { destroy(); }

    void initialize(pr::Context* backend)
    {
        CC_ASSERT(_backend == nullptr && "double init");
        _backend = backend;
        _cache.reserve(512);
    }

    void destroy()
    {
        if (_backend != nullptr)
        {
            freeAll();
            _backend = nullptr;
        }
    }

    void onNewFrame() { _cache.on_new_frame(); }

    pr::resource get(pr::generic_resource_info const& info, char const* debug_name);

    uint32_t freeAll();

private:
    pr::Context* _backend = nullptr;
    resource_cache _cache;
};
} // namespace inc::frag
