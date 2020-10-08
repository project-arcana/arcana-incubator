#include "mesh_loader.hh"

#include <fstream>
#include <unordered_map>

#include <typed-geometry/tg-std.hh>

#include <clean-core/algorithms.hh>
#include <clean-core/array.hh>
#include <clean-core/hash.hh>

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

namespace
{
void calculate_tangents(inc::assets::simple_mesh_data& mesh)
{
    auto intermediate_tangents = cc::array<tg::vec3>::filled(mesh.vertices.size(), {0, 0, 0});
    auto intermediate_bitangents = cc::array<tg::vec3>::filled(mesh.vertices.size(), {0, 0, 0});

    for (auto tri_i = 0u; tri_i < mesh.indices.size() / 3; ++tri_i)
    {
        auto const i0 = static_cast<unsigned>(mesh.indices[tri_i * 3 + 0]);
        auto const i1 = static_cast<unsigned>(mesh.indices[tri_i * 3 + 1]);
        auto const i2 = static_cast<unsigned>(mesh.indices[tri_i * 3 + 2]);

        auto const& v0 = mesh.vertices[i0];
        auto const& v1 = mesh.vertices[i1];
        auto const& v2 = mesh.vertices[i2];

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

    for (auto i = 0u; i < mesh.vertices.size(); ++i)
    {
        auto& vert = mesh.vertices[i];
        auto const& t = intermediate_tangents[i];
        auto const& b = intermediate_bitangents[i];
        auto const& n = vert.normal;

        auto const xyz = tg::normalize_safe(reject(t, n));
        vert.tangent = tg::vec4(xyz, tg::dot(tg::cross(t, b), n) > 0.f ? 1.f : -1.f);
    }
}
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

    {
        res.vertices.reserve(attrib.vertices.size() / 3);

        size_t num_indices = 0;
        for (auto const& shape : shapes)
        {
            num_indices += shape.mesh.indices.size();
        }
        res.indices.reserve(num_indices);
    }

    std::unordered_map<simple_vertex, uint32_t> unique_vertices;
    unique_vertices.reserve(attrib.vertices.size() / 3);

    auto const transform_uv = [&](tg::vec2 uv) {
        if (flip_uvs)
            uv.y = 1.f - uv.y;

        return uv;
    };

    for (auto const& shape : shapes)
    {
        CC_RUNTIME_ASSERT(shape.mesh.num_face_vertices[0] == 3 && "mesh not triangulated");
        for (auto const& index : shape.mesh.indices)
        {
            auto const vert_i = static_cast<size_t>(index.vertex_index);

            simple_vertex vertex = {};
            vertex.position = tg::pos3(attrib.vertices[3 * vert_i + 0], attrib.vertices[3 * vert_i + 1], attrib.vertices[3 * vert_i + 2]);

            if (index.texcoord_index != -1)
            {
                auto const texc_i = static_cast<size_t>(index.texcoord_index);
                vertex.texcoord = transform_uv(tg::vec2(attrib.texcoords[2 * texc_i + 0], attrib.texcoords[2 * texc_i + 1]));
            }
            else
            {
                vertex.texcoord.x = tg::fract(vertex.position.x);
                vertex.texcoord.y = tg::fract(vertex.position.y);
            }

            if (index.normal_index != -1)
            {
                auto const norm_i = static_cast<size_t>(index.normal_index);
                vertex.normal = tg::vec3(attrib.normals[3 * norm_i + 0], attrib.normals[3 * norm_i + 1], attrib.normals[3 * norm_i + 2]);
            }

            if (flip_xaxis)
            {
                vertex.position.x *= -1;
                vertex.normal.x *= -1;
            }

            if (unique_vertices.count(vertex) == 0)
            {
                unique_vertices[vertex] = uint32_t(res.vertices.size());
                res.vertices.push_back(vertex);
            }

            res.indices.push_back(unique_vertices[vertex]);
        }
    }

    calculate_tangents(res);

    // vertices was over-reserved
    res.vertices.shrink_to_fit();
    return res;
}

void inc::assets::write_binary_mesh(const inc::assets::simple_mesh_data& mesh, const char* out_path)
{
    auto outfile = std::fstream(out_path, std::ios::out | std::ios::binary);
    CC_RUNTIME_ASSERT(outfile.good() && "failed to write mesh");

    size_t const num_indices = mesh.indices.size();
    outfile.write((char const*)&num_indices, sizeof(num_indices));
    outfile.write((char const*)mesh.indices.data(), sizeof(mesh.indices[0]) * num_indices);

    size_t const num_vertices = mesh.vertices.size();
    outfile.write((char const*)&num_vertices, sizeof(num_vertices));
    outfile.write((char const*)mesh.vertices.data(), sizeof(mesh.vertices[0]) * num_vertices);
    outfile.close();
}

inc::assets::simple_mesh_data inc::assets::load_binary_mesh(const char* path)
{
    auto infile = std::fstream(path, std::ios::binary | std::ios::in);
    CC_RUNTIME_ASSERT(infile.good() && "failed to load mesh");

    simple_mesh_data res;

    size_t num_indices = 0;
    infile.read((char*)&num_indices, sizeof(num_indices));
    res.indices.resize(num_indices);
    infile.read((char*)res.indices.data(), sizeof(res.indices[0]) * num_indices);

    size_t num_vertices = 0;
    infile.read((char*)&num_vertices, sizeof(num_vertices));
    res.vertices.resize(num_vertices);
    infile.read((char*)res.vertices.data(), sizeof(res.vertices[0]) * num_vertices);
    infile.close();
    return res;
}

struct byte_reader
{
    byte_reader() = default;
    byte_reader(cc::span<cc::byte const> buffer) : _buffer(buffer.data()), _head(buffer.data()), _buffer_end(buffer.data() + buffer.size()) {}

