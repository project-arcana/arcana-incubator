#include "freefly_camera.hh"

#include <typed-geometry/tg.hh>

void inc::da::fps_cam_state::move_relative(tg::vec3 distance)
{
    auto const rotation = forward_to_rotation(forward);
    auto const delta = tg::transpose(tg::mat3(rotation)) * distance;

    position += delta;
}

void inc::da::fps_cam_state::mouselook(float dx, float dy)
{
    auto altitude = tg::atan2(forward.y, length(tg::vec2(forward.x, forward.z)));
    auto azimuth = tg::atan2(forward.z, forward.x);

    azimuth += 1_rad * dx;
    altitude = tg::clamp(altitude - 1_rad * dy, -89_deg, 89_deg);

    auto caz = tg::cos(azimuth);
    auto saz = tg::sin(azimuth);
    auto cal = tg::cos(altitude);
    auto sal = tg::sin(altitude);

    forward = tg::vec3(cal * caz, sal, cal * saz);
}

tg::quat inc::da::fps_cam_state::forward_to_rotation(tg::vec3 forward, tg::vec3 up)
{
    auto const fwd = tg::normalize(forward);
    tg::vec3 rightVector = tg::normalize(tg::cross(fwd, up));
    tg::vec3 upVector = tg::cross(rightVector, fwd);

    tg::mat3 rotMatrix;
    rotMatrix[0][0] = rightVector.x;
    rotMatrix[0][1] = upVector.x;
    rotMatrix[0][2] = -fwd.x;
    rotMatrix[1][0] = rightVector.y;
    rotMatrix[1][1] = upVector.y;
    rotMatrix[1][2] = -fwd.y;
    rotMatrix[2][0] = rightVector.z;
    rotMatrix[2][1] = upVector.z;
    rotMatrix[2][2] = -fwd.z;
    return tg::quat::from_rotation_matrix(rotMatrix);
}

void inc::da::smooth_fps_cam::interpolate_to_target(float dt)
{
    auto const alpha_rotation = tg::min(1.f, exponentialDecayAlpha(sensitivity_rotation, dt));
    auto const alpha_position = tg::min(1.f, exponentialDecayAlpha(sensitivity_position, dt));

    physical.forward = tg::normalize(tg::lerp(physical.forward, target.forward, alpha_rotation));
    physical.position = tg::lerp(physical.position, target.position, alpha_position);
}
