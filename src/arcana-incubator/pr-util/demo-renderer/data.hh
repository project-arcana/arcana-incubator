#pragma once

#include <clean-core/capped_array.hh>
#include <clean-core/utility.hh>

#include <typed-geometry/tg-lean.hh>

namespace inc::pre::dmr
{
struct camera_gpudata
{
    tg::mat4 proj;              ///< projection matrix, jittered
    tg::mat4 proj_inv;          ///< inverse of projection matrix, jittered
    tg::mat4 view;              ///< view matrix
    tg::mat4 view_inv;          ///< inverse of view matrix
    tg::mat4 vp;                ///< proj * view, jittered
    tg::mat4 vp_inv;            ///< inverse of (proj * view), jittered
    tg::mat4 clean_vp;          ///< proj * view, unjittered
    tg::mat4 clean_vp_inv;      ///< inverse of (proj * view), unjittered
    tg::mat4 prev_clean_vp;     ///< unjittered (proj * view) from previous frame
    tg::mat4 prev_clean_vp_inv; ///< inverse of unjittered (proj * view) from previous frame

    void fill_data(tg::isize2 res, tg::pos3 campos, tg::vec3 camforward, unsigned halton_index);

    // utility
    tg::pos3 extract_campos() const { return tg::pos3(view_inv[3]); }
    tg::ray3 calculate_view_ray(tg::vec2 normalized_mouse_pos) const;
};

struct frame_index_state
{
    // static
    unsigned num_backbuffers = 0;
    // dynamic
    unsigned halton_index = 0;
    unsigned current_frame_index = 0;
    bool is_history_a = true;

    void increment()
    {
        CC_ASSERT(num_backbuffers > 0 && "uninitialized");
        current_frame_index = cc::wrapped_increment(current_frame_index, num_backbuffers);
        halton_index = cc::wrapped_increment(halton_index, 8u);
        is_history_a = !is_history_a;
    }

    unsigned last() const { return cc::wrapped_decrement(current_frame_index, num_backbuffers); }
    unsigned current() const { return current_frame_index; }
    unsigned next() const { return cc::wrapped_increment(current_frame_index, num_backbuffers); }
};
}
