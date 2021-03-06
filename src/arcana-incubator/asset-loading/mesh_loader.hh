#pragma once

#include <cstdint>

#include <clean-core/alloc_vector.hh>
#include <clean-core/span.hh>

#include <typed-geometry/tg-lean.hh>

namespace inc::assets
{
struct simple_vertex
{
    tg::pos3 position = tg::pos3(0, 0, 0);
    tg::vec3 normal = tg::vec3(0, 1, 0);
    tg::vec2 texcoord = tg::vec2(0, 0);
    tg::vec4 tangent = tg::vec4(0, 0, 0, 0);

    constexpr bool operator==(simple_vertex const& rhs) const noexcept
    {
        return position == rhs.position && normal == rhs.normal && texcoord == rhs.texcoord && tangent == rhs.tangent;
    }
};

struct skinned_vertex
{
    tg::pos3 position = tg::pos3(0, 0, 0);
    tg::vec3 normal = tg::vec3(0, 1, 0);
    tg::vec2 texcoord = tg::vec2(0, 0);
    tg::vec4 tangent = tg::vec4(0, 0, 0, 0);
    tg::ivec4 joint_indices;
    tg::vec4 joint_weights;

    constexpr bool operator==(skinned_vertex const& rhs) const noexcept
    {
        return position == rhs.position && normal == rhs.normal && texcoord == rhs.texcoord && tangent == rhs.tangent
               && joint_indices == rhs.joint_indices && joint_weights == rhs.joint_weights;
    }
};

template <class I>
constexpr void introspect(I&& i, simple_vertex& v)
{
    i(v.position, "position");
    i(v.normal, "normal");
    i(v.texcoord, "texcoord");
    i(v.tangent, "tangent");
}

template <class I>
constexpr void introspect(I&& i, skinned_vertex& v)
{
    i(v.position, "position");
    i(v.normal, "normal");
    i(v.texcoord, "texcoord");
    i(v.tangent, "tangent");
    i(v.joint_indices, "indices");
    i(v.joint_weights, "weights");
}

struct simple_mesh_data
{
    cc::alloc_vector<uint32_t> indices;
    cc::alloc_vector<simple_vertex> vertices;
    cc::alloc_vector<uint32_t> num_indices_per_submesh;
};

struct simple_mesh_data_nonowning
{
    cc::span<uint32_t const> indices;
    cc::span<simple_vertex const> vertices;
};

[[nodiscard]] simple_mesh_data load_obj_mesh(char const* path, bool flip_uvs = true, bool flip_xaxis = true, float scale = 1.f, cc::allocator* alloc = cc::system_allocator);

// fills out tangents, requires other fields
void calculate_mesh_tangents(cc::span<inc::assets::simple_vertex> inout_vertices, cc::span<uint32_t const> indices, cc::allocator* scratch_alloc = cc::system_allocator);

// fills out normals, requires positions and texcoords
void calculate_mesh_normals(cc::span<inc::assets::simple_vertex> inout_vertices, cc::span<uint32_t const> indices, cc::allocator* scratch_alloc = cc::system_allocator);

bool write_binary_mesh(simple_mesh_data const& mesh, char const* out_path);

[[nodiscard]] simple_mesh_data load_binary_mesh(char const* path, cc::allocator* alloc = cc::system_allocator);
[[nodiscard]] simple_mesh_data load_binary_mesh(cc::span<std::byte const> data, cc::allocator* alloc = cc::system_allocator);

[[nodiscard]] simple_mesh_data_nonowning parse_binary_mesh(cc::span<std::byte const> data);

tg::aabb3 calculate_mesh_aabb(cc::span<simple_vertex const> vertices);
tg::aabb3 calculate_mesh_aabb(cc::span<skinned_vertex const> vertices);
}
