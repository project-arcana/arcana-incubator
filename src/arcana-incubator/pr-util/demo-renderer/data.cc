#include "data.hh"

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

tg::ray3 inc::pre::dmr::camera_gpudata::calculate_view_ray(tg::vec2 normalized_mouse_pos) const
{
    CC_ASSERT(0.f <= normalized_mouse_pos.x && normalized_mouse_pos.x <= 1.f && "mouse pos not normalized");
    CC_ASSERT(0.f <= normalized_mouse_pos.y && normalized_mouse_pos.y <= 1.f && "mouse pos not normalized");

    tg::vec3 ps[2];
    auto i = 0;
    for (auto d : {0.5f, -0.5f})
    {
        tg::vec4 v{normalized_mouse_pos.x * 2.f - 1.f, 1 - normalized_mouse_pos.y * 2.f, d * 2.f - 1.f, 1.f};

        v = this->proj_inv * v;
        v /= v.w;
        v = this->view_inv * v;
        ps[i++] = tg::vec3(v);
    }

    return tg::ray3{extract_campos(), tg::normalize(ps[0] - ps[1])};
}
