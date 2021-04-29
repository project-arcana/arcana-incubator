#include "mesh_loader.hh"

#include <fstream>
#include <unordered_map>

#include <typed-geometry/tg-std.hh>

#include <clean-core/algorithms.hh>
#include <clean-core/alloc_array.hh>
#include <clean-core/array.hh>
#include <clean-core/hash.hh>

#include <phantasm-hardware-interface/common/byte_reader.hh>

#include <arcana-incubator/asset-loading/lib/tiny_obj_loader.hh>

using inc::assets::simple_vertex;

namespace std
{
template <>
struct hash<simple_vertex>
{
    size_t operator()(simple_vertex const& vertex) const noexcept
    {
        return cc::make_hash(vertex.position.x, vertex.position.y, vertex.position.z, vertex.normal.x, vertex.normal.y, vertex.normal.z,
                             vertex.texcoord.x, vertex.texcoord.y);
    }
};
}

inc::assets::simple_mesh_data inc::assets::load_obj_mesh(const char* path, bool flip_uvs, bool flip_xaxis, float scale, cc::allocator* alloc)
{
    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string warnings, errors;

    bool const success = tinyobj::LoadObj(&attrib, &shapes, &materials, &warnings, &errors, path);

    // warnings are mostly missing materials (which is irrelevant here)
    /*if (!warnings.empty())
    {
        std::fprintf(stderr, "[mesh_loader] tinyobj reported warnings:\n  %s\n", warnings.c_str());
    }*/
    if (!errors.empty())
    {
        std::fprintf(stderr, "[mesh_loader] tinyobj reported errors:\n%s\n", errors.c_str());
    }

    if (!success)
    {
        return {};
    }

    simple_mesh_data res;

    {
        res.vertices.reset_reserve(alloc, attrib.vertices.size() / 3);

        size_t num_indices = 0;
        for (auto const& shape : shapes)
        {
            num_indices += shape.mesh.indices.size();
        }
        res.indices.reset_reserve(alloc, num_indices);
        res.num_indices_per_submesh.reset_reserve(alloc, shapes.size());
    }

    std::unordered_map<simple_vertex, uint32_t> unique_vertices;
    unique_vertices.reserve(attrib.vertices.size() / 3);

    auto f_transform_uv = [&](tg::vec2 uv) {
        if (flip_uvs)
            uv.y = 1.f - uv.y;

        return uv;
    };

    float const xaxis_multiplier = flip_xaxis ? -1.f : 1.f;
    auto const pos_scale = tg::comp3(xaxis_multiplier * scale, scale, scale);

    uint32_t numMissingUVs = 0, numMissingNormals = 0;

    for (auto const& shape : shapes)
    {
        CC_RUNTIME_ASSERT(shape.mesh.num_face_vertices[0] == 3 && "mesh not triangulated");

        res.num_indices_per_submesh.push_back(uint32_t(shape.mesh.indices.size()));
        for (auto const& index : shape.mesh.indices)
        {
            auto const vert_i = static_cast<size_t>(index.vertex_index);

            simple_vertex vertex = {};
            vertex.position = tg::pos3(attrib.vertices[3 * vert_i + 0], attrib.vertices[3 * vert_i + 1], attrib.vertices[3 * vert_i + 2]);

            if (index.texcoord_index != -1)
            {
                auto const texc_i = static_cast<size_t>(index.texcoord_index);
                vertex.texcoord = f_transform_uv(tg::vec2(attrib.texcoords[2 * texc_i + 0], attrib.texcoords[2 * texc_i + 1]));
            }
            else
            {
                ++numMissingUVs;
                vertex.texcoord.x = tg::fract(vertex.position.x);
                vertex.texcoord.y = tg::fract(vertex.position.y);
            }

            if (index.normal_index != -1)
            {
                auto const norm_i = static_cast<size_t>(index.normal_index);
                vertex.normal = tg::vec3(attrib.normals[3 * norm_i + 0], attrib.normals[3 * norm_i + 1], attrib.normals[3 * norm_i + 2]);
            }
            else
            {
                ++numMissingNormals;
            }

            vertex.position *= pos_scale;
            vertex.normal.x *= xaxis_multiplier;

            if (unique_vertices.count(vertex) == 0)
            {
                unique_vertices[vertex] = uint32_t(res.vertices.size());
                res.vertices.push_back(vertex);
            }

            res.indices.push_back(unique_vertices[vertex]);
        }
    }

    if (numMissingUVs > 0)
    {
        std::fprintf(stderr, "[mesh_loader] [warning] mesh has %u missing UVs across %zu indices (%.1f%%)\n", numMissingUVs, res.indices.size(),
                     (float(numMissingUVs) / float(res.indices.size())) * 100.f);
    }

    if (numMissingNormals > 0)
    {
        std::fprintf(stderr, "[mesh_loader] [warning] mesh has %u missing normals across %zu indices (%.1f%%)\n", numMissingNormals,
                     res.indices.size(), (float(numMissingNormals) / float(res.indices.size())) * 100.f);
        std::fprintf(stderr, "[mesh_loader] recomputing normals..\n");

        calculate_mesh_normals(res.vertices, res.indices);
    }

    calculate_mesh_tangents(res.vertices, res.indices);

    // vertices was over-reserved
    res.vertices.shrink_to_fit();
    return res;
}

