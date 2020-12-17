#include "freefly_camera.hh"

#include <typed-geometry/tg.hh>

#include <rich-log/log.hh>

#include "device_abstraction.hh"
#include "input.hh"

namespace
{
enum e_input : uint64_t
{
    ge_input_forward = 100'000'000,
    ge_input_left,
    ge_input_back,
    ge_input_right,
    ge_input_up,
    ge_input_down,
    ge_input_speedup,
    ge_input_slowdown,
    ge_input_camlook_active,
    ge_input_camlook_x,
    ge_input_camlook_y,
    ge_input_camlook_x_analog,
    ge_input_camlook_y_analog
};

}

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
    altitude = tg::clamp(altitude - 1_rad * -dy, -89_deg, 89_deg);

    auto caz = tg::cos(azimuth);
    auto saz = tg::sin(azimuth);
    auto cal = tg::cos(altitude);
    auto sal = tg::sin(altitude);

    forward = tg::vec3(cal * caz, sal, cal * saz);
}

bool inc::da::smooth_fps_cam::interpolate_to_target(float dt)
{
    auto const alpha_rotation = tg::min(1.f, exponential_decay_alpha(sensitivity_rotation, dt));
    auto const alpha_position = tg::min(1.f, exponential_decay_alpha(sensitivity_position, dt));

    auto const forward_diff = target.forward - physical.forward;
    auto const pos_diff = target.position - physical.position;

    if (tg::length_sqr(forward_diff) + tg::length_sqr(pos_diff) < tg::epsilon<float>)
    {
        // no changes below threshold
        return false;
    }
    else
    {
        physical.forward = tg::normalize(physical.forward + alpha_rotation * forward_diff);
        physical.position = physical.position + alpha_position * pos_diff;
        return true;
    }
}

void inc::da::smooth_fps_cam::setup_default_inputs(inc::da::input_manager& input)
{
    input.bindKey(ge_input_forward, inc::da::scancode::sc_W);
    input.bindKey(ge_input_left, inc::da::scancode::sc_A);
    input.bindKey(ge_input_back, inc::da::scancode::sc_S);
    input.bindKey(ge_input_right, inc::da::scancode::sc_D);
    input.bindKey(ge_input_up, inc::da::scancode::sc_E);
    input.bindKey(ge_input_down, inc::da::scancode::sc_Q);
    input.bindKey(ge_input_speedup, inc::da::scancode::sc_LSHIFT);
    input.bindKey(ge_input_slowdown, inc::da::scancode::sc_LCTRL);

    input.bindControllerAxis(ge_input_back, inc::da::controller_axis::ca_left_y);
    input.bindControllerAxis(ge_input_right, inc::da::controller_axis::ca_left_x);
    input.bindControllerAxis(ge_input_down, inc::da::controller_axis::ca_left_trigger, 0.239f, 0.5f, 0.5f, 0.5f);
    input.bindControllerAxis(ge_input_up, inc::da::controller_axis::ca_right_trigger, 0.239f, 0.5f, 0.5f, 0.5f);
    input.bindControllerAxis(ge_input_camlook_x_analog, inc::da::controller_axis::ca_right_x);
    input.bindControllerAxis(ge_input_camlook_y_analog, inc::da::controller_axis::ca_right_y);

    input.bindControllerButton(ge_input_speedup, inc::da::controller_button::cb_right_shoulder);
    input.bindControllerButton(ge_input_slowdown, inc::da::controller_button::cb_left_shoulder);
    input.bindControllerButton(ge_input_camlook_active, inc::da::controller_button::cb_B);

    input.bindMouseButton(ge_input_camlook_active, inc::da::mouse_button::mb_right);
    input.bindMouseAxis(ge_input_camlook_x, inc::da::mouse_axis::x, -.65f);
    input.bindMouseAxis(ge_input_camlook_y, inc::da::mouse_axis::y, -.65f);
}

bool inc::da::smooth_fps_cam::update_default_inputs(SDLWindow& window, inc::da::input_manager& input, float dt)
{
    auto speed_mul = 10.f;

    if (input.get(ge_input_speedup).isActive())
        speed_mul *= 4.f;

    if (input.get(ge_input_slowdown).isActive())
        speed_mul *= .25f;

    auto const delta_move = tg::vec3{input.get(ge_input_right).getAnalog() - input.get(ge_input_left).getAnalog(),
                                     input.get(ge_input_up).getAnalog() - input.get(ge_input_down).getAnalog(),
                                     input.get(e_input::ge_input_forward).getAnalog() - input.get(ge_input_back).getAnalog()

                            }
                            * dt * speed_mul;

    target.move_relative(delta_move);

    tg::vec2 mouse_delta = {0, 0};

    if (input.get(ge_input_camlook_active).isActive())
    {
        window.captureMouse();

        mouse_delta = {
            input.get(ge_input_camlook_x).getDelta() * 0.001f, //
            input.get(ge_input_camlook_y).getDelta() * 0.001f  //
        };
    }
    else
    {
        window.uncaptureMouse();
    }

    mouse_delta.x += input.get(ge_input_camlook_x_analog).getAnalog() * -2.f * dt;
    mouse_delta.y += input.get(ge_input_camlook_y_analog).getAnalog() * -2.f * dt;
    target.mouselook(mouse_delta.x, mouse_delta.y);

    return interpolate_to_target(dt);
}

tg::quat inc::da::forward_to_rotation(tg::vec3 forward, tg::vec3 up)
{
    auto const fwd = -tg::normalize(forward);
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
