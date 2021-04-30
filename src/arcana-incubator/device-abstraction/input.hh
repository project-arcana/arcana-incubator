#pragma once

#include <SDL2/SDL_events.h>

#include <clean-core/map.hh>
#include <clean-core/vector.hh>

#include <typed-geometry/types/vec.hh>

#include "input_enums.hh"

namespace inc::da
{
struct input_manager;

struct binding
{
    bool isActive() const { return is_active; }

    bool wasPressed() const { return is_active && num_ticks_steady == 0; }
    bool wasReleased() const { return !is_active && num_ticks_steady == 0; }

    unsigned getHeldTicks() const { return is_active ? num_ticks_steady + 1 : 0; }
    unsigned getReleasedTicks() const { return !is_active ? num_ticks_steady + 1 : 0; }

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

    unsigned num_ticks_steady = 0; ///< amount of ticks that the active state did not flip

    void prePoll();

    void addKeyEvent(bool is_press);
    void addControllerAxisEvent(::Sint16 value, float threshold, float deadzone, float scale, float bias);
    void addDelta(float delta);

    void postPoll();
};

struct input_manager
{
    void initialize(unsigned max_num_bindings = 256)
    {
        _bindings.reserve(max_num_bindings);
        detectController();
    }

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

    void bindControllerAxis(uint64_t id, controller_axis axis, float deadzone = 0.2395f, float threshold = 0.5f, float scale = 1.f, float bias = 0.f)
    {
        bindControllerAxisRaw(id, uint8_t(axis), deadzone, threshold, scale, bias);
    }

    void bindMouseWheel(uint64_t id, float scale = 1.f, bool vertical = false);

    // mouse (delta only)

    void bindMouseAxis(uint64_t id, mouse_axis axis, float delta_multiplier = 1.f);
    [[deprecated("use enum overload")]] void bindMouseAxis(uint64_t id, unsigned index, float delta_multiplier = 1.f);


    //
    // polling

    binding const& get(uint64_t id) { return _bindings[getOrCreateBinding(id)]; }

    tg::ivec2 getMousePositionRelative() const;

private:
    void bindKeyRawKeycode(uint64_t id, SDL_Keycode keycode);
    void bindKeyRawScancode(uint64_t id, SDL_Scancode scancode);
    void bindMouseButtonRaw(uint64_t id, uint8_t sdl_mouse_button);
    void bindControllerButtonRaw(uint64_t id, uint8_t sdl_controller_button);

    void bindControllerAxisRaw(uint64_t id, uint8_t sdl_controller_axis, float deadzone = 0.2395f, float threshold = 0.5f, float scale = 1.f, float bias = 0.f);

    // returns index into bindings member
    unsigned getOrCreateBinding(uint64_t id);

private:
    struct keycode_assoc
    {
        SDL_Keycode keycode;
        unsigned binding_idx;
    };

    struct scancode_assoc
    {
        SDL_Scancode scancode;
        unsigned binding_idx;
    };

    struct mousebutton_assoc
    {
        uint8_t mouse_button;
        unsigned binding_idx;
    };

    struct joybutton_assoc
    {
        uint8_t controller_button;
        unsigned binding_idx;
    };

    struct joyaxis_assoc
    {
        uint8_t controller_axis;
        unsigned binding_idx;
        float deadzone;
        float threshold;
        float scale;
        float bias;
    };

    struct mouseaxis_assoc
    {
        unsigned binding_idx;
        float delta_mul;
    };

    struct mousewheel_assoc
    {
        unsigned binding_idx;
        float scale;
        bool is_vertical;
    };

    cc::vector<binding> _bindings; // never resizes
    cc::vector<keycode_assoc> _keycode_assocs;
    cc::vector<scancode_assoc> _scancode_assocs;
    cc::vector<mousebutton_assoc> _mousebutton_assocs;
    cc::vector<joybutton_assoc> _joybutton_assocs;
    cc::vector<joyaxis_assoc> _joyaxis_assocs;

    cc::vector<mouseaxis_assoc> _mouseaxis_assocs_x;
    cc::vector<mouseaxis_assoc> _mouseaxis_assocs_y;
    cc::vector<mousewheel_assoc> _mousewheel_assocs;

    SDL_GameController* _game_controller = nullptr;
};
}
