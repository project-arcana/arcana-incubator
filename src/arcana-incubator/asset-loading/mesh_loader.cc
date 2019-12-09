#include "mesh_loader.hh"

#include <unordered_map>

#include <typed-geometry/tg-std.hh>

#include <clean-core/array.hh>

#include <arcana-incubator/asset-loading/lib/tiny_obj_loader.hh>

using inc::assets::simple_vertex;

namespace std
{
template <>
struct hash<simple_vertex>
{
    size_t operator()(simple_vertex const& vertex) const
    {
        return ((hash<tg::pos3>()(vertex.position) ^ (hash<tg::vec3>()(vertex.normal) << 1)) >> 1) ^ (hash<tg::vec2>()(vertex.texcoord) << 1);
    }
};
}

inc::assets::simple_mesh_data inc::assets::load_obj_mesh(const char* path, bool flip_uvs, bool flip_xaxis)
{
    tinyobj::attrib_t attrib;
    std::vector<tinyobj::shape_t> shapes;
    std::vector<tinyobj::material_t> materials;
    std::string warnings, errors;

    auto const ret = tinyobj::LoadObj(&attrib, &shapes, &materials, &warnings, &errors, path);
    CC_RUNTIME_ASSERT(ret && "Failed to load mesh");

    simple_mesh_data res;
    res.vertices.reserve(attrib.vertices.size() / 3);
    res.indices.reserve(attrib.vertices.size() * 6); // heuristic, possibly also exposed by tinyobjs returns

    std::unordered_map<simple_vertex, int> unique_vertices;
    unique_vertices.reserve(attrib.vertices.size() / 3);

    auto const transform_uv = [&](tg::vec2 uv) {
        if (flip_uvs)
            uv.y = 1.f - uv.y;

        return uv;
    };

    for (auto const& shape : shapes)
    {
        for (auto const& index : shape.mesh.indices)
        {
            simple_vertex vertex = {};
            vertex.position = tg::pos3(attrib.vertices[3 * index.vertex_index + 0], attrib.vertices[3 * index.vertex_index + 1],
                                       attrib.vertices[3 * index.vertex_index + 2]);

            if (index.texcoord_index != -1)
                vertex.texcoord = transform_uv(tg::vec2(attrib.texcoords[2 * index.texcoord_index + 0], attrib.texcoords[2 * index.texcoord_index + 1]));

            if (index.normal_index != -1)
                vertex.normal = tg::vec3(attrib.normals[3 * index.normal_index + 0], attrib.normals[3 * index.normal_index + 1],
                                         attrib.normals[3 * index.normal_index + 2]);

            if (flip_xaxis)
            {
                vertex.position.x *= -1;
                vertex.normal.x *= -1;
            }

            if (unique_vertices.count(vertex) == 0)
            {
                unique_vertices[vertex] = int(res.vertices.size());
                res.vertices.push_back(vertex);
            }

            res.indices.push_back(unique_vertices[vertex]);
        }
    }

    res.vertices.shrink_to_fit();
    res.indices.shrink_to_fit();

    // calculate tangents
    cc::array<tg::vec3> intermediate_tangents(res.vertices.size());
    cc::fill(intermediate_tangents, tg::vec3(0, 0, 0));
    cc::array<tg::vec3> intermediate_bitangents(res.vertices.size());
    cc::fill(intermediate_bitangents, tg::vec3(0, 0, 0));

    for (auto tri_i = 0u; tri_i < res.indices.size() / 3; ++tri_i)
    {
        auto const i0 = res.indices[tri_i * 3 + 0];
        auto const i1 = res.indices[tri_i * 3 + 1];
        auto const i2 = res.indices[tri_i * 3 + 2];

        auto const& v0 = res.vertices[i0];
        auto const& v1 = res.vertices[i1];
        auto const& v2 = res.vertices[i2];
        auto const& p0 = v0.position;
        auto const& p1 = v1.position;
        auto const& p2 = v2.position;
        auto const& w0 = v0.texcoord;
        auto const& w1 = v1.texcoord;
        auto const& w2 = v2.texcoord;

        auto const e1 = p1 - p0;
        auto const e2 = p2 - p0;

        float x1 = w1.x - w0.x, x2 = w2.x - w0.x;
        float y1 = w1.y - w0.y, y2 = w2.y - w0.y;

        float r = 1.f / (x1 * y2 - x2 * y1);

        auto t = (e1 * y2 - e2 * y1) * r;
        auto b = (e2 * x1 - e1 * x2) * r;

        intermediate_tangents[i0] += t;
        intermediate_tangents[i1] += t;
        intermediate_tangents[i2] += t;

        intermediate_bitangents[i0] += b;
        intermediate_bitangents[i1] += b;
        intermediate_bitangents[i2] += b;
    }

    auto const reject = [](tg::vec3 const& a, tg::vec3 const& b) -> tg::vec3 { return a - tg::dot(a, b) * b; };

    for (auto i = 0u; i < res.vertices.size(); ++i)
    {
        auto& vert = res.vertices[i];
        auto const& t = intermediate_tangents[i];
        auto const& b = intermediate_bitangents[i];
        auto const& n = vert.normal;

        auto const xyz = tg::normalize_safe(reject(t, n));
        vert.tangent = tg::vec4(xyz, tg::dot(tg::cross(t, b), n) > 0.f ? 1.f : -1.f);
    }

    return res;
}
