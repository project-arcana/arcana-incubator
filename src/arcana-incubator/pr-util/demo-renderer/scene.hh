#pragma once

#include <clean-core/capped_array.hh>
#include <clean-core/utility.hh>
#include <clean-core/vector.hh>

#include <typed-geometry/tg-lean.hh>

#include <phantasm-renderer/argument.hh>

#include "types.hh"

namespace inc::pre::dmr
{
struct scene_gpudata
{
    tg::mat4 proj;
    tg::mat4 proj_inv;
    tg::mat4 view;
    tg::mat4 view_inv;
    tg::mat4 vp;
    tg::mat4 vp_inv;
    tg::mat4 clean_vp;
    tg::mat4 clean_vp_inv;
    tg::mat4 prev_clean_vp;
    tg::mat4 prev_clean_vp_inv;

    void fill_data(tg::isize2 res, tg::pos3 campos, tg::vec3 camforward, unsigned halton_index);
};

struct scene
{
    //
    // parallel instance arrays
    cc::vector<instance> instances;
    cc::vector<instance_gpudata> instance_transforms;

    //
    // global data
    tg::isize2 resolution;
    unsigned halton_index = 0;
    bool is_history_a = true;
    scene_gpudata camdata;

    //
    // multi-buffered resources
    struct per_frame_resource_t
    {
        pr::auto_buffer cb_camdata;
        pr::auto_buffer sb_modeldata;
    };

    cc::capped_array<per_frame_resource_t, 5> per_frame_resources;
    unsigned num_backbuffers = 0;
    unsigned current_frame_index = 0;

    void init(pr::Context& ctx, unsigned max_num_instances);

    per_frame_resource_t& last_frame() { return per_frame_resources[cc::wrapped_decrement(current_frame_index, num_backbuffers)]; }
    per_frame_resource_t& current_frame() { return per_frame_resources[current_frame_index]; }
    per_frame_resource_t& next_frame() { return per_frame_resources[cc::wrapped_increment(current_frame_index, num_backbuffers)]; }

    void on_next_frame();

    void upload_current_frame(pr::Context& ctx);

    void addInstance(handle::mesh mesh, handle::material mat, tg::mat4 transform)
    {
        instances.push_back({mesh, mat});
        instance_transforms.push_back({transform});
    }
};

}
