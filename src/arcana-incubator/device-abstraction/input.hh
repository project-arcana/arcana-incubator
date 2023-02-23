#pragma once

#include <SDL2/SDL_events.h>

#include <clean-core/alloc_vector.hh>

#include <typed-geometry/types/vec.hh>

#include "input_enums.hh"

namespace inc::da
{
struct input_manager;

struct binding
{
    bool isActive() const { return is_active; }
    bool isActiveSinceTicks(uint32_t numTicks) const { return getHeldTicks() >= numTicks; }

    // pressed this frame
    bool wasPressed() const { return is_active && num_ticks_steady == 0; }
    // released this frame
    bool wasReleased() const { return !is_active && num_ticks_steady == 0; }

    // pressed this frame and was only up for max. #ticks before
    bool wasPressedAfterMaximumTicks(uint32_t maxTicks) const { return wasPressed() && getPreviouslyReleasedTicks() <= maxTicks; }
    // released this frame and was only down for max. #ticks before
    bool wasReleasedAfterMaximumTicks(uint32_t maxTicks) const { return wasReleased() && getPreviouslyHeldTicks() <= maxTicks; }

    // amount of frames down in a row
    uint32_t getHeldTicks() const { return is_active ? num_ticks_steady + 1 : 0; }
    // amount of frames up in a row
    uint32_t getReleasedTicks() const { return !is_active ? num_ticks_steady + 1 : 0; }

    // amount of frames the signal was active previously
    uint32_t getPreviouslyHeldTicks() const { return !is_active ? prev_num_ticks_steady : 0; }
    // amount of ticks the signal was inactive previously
    uint32_t getPreviouslyReleasedTicks() const { return is_active ? prev_num_ticks_steady : 0; }

    float getAnalog() const { return activation; }
    float getDelta() const { return delta; }

private:
    friend input_manager;

    binding() = default;
    binding(uint64_t id) : id(id) {}

    uint64_t id = 0;

    float activation = 0.f;
    float delta = 0.f;
    bool is_active = false; ///< whether the input is considered active, by analog or digital activation

    // persistent from last frame
    int num_active_digital = 0;

    float prev_activation = 0.f;
    float prev_delta = 0.f;
    bool prev_active = false;

    // amount of ticks that the active state did not flip
    uint32_t num_ticks_steady = 0;
    // amount of steady ticks before the last flip
    uint32_t prev_num_ticks_steady = 0;

    void prePoll();

    void addKeyEvent(bool is_press);
    void addControllerAxisEvent(::Sint16 value, float threshold, float deadzone, float scale, float bias);
    void addControllerStickEvent(float value, float threshold);
    void addDelta(float delta);

    void postPoll();
};

struct input_manager
{
    void initialize(uint32_t max_num_bindings = 256, cc::allocator* allocator = cc::system_allocator);

    bool detectController();

    //
    // updating

    void updatePrePoll();
    // processes an event, returns true if it was an input event
    bool processEvent(SDL_Event const& e);
    void updatePostPoll();

    //
    // binding

    // digital

    void bindKey(uint64_t id, scancode sc) { bindKeyRawScancode(id, SDL_Scancode(sc)); }
    void bindKey(uint64_t id, keycode kc) { bindKeyRawKeycode(id, SDL_Keycode(kc)); }
    void bindMouseButton(uint64_t id, mouse_button btn) { bindMouseButtonRaw(id, uint8_t(btn)); }
    void bindControllerButton(uint64_t id, controller_button btn) { bindControllerButtonRaw(id, uint8_t(btn)); }

    // analog

    // NOTE: for analog sticks, prefer bindControllerStick for proper radial deadzone handling
    void bindControllerAxis(uint64_t id, controller_axis axis, float deadzone = 0.2395f, float threshold = 0.5f, float scale = 1.f, float bias = 0.f);