void inc::assets::calculate_mesh_tangents(cc::span<inc::assets::simple_vertex> inout_vertices, cc::span<const uint32_t> indices, cc::allocator* scratch_alloc)
{
    auto intermediate_tangents = cc::alloc_array<tg::vec3>::filled(inout_vertices.size(), {0, 0, 0}, scratch_alloc);
    auto intermediate_bitangents = cc::alloc_array<tg::vec3>::filled(inout_vertices.size(), {0, 0, 0}, scratch_alloc);

    for (auto tri_i = 0u; tri_i < indices.size() / 3; ++tri_i)
    {
        auto const i0 = indices[tri_i * 3 + 0];
        auto const i1 = indices[tri_i * 3 + 1];
        auto const i2 = indices[tri_i * 3 + 2];

        auto const& v0 = inout_vertices[i0];
        auto const& v1 = inout_vertices[i1];
        auto const& v2 = inout_vertices[i2];

        auto const e1 = v1.position - v0.position;
        auto const e2 = v2.position - v0.position;

        auto const duv1 = v1.texcoord - v0.texcoord;
        auto const duv2 = v2.texcoord - v0.texcoord;

        float const r = 1.f / (duv1.x * duv2.y - duv2.x * duv1.y);

        auto t = (e1 * duv2.y - e2 * duv1.y) * r;
        auto b = (e2 * duv1.x - e1 * duv2.x) * r;

        intermediate_tangents[i0] += t;
        intermediate_tangents[i1] += t;
        intermediate_tangents[i2] += t;

        intermediate_bitangents[i0] += b;
        intermediate_bitangents[i1] += b;
        intermediate_bitangents[i2] += b;
    }

    auto const reject = [](tg::vec3 const& a, tg::vec3 const& b) -> tg::vec3 { return a - tg::dot(a, b) * b; };

    for (auto i = 0u; i < inout_vertices.size(); ++i)
    {
        auto& vert = inout_vertices[i];
        auto const& t = intermediate_tangents[i];
        auto const& b = intermediate_bitangents[i];
        auto const& n = vert.normal;

        auto const xyz = tg::normalize_safe(reject(t, n));
        vert.tangent = tg::vec4(xyz, tg::dot(tg::cross(t, b), n) > 0.f ? 1.f : -1.f);
    }
}

void inc::assets::calculate_mesh_normals(cc::span<inc::assets::simple_vertex> inout_vertices, cc::span<uint32_t const> indices, cc::allocator* scratch_alloc)
{
    auto numNormalsPerVertex = cc::alloc_vector<uint32_t>::filled(inout_vertices.size(), 0u, scratch_alloc);

    uint32_t numTris = uint32_t(indices.size()) / 3u;
    for (auto tri_i = 0u; tri_i < numTris; ++tri_i)
    {
        uint32_t i0 = indices[tri_i * 3 + 0];
        uint32_t i1 = indices[tri_i * 3 + 1];
        uint32_t i2 = indices[tri_i * 3 + 2];

        simple_vertex& v0 = inout_vertices[i0];
        simple_vertex& v1 = inout_vertices[i1];
        simple_vertex& v2 = inout_vertices[i2];

        auto const e1 = v1.position - v0.position;
        auto const e2 = v2.position - v0.position;

        auto const normal = tg::normalize_safe(tg::cross(e1, e2)); // left-handed, clockwise winding

        v0.normal += normal;
        v1.normal += normal;
        v2.normal += normal;

        numNormalsPerVertex[i0] += 1;
        numNormalsPerVertex[i1] += 1;
        numNormalsPerVertex[i2] += 1;
    }

    for (auto i = 0u; i < inout_vertices.size(); ++i)
    {
        inout_vertices[i].normal /= float(cc::max(numNormalsPerVertex[i], 1u));
    }
}

