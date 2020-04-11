#include "input.hh"

#include <cmath>

#include <rich-log/log.hh>

void inc::da::binding::prePoll()
{
    prev_activation = activation;
    prev_delta = delta;
    prev_active = is_active;

    delta = 0.f;
}

void inc::da::binding::addKeyEvent(bool is_press)
{
    num_active_digital += is_press ? 1 : -1;

    if (num_active_digital <= 0)
    {
        num_active_digital = 0;
    }

    is_active = num_active_digital > 0;
    activation = is_active ? 1.f : 0.f; // right now always overrides activation
}

void inc::da::binding::addJoyaxisEvent(Sint16 value, float threshold, float deadzone)
{
    // always overrides activation
    activation = float(value) / 32767;
    if (std::abs(activation) <= deadzone)
        activation = 0.f;

    if (std::abs(activation) >= threshold)
    {
        is_active = true;
    }
    else
    {
        // only deactivate if no digitals are active
        is_active = num_active_digital == 0;
    }
}

void inc::da::binding::addDelta(float delta) { this->delta += delta; }

void inc::da::binding::postPoll()
{
    bool const is_steady = is_active == prev_active;
    num_ticks_steady = is_steady ? num_ticks_steady + 1 : 0;
}

void inc::da::input_manager::updatePrePoll()
{
    for (auto& binding : bindings)
        binding.prePoll();
}

bool inc::da::input_manager::processEvent(const SDL_Event& e)
{
    bool was_input_event = true;
    if (e.type == SDL_KEYDOWN || e.type == SDL_KEYUP)
    {
        if (e.key.repeat != 0) // skip repeat events
            return true;

        bool is_press = e.type == SDL_KEYDOWN;

        for (auto const& assoc : keycode_assocs)
            if (assoc.keycode == e.key.keysym.sym)
                bindings[assoc.binding_idx].addKeyEvent(is_press);

        for (auto const& assoc : scancode_assocs)
            if (assoc.scancode == e.key.keysym.scancode)
                bindings[assoc.binding_idx].addKeyEvent(is_press);
    }
    else if (e.type == SDL_MOUSEBUTTONUP || e.type == SDL_MOUSEBUTTONDOWN)
    {
        bool is_press = e.type == SDL_MOUSEBUTTONDOWN;

        for (auto const& assoc : mousebutton_assocs)
            if (assoc.mouse_button == e.button.button)
                bindings[assoc.binding_idx].addKeyEvent(is_press);
    }
    else if (e.type == SDL_JOYBUTTONDOWN || e.type == SDL_JOYBUTTONUP)
    {
        bool is_press = e.type == SDL_JOYBUTTONDOWN;
        for (auto const& assoc : joybutton_assocs)
            if (assoc.joy_button == e.jbutton.button)
                bindings[assoc.binding_idx].addKeyEvent(is_press);
    }
    else if (e.type == SDL_MOUSEMOTION)
    {
        for (auto const& assoc : mouseaxis_assocs_x)
            bindings[assoc.binding_idx].addDelta(float(e.motion.xrel) * assoc.delta_mul);

        for (auto const& assoc : mouseaxis_assocs_y)
            bindings[assoc.binding_idx].addDelta(float(e.motion.yrel) * assoc.delta_mul);
    }
    else if (e.type == SDL_JOYAXISMOTION)
    {
        for (auto const& assoc : joyaxis_assocs)
            if (assoc.joy_axis == e.jaxis.axis)
                bindings[assoc.joy_axis].addJoyaxisEvent(e.jaxis.value, assoc.threshold, assoc.deadzone);
    }
    else
    {
        was_input_event = false;
    }

    return was_input_event;
}

void inc::da::input_manager::updatePostPoll()
{
    for (auto& binding : bindings)
        binding.postPoll();
}

void inc::da::input_manager::bindKey(uint64_t id, SDL_Keycode keycode) { keycode_assocs.push_back({keycode, getOrCreateBinding(id)}); }

void inc::da::input_manager::bindKey(uint64_t id, SDL_Scancode scancode) { scancode_assocs.push_back({scancode, getOrCreateBinding(id)}); }

void inc::da::input_manager::bindMouseButton(uint64_t id, uint8_t sdl_mouse_button)
{
    mousebutton_assocs.push_back({sdl_mouse_button, getOrCreateBinding(id)});
}

void inc::da::input_manager::bindJoyButton(uint64_t id, uint8_t sdl_joy_button)
{
    joybutton_assocs.push_back({sdl_joy_button, getOrCreateBinding(id)});
}

void inc::da::input_manager::bindJoyAxis(uint64_t id, uint8_t sdl_joy_axis, float deadzone, float threshold)
{
    joyaxis_assocs.push_back({sdl_joy_axis, getOrCreateBinding(id), deadzone, threshold});
}

void inc::da::input_manager::bindMouseAxis(uint64_t id, unsigned index, float delta_multiplier) // only emits delta, no activation nor analog
{
    cc::vector<mouseaxis_assoc>& dest = index == 0 ? mouseaxis_assocs_x : mouseaxis_assocs_y;
    dest.push_back({getOrCreateBinding(id), delta_multiplier});
}

unsigned inc::da::input_manager::getOrCreateBinding(uint64_t id)
{
    for (auto i = 0u; i < bindings.size(); ++i)
    {
        if (bindings[i].id == id)
            return i;
    }

    CC_ASSERT(bindings.size() != bindings.capacity() && "bindings full");
    bindings.push_back(binding(id));
    return unsigned(bindings.size() - 1);
}
