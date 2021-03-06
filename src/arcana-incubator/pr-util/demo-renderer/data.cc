#include "data.hh"

#include <typed-geometry/tg.hh>

#include <arcana-incubator/device-abstraction/freefly_camera.hh>

void inc::pre::dmr::camera_gpudata::fill_data(tg::isize2 res, tg::pos3 campos, tg::vec3 camforward, unsigned halton_index, tg::angle fov, float nearplane)
{
    auto const clean_proj = tg::perspective_reverse_z_directx(fov, res.width / float(res.height), nearplane);

    // halton is [0,1], rescale to [-1,1], divide by resolution
    auto const jitter_x = (2.f * inc::da::halton_sequence(halton_index, 2) - 1.f) / float(res.width);
    auto const jitter_y = (2.f * inc::da::halton_sequence(halton_index, 3) - 1.f) / float(res.height);

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

tg::ray3 inc::pre::dmr::camera_gpudata::calculate_view_ray(tg::vec2 mousepos_norm) const
{
    auto hdc_far = tg::vec4{tg::clamp(mousepos_norm.x, 0, 1) * +2.f - 1.f, // x
                            tg::clamp(mousepos_norm.y, 0, 1) * -2.f + 1.f, // y
                            0.1f,                                          // z - far but not 0
                            1};
    hdc_far = vp_inv * hdc_far;
    hdc_far /= hdc_far.w;

    auto const world_far = tg::pos3(hdc_far);
    auto const campos = extract_campos();
    return tg::ray3{campos, tg::normalize(world_far - campos)};
}

bool inc::pre::dmr::camera_gpudata::project_mouse_to_plane(tg::vec2 mousepos_normalized, tg::pos3& out_hit, tg::dir3 plane_normal, tg::pos3 plane_center) const
{
    auto const ray = calculate_view_ray(mousepos_normalized);
    auto const plane = tg::plane3(plane_normal, plane_center);
    auto const res = tg::intersection(ray, plane);

    if (!res.any())
        return false;

    out_hit = res.first();
    return true;
}
