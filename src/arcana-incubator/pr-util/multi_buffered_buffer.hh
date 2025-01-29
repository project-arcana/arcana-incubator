#pragma once

#include <clean-core/capped_vector.hh>

#include <phantasm-renderer/resource_types.hh>

namespace inc::pre
{
/// multi-buffered upload/readback buffer to avoid teared writes/reads
/// (easier way to avoid this issue: use cached buffers)
struct multi_buffered_buffer
{
    void initialize(pr::Context& ctx, phi::resource_heap heap, uint32_t size_bytes, uint32_t stride_bytes, char const* debug_name = nullptr, uint32_t num_backbuffers = 3u);

    void initialize_readback(pr::Context& ctx, uint32_t size_bytes, uint32_t stride_bytes, char const* debug_name = nullptr, uint32_t num_backbuffers = 3u)
    {
        initialize(ctx, phi::resource_heap::readback, size_bytes, stride_bytes, debug_name, num_backbuffers);
    }

    void initialize_upload(pr::Context& ctx, uint32_t size_bytes, uint32_t stride_bytes, char const* debug_name = nullptr, uint32_t num_backbuffers = 3u)
    {
        initialize(ctx, phi::resource_heap::upload, size_bytes, stride_bytes, debug_name, num_backbuffers);
    }

    [[deprecated]] void initialize(pr::Context& ctx, unsigned num_backbuffers, pr::buffer_info const& info);

    bool is_valid() const { return buffers.size() > 0; }

    void destroy(pr::Context& ctx);

    multi_buffered_buffer() = default;
    multi_buffered_buffer(multi_buffered_buffer const&) = delete;
    multi_buffered_buffer(multi_buffered_buffer&& rhs) noexcept = default;
    multi_buffered_buffer& operator=(multi_buffered_buffer&& rhs) noexcept = default;

    pr::buffer get(unsigned i) const { return {buffers[i]}; }

    cc::capped_vector<pr::buffer, 5> buffers;
};
} // namespace inc::pre
