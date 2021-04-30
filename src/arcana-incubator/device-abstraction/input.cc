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

void inc::da::binding::addControllerAxisEvent(Sint16 value, float threshold, float deadzone, float scale, float bias)
{
    // always overrides activation

    // NOTE: this deadzone handling is pretty poor
    // see: http://blog.hypersect.com/interpreting-analog-sticks/
    // to fix it someday
    float const abs_scaled_value = std::abs(float(value) / 32767);
    if (abs_scaled_value <= deadzone)
    {
        // snap to zero
        activation = 0.f;
    }
    else
    {
        // rescale to be in [-1, 0] / [0,1] outside of deadzone
        float const remapped_value = ((abs_scaled_value - deadzone) / (1.f - deadzone)) * (value < 0 ? -1 : 1);

        // apply custom scale and bias
        activation = remapped_value * scale + bias;
    }

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
    for (auto& binding : _bindings)
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

        for (auto const& assoc : _keycode_assocs)
            if (assoc.keycode == e.key.keysym.sym)
                _bindings[assoc.binding_idx].addKeyEvent(is_press);

        for (auto const& assoc : _scancode_assocs)
            if (assoc.scancode == e.key.keysym.scancode)
                _bindings[assoc.binding_idx].addKeyEvent(is_press);
    }
    else if (e.type == SDL_MOUSEBUTTONUP || e.type == SDL_MOUSEBUTTONDOWN)
    {
        bool is_press = e.type == SDL_MOUSEBUTTONDOWN;

        for (auto const& assoc : _mousebutton_assocs)
            if (assoc.mouse_button == e.button.button)
                _bindings[assoc.binding_idx].addKeyEvent(is_press);
    }
    else if (e.type == SDL_CONTROLLERBUTTONDOWN || e.type == SDL_CONTROLLERBUTTONUP)
    {
        bool is_press = e.type == SDL_CONTROLLERBUTTONDOWN;
        for (auto const& assoc : _joybutton_assocs)
            if (assoc.controller_button == e.cbutton.button)
                _bindings[assoc.binding_idx].addKeyEvent(is_press);
    }
    else if (e.type == SDL_MOUSEMOTION)
    {
        for (auto const& assoc : _mouseaxis_assocs_x)
            _bindings[assoc.binding_idx].addDelta(float(e.motion.xrel) * assoc.delta_mul);

        for (auto const& assoc : _mouseaxis_assocs_y)
            _bindings[assoc.binding_idx].addDelta(float(e.motion.yrel) * assoc.delta_mul);
    }
    else if (e.type == SDL_MOUSEWHEEL)
    {
        for (auto const& assoc : _mousewheel_assocs)
            _bindings[assoc.binding_idx].addDelta(float(e.wheel.x * int(assoc.is_vertical) + e.wheel.y * int(!assoc.is_vertical)) * assoc.scale);
    }
    else if (e.type == SDL_CONTROLLERAXISMOTION)
    {
        for (auto const& assoc : _joyaxis_assocs)
            if (assoc.controller_axis == e.caxis.axis)
                _bindings[assoc.binding_idx].addControllerAxisEvent(e.caxis.value, assoc.threshold, assoc.deadzone, assoc.scale, assoc.bias);
    }
    else
    {
        was_input_event = false;
    }

    return was_input_event;
}

void inc::da::input_manager::updatePostPoll()
{
    for (auto& binding : _bindings)
        binding.postPoll();
}

void inc::da::input_manager::bindKeyRawKeycode(uint64_t id, SDL_Keycode keycode) { _keycode_assocs.push_back({keycode, getOrCreateBinding(id)}); }

void inc::da::input_manager::bindKeyRawScancode(uint64_t id, SDL_Scancode scancode)
{
    _scancode_assocs.push_back({scancode, getOrCreateBinding(id)});
}

void inc::da::input_manager::bindMouseButtonRaw(uint64_t id, uint8_t sdl_mouse_button)
{
    _mousebutton_assocs.push_back({sdl_mouse_button, getOrCreateBinding(id)});
}

void inc::da::input_manager::bindControllerButtonRaw(uint64_t id, uint8_t sdl_controller_button)
{
    _joybutton_assocs.push_back({sdl_controller_button, getOrCreateBinding(id)});
}

void inc::da::input_manager::bindControllerAxisRaw(uint64_t id, uint8_t sdl_controller_axis, float deadzone, float threshold, float scale, float bias)
{
    // NOTE: the default deadzone argument, 0.2395f, is XInput's recommended deadzone (= 7849 / 32767)
    CC_ASSERT(deadzone < 1.f && deadzone >= 0.f && "invalid deadzone");
    _joyaxis_assocs.push_back({sdl_controller_axis, getOrCreateBinding(id), deadzone, threshold, scale, bias});
}

void inc::da::input_manager::bindMouseAxis(uint64_t id, unsigned index, float delta_multiplier)
{
    cc::vector<mouseaxis_assoc>& dest = index == 0 ? _mouseaxis_assocs_x : _mouseaxis_assocs_y;
    dest.push_back({getOrCreateBinding(id), delta_multiplier});
}

void inc::da::input_manager::bindMouseAxis(uint64_t id, inc::da::mouse_axis axis, float delta_multiplier)
{
    cc::vector<mouseaxis_assoc>& dest = axis == mouse_axis::x ? _mouseaxis_assocs_x : _mouseaxis_assocs_y;
    dest.push_back({getOrCreateBinding(id), delta_multiplier});
}

void inc::da::input_manager::bindMouseWheel(uint64_t id, float scale, bool vertical)
{
    _mousewheel_assocs.push_back({getOrCreateBinding(id), scale, vertical});
}

tg::ivec2 inc::da::input_manager::getMousePositionRelative() const
{
    tg::ivec2 res;
    SDL_GetMouseState(&res.x, &res.y);
    return res;
}

bool inc::da::input_manager::detectController()
{
    if (_game_controller != nullptr)
    {
        SDL_GameControllerClose(_game_controller);
        _game_controller = nullptr;
    }

    int const num_joysticks = SDL_NumJoysticks();
    for (int i = 0; i < num_joysticks; ++i)
    {
        if (SDL_IsGameController(i))
        {
            _game_controller = SDL_GameControllerOpen(i);
            break;
        }
    }

    return _game_controller != nullptr;
}

unsigned inc::da::input_manager::getOrCreateBinding(uint64_t id)
{
    for (auto i = 0u; i < _bindings.size(); ++i)
    {
        if (_bindings[i].id == id)
            return i;
    }

    CC_ASSERT(_bindings.size() != _bindings.capacity() && "bindings full");
    _bindings.push_back(binding(id));
    return unsigned(_bindings.size() - 1);
}