bool inc::assets::write_binary_mesh(const inc::assets::simple_mesh_data& mesh, const char* out_path)
{
    auto outfile = std::fstream(out_path, std::ios::out | std::ios::binary);
    if (!outfile.good())
        return false;

    size_t const num_indices = mesh.indices.size();
    outfile.write((char const*)&num_indices, sizeof(num_indices));
    outfile.write((char const*)mesh.indices.data(), sizeof(mesh.indices[0]) * num_indices);

    size_t const num_vertices = mesh.vertices.size();
    outfile.write((char const*)&num_vertices, sizeof(num_vertices));
    outfile.write((char const*)mesh.vertices.data(), sizeof(mesh.vertices[0]) * num_vertices);
    outfile.close();
    return true;
}

inc::assets::simple_mesh_data inc::assets::load_binary_mesh(const char* path, cc::allocator* alloc)
{
    auto infile = std::fstream(path, std::ios::binary | std::ios::in);
    CC_RUNTIME_ASSERT(infile.good() && "failed to load mesh");

    simple_mesh_data res;

    size_t num_indices = 0;
    infile.read((char*)&num_indices, sizeof(num_indices));
    res.indices.reset_reserve(alloc, num_indices);
    res.indices.resize(num_indices);
    infile.read((char*)res.indices.data(), sizeof(res.indices[0]) * num_indices);

    size_t num_vertices = 0;
    infile.read((char*)&num_vertices, sizeof(num_vertices));
    res.vertices.reset_reserve(alloc, num_vertices);
    res.vertices.resize(num_vertices);
    infile.read((char*)res.vertices.data(), sizeof(res.vertices[0]) * num_vertices);
    infile.close();
    return res;
}

inc::assets::simple_mesh_data inc::assets::load_binary_mesh(cc::span<const std::byte> data, cc::allocator* alloc)
{
    auto reader = phi::byte_reader{data};
    simple_mesh_data res;

    auto const indices_span = reader.read_sized_array<uint32_t>();
    res.indices.reset_reserve(alloc, indices_span.size());
    res.indices.resize(indices_span.size());
    indices_span.copy_to<uint32_t>(res.indices);

    auto const vertices_span = reader.read_sized_array<simple_vertex>();
    res.vertices.reset_reserve(alloc, vertices_span.size());
    res.vertices.resize(vertices_span.size());
    vertices_span.copy_to<simple_vertex>(res.vertices);

    return res;
}

inc::assets::simple_mesh_data_nonowning inc::assets::parse_binary_mesh(cc::span<const std::byte> data)
{
    auto reader = phi::byte_reader{data};
    simple_mesh_data_nonowning res;

    res.indices = reader.read_sized_array<uint32_t>();
    res.vertices = reader.read_sized_array<simple_vertex>();

    return res;
}

tg::aabb3 inc::assets::calculate_mesh_aabb(cc::span<const inc::assets::simple_vertex> vertices)
{
    if (vertices.empty())
        return tg::aabb3::unit_from_zero;

    auto res = tg::aabb3(vertices[0].position, vertices[0].position);

    for (auto i = 1u; i < vertices.size(); ++i)
    {
        res.max = tg::max(res.max, vertices[i].position);
        res.min = tg::min(res.min, vertices[i].position);
    }

    return res;
}

tg::aabb3 inc::assets::calculate_mesh_aabb(cc::span<const inc::assets::skinned_vertex> vertices)
{
    if (vertices.empty())
        return tg::aabb3::unit_from_zero;

    auto res = tg::aabb3(vertices[0].position, vertices[0].position);

    for (auto i = 1u; i < vertices.size(); ++i)
    {
        res.max = tg::max(res.max, vertices[i].position);
        res.min = tg::min(res.min, vertices[i].position);
    }

    return res;
}
