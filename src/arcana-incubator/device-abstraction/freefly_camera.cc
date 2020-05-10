#include "freefly_camera.hh"

#include <typed-geometry/tg.hh>

#include <rich-log/log.hh>

#include "input.hh"

namespace
{
enum e_input : uint64_t
{
    ge_input_forward,
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

tg::quat inc::da::fps_cam_state::forward_to_rotation(tg::vec3 forward, tg::vec3 up)
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

void inc::da::smooth_fps_cam::interpolate_to_target(float dt)
{
    auto const alpha_rotation = tg::min(1.f, exponential_decay_alpha(sensitivity_rotation, dt));
    auto const alpha_position = tg::min(1.f, exponential_decay_alpha(sensitivity_position, dt));

    physical.forward = tg::normalize(tg::lerp(physical.forward, target.forward, alpha_rotation));
    physical.position = tg::lerp(physical.position, target.position, alpha_position);
}

void inc::da::smooth_fps_cam::setup_default_inputs(inc::da::input_manager& input)
{
    input.bindKey(ge_input_forward, SDL_SCANCODE_W);
    input.bindKey(ge_input_left, SDL_SCANCODE_A);
    input.bindKey(ge_input_back, SDL_SCANCODE_S);
    input.bindKey(ge_input_right, SDL_SCANCODE_D);
    input.bindKey(ge_input_up, SDL_SCANCODE_E);
    input.bindKey(ge_input_down, SDL_SCANCODE_Q);
    input.bindKey(ge_input_speedup, SDL_SCANCODE_LSHIFT);
    input.bindKey(ge_input_slowdown, SDL_SCANCODE_LCTRL);

    input.bindControllerAxis(ge_input_back, SDL_CONTROLLER_AXIS_LEFTY, 0.2f);
    input.bindControllerAxis(ge_input_right, SDL_CONTROLLER_AXIS_LEFTX, 0.2f);
    input.bindControllerAxis(ge_input_down, SDL_CONTROLLER_AXIS_TRIGGERLEFT, 0.1f, 0.5f, 0.5f, 0.5f);
    input.bindControllerAxis(ge_input_up, SDL_CONTROLLER_AXIS_TRIGGERRIGHT, 0.1f, 0.5f, 0.5f, 0.5f);
    input.bindControllerAxis(ge_input_camlook_x_analog, SDL_CONTROLLER_AXIS_RIGHTX, 0.25f);
    input.bindControllerAxis(ge_input_camlook_y_analog, SDL_CONTROLLER_AXIS_RIGHTY, 0.25f);

    input.bindControllerButton(ge_input_speedup, SDL_CONTROLLER_BUTTON_RIGHTSHOULDER);
    input.bindControllerButton(ge_input_slowdown, SDL_CONTROLLER_BUTTON_LEFTSHOULDER);
    input.bindControllerButton(ge_input_camlook_active, SDL_CONTROLLER_BUTTON_B);

    input.bindMouseButton(ge_input_camlook_active, SDL_BUTTON_RIGHT);
    input.bindMouseAxis(ge_input_camlook_x, 0, -.65f);
    input.bindMouseAxis(ge_input_camlook_y, 1, -.65f);
}

void inc::da::smooth_fps_cam::update_default_inputs(SDL_Window* window, inc::da::input_manager& input, float dt)
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
        if (!_mouse_captured)
        {
            SDL_GetMouseState(&_mouse_x_precap, &_mouse_y_precap);
            SDL_SetRelativeMouseMode(SDL_TRUE);
            _mouse_captured = true;
        }

        mouse_delta = {
            input.get(ge_input_camlook_x).getDelta() * 0.001f, //
            input.get(ge_input_camlook_y).getDelta() * 0.001f  //
        };
    }
    else if (_mouse_captured)
    {
        SDL_SetRelativeMouseMode(SDL_FALSE);
        SDL_WarpMouseInWindow(window, _mouse_x_precap, _mouse_y_precap);
        _mouse_captured = false;
    }

    mouse_delta.x += input.get(ge_input_camlook_x_analog).getAnalog() * -2.f * dt;
    mouse_delta.y += input.get(ge_input_camlook_y_analog).getAnalog() * -2.f * dt;
    target.mouselook(mouse_delta.x, mouse_delta.y);

    interpolate_to_target(dt);
}
