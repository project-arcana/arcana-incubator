#pragma once

#include <phantasm-hardware-interface/detail/unique_buffer.hh>

namespace inc
{
phi::detail::unique_buffer get_shader_binary(const char* path, char const* name, char const* ending);
}
