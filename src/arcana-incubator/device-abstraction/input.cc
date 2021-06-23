#include "input.hh"

#include <cmath>

#include <clean-core/utility.hh>

#include <rich-log/log.hh>

namespace
{
// proper analog stick deadzones, ref: http://blog.hypersect.com/interpreting-analog-sticks/
void applyRadialDeadzone(float x,            // raw stick value X in [-1,1]
                         float y,            // raw stick value Y in [-1,1]
                         float deadzoneLow,  // distance from center to ignore
                         float deadzoneHigh, // distance from unit circle to ignore
                         float& outX,
                         float& outY)
{
    float magnitude = sqrtf(x * x + y * y);

    if (magnitude <= deadzoneLow)
    {
        // inner dead zone
        outX = 0.f;
        outY = 0.f;
        return;
    }

    // scale in order to output a magnitude in [0,1]
    float legalRange = 1.f - deadzoneHigh - deadzoneLow;
    float normalizedMagnitude = cc::min(1.f, (magnitude - deadzoneLow) / legalRange);
    float scale = normalizedMagnitude / magnitude;
    outX = x * scale;
    outY = y * scale;
}
} // namespace

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

    // this deadzone handling is poor because it can only consider a single axis by design
    // use the standalone polling

    float const absScaledValue = std::abs(float(value) / 32767.f);
    if (absScaledValue <= deadzone)
    {
        // snap to zero
        activation = 0.f;
    }
    else
    {
        // rescale to be in [-1, 0] / [0,1] outside of deadzone
        float const remappedValue = ((absScaledValue - deadzone) / (1.f - deadzone)) * (value < 0 ? -1 : 1);

        // apply custom scale and bias
        activation = remappedValue * scale + bias;
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

void inc::da::binding::addControllerStickEvent(float value, float threshold)
{
    // no postprocessing, this already happened when iterating stick associations
    activation = value;

    if (std::abs(value) >= threshold)
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
    bool const isSteady = is_active == prev_active;
    num_ticks_steady = isSteady ? num_ticks_steady + 1 : 0;
}

void inc::da::input_manager::updatePrePoll()
{
    for (auto& binding : _bindings)
        binding.prePoll();
}

bool inc::da::input_manager::processEvent(const SDL_Event& e)
{
    bool wasInputEvent = true;

    if (e.type == SDL_KEYDOWN || e.type == SDL_KEYUP)
    {
        if (e.key.repeat != 0) // skip repeat events
            return true;

        bool isPress = e.type == SDL_KEYDOWN;

        for (auto const& assoc : _keycode_assocs)
            if (assoc.keycode == e.key.keysym.sym)
                _bindings[assoc.binding_idx].addKeyEvent(isPress);

        for (auto const& assoc : _scancode_assocs)
            if (assoc.scancode == e.key.keysym.scancode)
                _bindings[assoc.binding_idx].addKeyEvent(isPress);
    }
    else if (e.type == SDL_MOUSEBUTTONUP || e.type == SDL_MOUSEBUTTONDOWN)
    {
        bool isPress = e.type == SDL_MOUSEBUTTONDOWN;

        for (auto const& assoc : _mousebutton_assocs)
            if (assoc.mouse_button == e.button.button)
                _bindings[assoc.binding_idx].addKeyEvent(isPress);
    }
    else if (e.type == SDL_CONTROLLERBUTTONDOWN || e.type == SDL_CONTROLLERBUTTONUP)
    {
        bool isPress = e.type == SDL_CONTROLLERBUTTONDOWN;

        for (auto const& assoc : _joybutton_assocs)
            if (assoc.controller_button == e.cbutton.button)
                _bindings[assoc.binding_idx].addKeyEvent(isPress);
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
        {
            _bindings[assoc.binding_idx].addDelta(float(e.wheel.x * int(assoc.is_vertical) + e.wheel.y * int(!assoc.is_vertical)) * assoc.scale);
        }
    }
    else if (e.type == SDL_CONTROLLERAXISMOTION)
    {
        for (auto const& assoc : _joyaxis_assocs)
        {
            if (assoc.controller_axis == e.caxis.axis)
            {
                _bindings[assoc.binding_idx].addControllerAxisEvent(e.caxis.value, assoc.threshold, assoc.deadzone, assoc.scale, assoc.bias);
            }
        }

        controller_analog_stick stick = controller_analog_stick::INVALID;
        bool isX = false;

        if (e.caxis.axis == uint8_t(controller_axis::ca_left_x) || e.caxis.axis == uint8_t(controller_axis::ca_left_y))
        {
            stick = controller_analog_stick::left;
            isX = e.caxis.axis == uint8_t(controller_axis::ca_left_x);
        }
        else if (e.caxis.axis == uint8_t(controller_axis::ca_right_x) || e.caxis.axis == uint8_t(controller_axis::ca_right_y))
        {
            stick = controller_analog_stick::right;
            isX = e.caxis.axis == uint8_t(controller_axis::ca_right_x);
        }

        if (stick != controller_analog_stick::INVALID)
        {
            float valf = float(e.caxis.value) / 32767.f;

            // search in stick associations
            for (auto& assoc : _stick_assocs)
            {
                if (assoc.controller_stick == stick)
                {
                    if (isX)
                    {
                        assoc.cachedValX = valf;
                    }
                    else
                    {
                        assoc.cachedValY = valf;
                    }

                    assoc.cachedValsNew = true;
                }
            }
        }
    }
    else
    {
        wasInputEvent = false;
    }

    return wasInputEvent;
}

void inc::da::input_manager::updatePostPoll()
{
    // postprocess controller analog stick assocations and update their bindings
    for (auto& assoc : _stick_assocs)
    {
        if (!assoc.cachedValsNew)
            continue;

        float finalValX, finalValY;
        applyRadialDeadzone(assoc.cachedValX, assoc.cachedValY, assoc.deadzoneLow, assoc.deadzoneHigh, finalValX, finalValY);

        _bindings[assoc.binding_idx_x].addControllerStickEvent(finalValX, assoc.perAxisDigitalThreshold);
        _bindings[assoc.binding_idx_y].addControllerStickEvent(finalValY, assoc.perAxisDigitalThreshold);
        assoc.cachedValsNew = false;
    }

    // update "num ticks steady" info per binding
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

void inc::da::input_manager::bindMouseAxis(uint64_t id, inc::da::mouse_axis axis, float delta_multiplier)
{
    cc::alloc_vector<mouseaxis_assoc>& dest = axis == mouse_axis::x ? _mouseaxis_assocs_x : _mouseaxis_assocs_y;
    dest.push_back({getOrCreateBinding(id), delta_multiplier});
}

void inc::da::input_manager::bindControllerAxis(uint64_t id, controller_axis axis, float deadzone, float threshold, float scale, float bias)
{
    bindControllerAxisRaw(id, uint8_t(axis), deadzone, threshold, scale, bias);
}

void inc::da::input_manager::bindControllerStick(uint64_t idX, uint64_t idY, controller_analog_stick stick, float deadzoneLow, float deadzoneHigh, float perAxisDigitalThreshold)
{
    CC_ASSERT(deadzoneLow < 1.f && deadzoneLow >= 0.f && "invalid low deadzone");
    CC_ASSERT(deadzoneHigh < 1.f && deadzoneHigh >= 0.f && "invalid high deadzone");
    CC_ASSERT(deadzoneLow + deadzoneHigh < 1.f && "high/low deadzones overlap");

    auto const bindingX = getOrCreateBinding(idX);
    auto const bindingY = getOrCreateBinding(idY);
    _stick_assocs.push_back(stick_assoc{stick, bindingX, bindingY, deadzoneLow, deadzoneHigh, perAxisDigitalThreshold, 0.f, 0.f});
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

void inc::da::input_manager::initialize(uint32_t max_num_bindings, cc::allocator* allocator)
{
    _bindings.reset_reserve(allocator, max_num_bindings);

    _keycode_assocs.reset_reserve(allocator, 128);
    _scancode_assocs.reset_reserve(allocator, 128);
    _mousebutton_assocs.reset_reserve(allocator, 16);
    _joybutton_assocs.reset_reserve(allocator, 64);
    _joyaxis_assocs.reset_reserve(allocator, 16);
    _stick_assocs.reset_reserve(allocator, 4);
    _mouseaxis_assocs_x.reset_reserve(allocator, 4);
    _mouseaxis_assocs_y.reset_reserve(allocator, 4);
    _mousewheel_assocs.reset_reserve(allocator, 4);

    detectController();
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

uint32_t inc::da::input_manager::getOrCreateBinding(uint64_t id)
{
    for (auto i = 0u; i < _bindings.size(); ++i)
    {
        if (_bindings[i].id == id)
            return i;
    }

    CC_ASSERT(!_bindings.at_capacity() && "bindings full");

    _bindings.emplace_back_stable(binding(id));
    return uint32_t(_bindings.size() - 1);
}
