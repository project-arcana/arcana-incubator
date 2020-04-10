#pragma once

#include <SDL2/SDL_events.h>

#include <clean-core/map.hh>
#include <clean-core/vector.hh>

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
    void addJoyaxisEvent(::Sint16 value, float threshold, float deadzone);
    void addDelta(float delta);

    void postPoll();
};

struct input_manager
{
    void initialize(unsigned max_num_bindings) { bindings.reserve(max_num_bindings); }

    //
    // updating

    void updatePrePoll();
    // processes an event, returns true if it was an input event
    bool processEvent(SDL_Event const& e);
    void updatePostPoll();

    //
    // binding

    void bindKey(uint64_t id, SDL_Keycode keycode);
    void bindKey(uint64_t id, SDL_Scancode scancode);
    void bindMouseButton(uint64_t id, uint8_t sdl_mouse_button);
    void bindJoyButton(uint64_t id, uint8_t sdl_joy_button);

    void bindJoyAxis(uint64_t id, uint8_t sdl_joy_axis, float deadzone = 0.01f, float threshold = 0.5f);
    void bindMouseAxis(uint64_t id, unsigned index, float delta_multiplier = 1.f);

    //
    // polling

    binding const& get(uint64_t id) { return bindings[getOrCreateBinding(id)]; }

private:
    // returns index into bindings member
    unsigned getOrCreateBinding(uint64_t id);

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
        uint8_t joy_button;
        unsigned binding_idx;
    };

    struct joyaxis_assoc
    {
        uint8_t joy_axis;
        unsigned binding_idx;
        float deadzone;
        float threshold;
    };

    struct mouseaxis_assoc
    {
        unsigned binding_idx;
        float delta_mul;
    };

    cc::vector<binding> bindings; // never resizes
    cc::vector<keycode_assoc> keycode_assocs;
    cc::vector<scancode_assoc> scancode_assocs;
    cc::vector<mousebutton_assoc> mousebutton_assocs;
    cc::vector<joybutton_assoc> joybutton_assocs;
    cc::vector<joyaxis_assoc> joyaxis_assocs;

    cc::vector<mouseaxis_assoc> mouseaxis_assocs_x;
    cc::vector<mouseaxis_assoc> mouseaxis_assocs_y;
};
}
