#pragma once

namespace inc
{
struct unique_buffer;

unique_buffer get_shader_binary(const char* path, char const* name, char const* ending);
} // namespace inc
