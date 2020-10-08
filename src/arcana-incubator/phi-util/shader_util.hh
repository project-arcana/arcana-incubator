#pragma once

#include <phantasm-hardware-interface/common/container/unique_buffer.hh>

namespace inc
{
phi::unique_buffer get_shader_binary(const char* path, char const* name, char const* ending);
}
