#pragma once

#include <clean-core/pair.hh>

#include <phantasm-hardware-interface/common/container/unique_buffer.hh>
#include <phantasm-hardware-interface/types.hh>

#include <phantasm-renderer/fwd.hh>
#include <phantasm-renderer/resource_types.hh>

namespace inc::assets
{
struct simple_vertex;
}

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
[[nodiscard]] cc::pair<pr::auto_shader_binary, phi::unique_buffer> load_shader(pr::Context& ctx, char const* path, phi::shader_stage stage, char const* path_prefix = "", char const* file_ending_override = nullptr);

/// loads a .obj or binary mesh from disk to GPU
[[nodiscard]] pr_mesh load_mesh(pr::Context& ctx, char const* path, bool binary = false);

/// loads a mesh from memory to GPU
[[nodiscard]] pr_mesh load_mesh(pr::Context& ctx, cc::span<uint32_t const> indices, cc::span<inc::assets::simple_vertex const> vertices);
}
