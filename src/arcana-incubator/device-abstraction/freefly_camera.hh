#pragma once

#include <cmath>

#include <typed-geometry/types/pos.hh>
#include <typed-geometry/types/quat.hh>
#include <typed-geometry/types/vec.hh>

namespace inc::da
{
struct input_manager;
class SDLWindow;

/// simple camera state, assumes LHS world space
struct fps_cam_state
{
    tg::pos3 position = tg::pos3(0);
    tg::vec3 forward = tg::vec3(0, 0, 1);

    void move_relative(tg::vec3 distance);

    void mouselook(float dx, float dy);
    
    void mousestrafe(float dx, float dy, float speed = 75.f);
    
    void set_focus(tg::pos3 focus, tg::vec3 global_offset);
};

/// exponential smoothing camera, assumes LHS world space
/// has provisions for out of the box input handling
struct smooth_fps_cam
{
    fps_cam_state physical;
    fps_cam_state target;

    float sensitivity_rotation = 70.f;
    float sensitivity_position = 25.f;

    /// interpolates physical state towards target based on delta time and sensitivities
    /// returns true if changes to physical were made
    bool interpolate_to_target(float dt);

    //
    // default input all-in-one

    void setup_default_inputs(input_manager& input);
    bool update_default_inputs(SDLWindow& window, input_manager& input, float dt, float base_speed = 10.f);
};

/// Smoothed lerp alpha, framerate-correct
/// lower smoothing -> higher alphas
inline float smooth_lerp_alpha(float smoothing, float dt) { return 1 - std::pow(smoothing, dt); }

/// Exponential decay alpha, framerate-correct damp / lerp
inline float exponential_decay_alpha(float lambda, float dt) { return 1 - std::exp(-lambda * dt); }

/// alpha based on the halftime between current and target state
inline float halftime_lerp_alpha(float halftime, float dt) { return 1 - std::pow(.5f, dt / halftime); }

tg::quat forward_to_rotation(tg::vec3 fwd, tg::vec3 up = {0, 1, 0});

/// calculates the halton sequence for temporal jittering, in [0,1]
constexpr float halton_sequence(int index, int base)
{
    float res = 0.f;
    float base_inv = 1.f / base;
    float f = base_inv;
    while (index > 0)
    {
        res += (index % base) * f;
        index /= base;
        f *= base_inv;
    }
    return res;
}
} // namespace inc::da