    template <class T>
    void read_t(T& out_t)
    {
        static_assert(std::is_trivially_copyable_v<T>, "T not memcpyable");
        CC_ASSERT(_head + sizeof(T) <= _buffer_end && "read OOB");
        std::memcpy(&out_t, _head, sizeof(T));
        _head += sizeof(T);
    }

    void read(cc::span<cc::byte> out_data)
    {
        CC_ASSERT(_head + out_data.size() <= _buffer_end && "read OOB");
        std::memcpy(out_data.data(), _head, out_data.size());
        _head += out_data.size();
    }

    void skip(size_t size)
    {
        CC_ASSERT(_head + size <= _buffer_end && "skip OOB");
        _head += size;
    }

    void reset() { _head = _buffer; }

    size_t size() const { return _buffer == nullptr ? 0 : _buffer_end - _buffer; }
    size_t size_left() const { return _buffer == nullptr ? 0 : _buffer_end - _head; }

    std::byte const* head() const { return _head; }

private:
    std::byte const* _buffer = nullptr;
    std::byte const* _head = nullptr;
    std::byte const* _buffer_end = nullptr;
};

inc::assets::simple_mesh_data inc::assets::load_binary_mesh(cc::span<const std::byte> data)
{
    auto reader = byte_reader{data};
    simple_mesh_data res;

    size_t num_indices = 0;
    reader.read_t(num_indices);
    res.indices.resize(num_indices);
    reader.read(cc::as_byte_span(res.indices));

    size_t num_vertices = 0;
    reader.read_t(num_vertices);
    res.vertices.resize(num_vertices);
    reader.read(cc::as_byte_span(res.vertices));
    return res;
}

inc::assets::simple_mesh_data_nonowning inc::assets::parse_binary_mesh(cc::span<const std::byte> data)
{
    auto reader = byte_reader{data};

    simple_mesh_data_nonowning res;

    size_t num_indices = 0;
    reader.read_t(num_indices);
    res.indices = cc::span{reinterpret_cast<uint32_t const*>(reader.head()), num_indices};
    reader.skip(res.indices.size_bytes());

    size_t num_vertices = 0;
    reader.read_t(num_vertices);
    res.vertices = cc::span{reinterpret_cast<simple_vertex const*>(reader.head()), num_vertices};

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
