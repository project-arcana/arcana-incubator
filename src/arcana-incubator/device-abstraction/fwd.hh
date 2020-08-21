#pragma once

union SDL_Event;
struct SDL_Window;

namespace inc::da
{
class SDLWindow;
class Timer;

struct input_manager;
struct fps_cam_state;
struct smooth_fps_cam;
struct binding;

enum class scancode : int;
enum class keycode : int;
enum class controller_axis : int;
enum class controller_button : int;
enum class mouse_button : int;
}
