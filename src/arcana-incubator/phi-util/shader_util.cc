#include "shader_util.hh"

#include <cstdio>

#include "unique_buffer.hh"

inc::unique_buffer inc::get_shader_binary(char const* path, const char* name, const char* ending)
{
    char name_formatted[1024];
    std::snprintf(name_formatted, sizeof(name_formatted), "%s/%s.%s", path, name, ending);
    return inc::unique_buffer::create_from_binary_file(name_formatted);
}
