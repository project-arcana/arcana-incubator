#include "timestamp_bundle.hh"

#include <algorithm>

#include <phantasm-renderer/Context.hh>
#include <phantasm-renderer/Frame.hh>


void inc::pre::timing_metric::init(unsigned ring_size)
{
    cpu_times = cpu_times.filled(ring_size, 0.f);
    gpu_times = gpu_times.filled(ring_size, 0.f);
}

void inc::pre::timing_metric::on_frame(float cpu_time, float gpu_time)
{
    cpu_times[index] = cpu_time;
    gpu_times[index] = gpu_time;
    index = cc::wrapped_increment(index, unsigned(cpu_times.size()));

    max_cpu = *std::max_element(cpu_times.begin(), cpu_times.end());
    min_cpu = *std::min_element(cpu_times.begin(), cpu_times.end());

    max_gpu = *std::max_element(gpu_times.begin(), gpu_times.end());
    min_gpu = *std::min_element(gpu_times.begin(), gpu_times.end());
}


void inc::pre::timestamp_bundle::initialize(pr::Context& ctx, unsigned num_timers, unsigned num_backbuffers)
{
    this->num_timings = num_timers;
    this->active_timing = num_timers;

    query_range = ctx.make_query_range(pr::query_type::timestamp, num_timers * 2 * num_backbuffers);

    readback_buffers = readback_buffers.defaulted(num_backbuffers);
    last_timings = cc::array<double>::filled(num_timers, 0.f);
    readback_memory = cc::array<uint64_t>::filled(num_timers * 2, 0);
    timing_usage_flags = cc::array<bool>::uninitialized(num_timers);
    std::memset(timing_usage_flags.data(), 0, timing_usage_flags.size_bytes());

    unsigned const buffer_size = sizeof(uint64_t) * 2 * num_timers;
    for (auto& buf : readback_buffers)
    {
        buf = ctx.make_readback_buffer(buffer_size);
    }
}

void inc::pre::timestamp_bundle::begin_timing(pr::raii::Frame& frame, unsigned idx)
{
    CC_CONTRACT(idx < num_timings);
    frame.write_timestamp(query_range, frame_index * num_timings * 2 + idx * 2);
    timing_usage_flags[idx] = true;
}

void inc::pre::timestamp_bundle::end_timing(pr::raii::Frame& frame, unsigned idx) const
{
    CC_CONTRACT(idx < num_timings);
    frame.write_timestamp(query_range, frame_index * num_timings * 2 + idx * 2 + 1);
}

void inc::pre::timestamp_bundle::finalize_frame(pr::raii::Frame& frame)
{
    // resolve queries from current frame
    auto const base_index = frame_index * num_timings * 2;
    pr::buffer const& readback_target = readback_buffers[frame_index];

    unsigned num_contiguous_used = 0;
    for (auto i = 0u; i < num_timings; ++i)
    {
        if (timing_usage_flags[i] == false)
        {
            if (num_contiguous_used > 0)
            {
                auto const first_query = i - num_contiguous_used;
                frame.resolve_queries(query_range, readback_target, base_index + 2 * first_query, num_contiguous_used * 2, first_query * 2 * sizeof(uint64_t));
            }

            num_contiguous_used = 0;
        }
        else
        {
            ++num_contiguous_used;
        }
    }

    if (num_contiguous_used > 0)
    {
        auto const first_query = num_timings - num_contiguous_used;
        frame.resolve_queries(query_range, readback_target, base_index + 2 * first_query, num_contiguous_used * 2, first_query * 2 * sizeof(uint64_t));
    }


    // map and copy the queries from _next_ frame (which was recorded the longest ago)
    frame.context().read_from_buffer(readback_buffers[cc::wrapped_increment(frame_index, unsigned(readback_buffers.size()))], readback_memory);

    for (auto i = 0u; i < num_timings; ++i)
    {
        last_timings[i] = frame.context().get_timestamp_difference_milliseconds(readback_memory[i * 2], readback_memory[i * 2 + 1]);
    }

    frame_index = cc::wrapped_increment(frame_index, unsigned(readback_buffers.size()));
    std::memset(timing_usage_flags.data(), 0, timing_usage_flags.size_bytes());
}
