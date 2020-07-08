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

    void fill_data(tg::isize2 res, tg::pos3 campos, tg::vec3 camforward, unsigned halton_index, tg::angle fov = 60_deg, float nearplane = 0.1f);

    /// returns the camera position, reading it out of the inverse view matrix
    tg::pos3 extract_campos() const { return tg::pos3(view_inv[3]); }

    /// calculates a view ray based on the mouse position, normalized (in [0,1])
    tg::ray3 calculate_view_ray(tg::vec2 mousepos_normalized) const;
};

struct frame_index_state
{
    // static
    unsigned num_backbuffers = 0;
    // dynamic
    unsigned halton_index = 0;
    unsigned current_frame_index = 0; // in [0, num_backbuffers - 1]
    bool is_history_a = true;         // A -> B or B -> A frame
    bool did_wrap = false;            // if current_frame_index wrapped to every index once

    void increment()
    {
        CC_ASSERT(num_backbuffers > 0 && "uninitialized");

        if (current_frame_index == num_backbuffers - 1)
            did_wrap = true;

        current_frame_index = cc::wrapped_increment(current_frame_index, num_backbuffers);
        halton_index = cc::wrapped_increment(halton_index, 8u);
        is_history_a = !is_history_a;
    }

    unsigned last() const { return cc::wrapped_decrement(current_frame_index, num_backbuffers); }
    unsigned current() const { return current_frame_index; }
    unsigned next() const { return cc::wrapped_increment(current_frame_index, num_backbuffers); }
};
}