    // bind a controller stick to two bindings, one per axis (X/Y)
    // deadzoneLow:  distance from center to ignore in [0,1], XInput recomendation: 0.2395f (= 7849 / 32767)
    // deadzoneHigh: distance from unit circle to ignore in [0,1]
    void bindControllerStick(uint64_t idX, uint64_t idY, controller_analog_stick stick, float deadzoneLow = 0.2395f, float deadzoneHigh = 0.14f, float perAxisDigitalThreshold = 0.5f);

    void bindMouseWheel(uint64_t id, float scale = 1.f, bool vertical = false);

    // mouse (delta only)

    void bindMouseAxis(uint64_t id, mouse_axis axis, float delta_multiplier = 1.f);

    //
    // polling

    binding const& get(uint64_t id) { return _bindings[getOrCreateBinding(id)]; }

    tg::ivec2 getMousePositionRelative() const;

    SDL_GameController* getCurrentController() const { return _game_controller; }

private:
    void bindKeyRawKeycode(uint64_t id, SDL_Keycode keycode);
    void bindKeyRawScancode(uint64_t id, SDL_Scancode scancode);
    void bindMouseButtonRaw(uint64_t id, uint8_t sdl_mouse_button);
    void bindControllerButtonRaw(uint64_t id, uint8_t sdl_controller_button);

    void bindControllerAxisRaw(uint64_t id, uint8_t sdl_controller_axis, float deadzone = 0.2395f, float threshold = 0.5f, float scale = 1.f, float bias = 0.f);

    // returns index into bindings member
    uint32_t getOrCreateBinding(uint64_t id);

private:
    struct keycode_assoc
    {
        SDL_Keycode keycode;
        uint32_t binding_idx;
    };

    struct scancode_assoc
    {
        SDL_Scancode scancode;
        uint32_t binding_idx;
    };

    struct mousebutton_assoc
    {
        uint8_t mouse_button;
        uint32_t binding_idx;
    };

    struct joybutton_assoc
    {
        uint8_t controller_button;
        uint32_t binding_idx;
    };

    struct joyaxis_assoc
    {
        uint8_t controller_axis;
        uint32_t binding_idx;
        float deadzone;
        float threshold;
        float scale;
        float bias;
    };

    struct mouseaxis_assoc
    {
        uint32_t binding_idx;
        float delta_mul;
    };

    struct mousewheel_assoc
    {
        uint32_t binding_idx;
        float scale;
        bool is_vertical;
    };

    // a single game controller stick has two associated bindings
    // 1:2 association required for radial deadzones, magnitude scaling etc.
    struct stick_assoc
    {
        controller_analog_stick controller_stick;
        uint32_t binding_idx_x;
        uint32_t binding_idx_y;
        float deadzoneLow;
        float deadzoneHigh;

        // the value per axis above which an input is a digital activation
        // (after all deadzone/sensitivity transforms)
        float perAxisDigitalThreshold;

        // raw values per axis
        float cachedValX = 0.f;
        float cachedValY = 0.f;
        bool cachedValsNew = false;
    };

    cc::alloc_vector<binding> _bindings; // never resizes

    cc::alloc_vector<keycode_assoc> _keycode_assocs;
    cc::alloc_vector<scancode_assoc> _scancode_assocs;
    cc::alloc_vector<mousebutton_assoc> _mousebutton_assocs;
    cc::alloc_vector<joybutton_assoc> _joybutton_assocs;
    cc::alloc_vector<joyaxis_assoc> _joyaxis_assocs;
    cc::alloc_vector<stick_assoc> _stick_assocs;

    cc::alloc_vector<mouseaxis_assoc> _mouseaxis_assocs_x;
    cc::alloc_vector<mouseaxis_assoc> _mouseaxis_assocs_y;
    cc::alloc_vector<mousewheel_assoc> _mousewheel_assocs;

    SDL_GameController* _game_controller = nullptr;
};
} // namespace inc::da
