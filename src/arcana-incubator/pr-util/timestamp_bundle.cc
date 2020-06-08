#include "timestamp_bundle.hh"

#include <phantasm-renderer/Context.hh>
#include <phantasm-renderer/Frame.hh>

void inc::pre::timestamp_bundle::initialize(pr::Context& ctx, unsigned num_timers)
{
    auto const num_backbuffers = ctx.get_num_backbuffers();

    this->num_timings = num_timers;
    this->active_timing = num_timers;

    query_range = ctx.make_query_range(pr::query_type::timestamp, num_timers * 2 * num_backbuffers);

    readback_buffers = readback_buffers.defaulted(num_backbuffers);
    last_timings = cc::array<double>::filled(num_timers, 0.f);
    readback_memory = cc::array<uint64_t>::filled(num_timers * 2, 0);

    unsigned const buffer_size = sizeof(uint64_t) * 2 * num_timers;
    for (auto& buf : readback_buffers)
    {
        buf = ctx.make_readback_buffer(buffer_size);
    }
}

void inc::pre::timestamp_bundle::begin_timing(pr::raii::Frame& frame, unsigned idx)
{
    frame.write_timestamp(query_range, frame_index * num_timings * 2 + idx * 2);
}

void inc::pre::timestamp_bundle::end_timing(pr::raii::Frame& frame, unsigned idx)
{
    frame.write_timestamp(query_range, frame_index * num_timings * 2 + idx * 2 + 1);
}

void inc::pre::timestamp_bundle::finalize_frame(pr::raii::Frame& frame)
{
    // resolve queries from current frame
    frame.resolve_queries(query_range, readback_buffers[frame_index], frame_index * num_timings * 2, num_timings * 2);

    // map and copy the queries from _next_ frame (which was recorded the longest ago)
    frame.context().read_from_buffer(readback_buffers[cc::wrapped_increment(frame_index, unsigned(readback_buffers.size()))], readback_memory);

    for (auto i = 0u; i < num_timings; ++i)
    {
        last_timings[i] = frame.context().get_timestamp_difference_milliseconds(readback_memory[i * 2], readback_memory[i * 2 + 1]);
    }

    frame_index = cc::wrapped_increment(frame_index, unsigned(readback_buffers.size()));
}
