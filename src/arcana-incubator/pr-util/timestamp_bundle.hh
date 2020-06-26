#pragma once

#include <clean-core/array.hh>
#include <clean-core/capped_array.hh>
#include <clean-core/defer.hh>

#include <phantasm-renderer/resource_types.hh>

namespace inc::pre
{
struct timing_metric
{
    void init(unsigned ring_size);

    void on_frame(float cpu_time, float gpu_time);

    cc::array<float> cpu_times;
    cc::array<float> gpu_times;
    float min_gpu = 0.f;
    float max_gpu = 0.f;
    float min_cpu = 0.f;
    float max_cpu = 0.f;
    unsigned index = 0;
};

struct timestamp_bundle
{
    void initialize(pr::Context& ctx, unsigned num_timers);

    void begin_timing(pr::raii::Frame& frame, unsigned idx);
    void end_timing(pr::raii::Frame& frame, unsigned idx);

    auto scoped_timing(pr::raii::Frame& frame, unsigned idx)
    {
        begin_timing(frame, idx);
        auto* const frame_ptr = &frame;
        CC_RETURN_DEFER { end_timing(*frame_ptr, idx); };
    }

    // once per application frame
    void finalize_frame(pr::raii::Frame& frame);

    double get_last_timing(unsigned idx) const { return last_timings[idx]; }

    unsigned frame_index = 0;
    unsigned num_timings = 0;
    unsigned active_timing = 0;
    pr::auto_query_range query_range;
    cc::capped_array<pr::auto_buffer, 5> readback_buffers;

    cc::array<bool> timing_usage_flags;
    cc::array<double> last_timings;
    cc::array<uint64_t> readback_memory;
};

}
