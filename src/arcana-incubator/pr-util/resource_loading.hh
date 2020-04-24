#pragma once

#include <clean-core/pair.hh>

#include <phantasm-hardware-interface/detail/unique_buffer.hh>
#include <phantasm-hardware-interface/types.hh>

#include <phantasm-renderer/fwd.hh>
#include <phantasm-renderer/resource_types.hh>

namespace inc::pre
{
struct pr_mesh
{
    pr::auto_buffer vertex;
    pr::auto_buffer index;
};

[[nodiscard]] bool is_shader_present(char const* path, char const* path_prefix = "");

/// loads a shader (binary) from disk and returns a pr::auto_shader_binary
/// usage: auto [vs, b1] = load_shader(ctx, "post/bloom_ps", phi::shader_stage::vertex, "res/shaders/bin/");
[[nodiscard]] cc::pair<pr::auto_shader_binary, phi::detail::unique_buffer> load_shader(pr::Context& ctx, char const* path, phi::shader_stage stage, char const* path_prefix = "");

/// loads a .obj or binary mesh from disk and flushes GPU completely, result is usable immediately
[[nodiscard]] pr_mesh load_mesh(pr::Context& ctx, char const* path, bool binary = false);
}
