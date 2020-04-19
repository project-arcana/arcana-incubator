#pragma once

#include <cmath>

#include <typed-geometry/types/pos.hh>
#include <typed-geometry/types/quat.hh>
#include <typed-geometry/types/vec.hh>

namespace inc::da
{
/// Smoothed lerp alpha, framerate-correct
inline float smooth_lerp_alpha(float smoothing, float dt) { return 1 - std::pow(smoothing, dt); }
/// Exponential decay alpha, framerate-correct damp / lerp
inline float exponential_decay_alpha(float lambda, float dt) { return 1 - std::exp(-lambda * dt); }
/// alpha based on the halftime between current and target state
inline float halftime_lerp_alpha(float halftime, float dt) { return 1 - std::pow(.5f, dt / halftime); }

constexpr float halton_sequence(int index, int base)
{
    float f = 1.f;
    float r = 0.f;
    while (index > 0)
    {
        f = f / float(base);
        r = r + f * float(index % base);
        index = index / base;
    }
    return r;
}


struct fps_cam_state
{
    tg::pos3 position = tg::pos3(0, 0, 5);
    tg::vec3 forward = tg::vec3(0, 0, 1);

    void move_relative(tg::vec3 distance);
    void mouselook(float dx, float dy);
    static tg::quat forward_to_rotation(tg::vec3 fwd, tg::vec3 up = {0, 1, 0});
};

struct input_manager;

struct smooth_fps_cam
{
    fps_cam_state physical;
    fps_cam_state target;

    float sensitivity_rotation = 70.f;
    float sensitivity_position = 25.f;

    void interpolate_to_target(float dt);

    //
    // default input all-in-one

    void setup_default_inputs(input_manager& input);
    void update_default_inputs(input_manager& input, float dt);

private:
    bool _mouse_captured = false;
};

}