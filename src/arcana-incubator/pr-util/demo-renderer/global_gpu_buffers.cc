#include "global_gpu_buffers.hh"

#include <typed-geometry/tg.hh>

#include <arcana-incubator/device-abstraction/freefly_camera.hh>

void inc::pre::dmr::camera_gpudata::fill_data(tg::isize2 res, tg::pos3 campos, tg::vec3 camforward, unsigned halton_index)
{
    auto const clean_proj = tg::perspective_reverse_z_directx(60_deg, res.width / float(res.height), 0.1f);

    auto const jitter_x = (inc::da::halton_sequence(halton_index, 2) - 0.5f) / float(res.width);
    auto const jitter_y = (inc::da::halton_sequence(halton_index, 3) - 0.5f) / float(res.height);

    proj = clean_proj;
    proj[2][0] = jitter_x;
    proj[2][1] = jitter_y;
    proj_inv = tg::inverse(proj);

    view = tg::look_at_directx(campos, camforward, tg::vec3(0, 1, 0));
    view_inv = tg::inverse(view);

    vp = proj * view;
    vp_inv = tg::inverse(vp);

    prev_clean_vp = clean_vp;
    prev_clean_vp_inv = clean_vp_inv;

    clean_vp = clean_proj * view;
    clean_vp_inv = tg::inverse(clean_vp);
}
