#include "resource_loading.hh"

#include <cstdio>
#include <cstdlib>
#include <fstream>

#include <clean-core/defer.hh>

#include <phantasm-hardware-interface/Backend.hh>

#include <phantasm-renderer/CompiledFrame.hh>
#include <phantasm-renderer/Context.hh>
#include <phantasm-renderer/Frame.hh>

#include <arcana-incubator/asset-loading/mesh_loader.hh>

bool inc::pre::is_shader_present(const char* path, const char* path_prefix)
{
    char name_formatted[1024];
    std::snprintf(name_formatted, sizeof(name_formatted), "%s%s.%s", path_prefix, path, "spv");
    return std::fstream(name_formatted).good();
}
cc::pair<pr::auto_shader_binary, inc::unique_buffer> inc::pre::load_shader(
    pr::Context& ctx, const char* path, phi::shader_stage stage, char const* path_prefix, char const* file_ending_override)
{
    char const* const ending = file_ending_override ? file_ending_override : ctx.get_backend().getBackendType() == phi::backend_type::d3d12 ? "dxil" : "spv";

    char name_formatted[1024];
    std::snprintf(name_formatted, sizeof(name_formatted), "%s%s.%s", path_prefix, path, ending);

    auto buffer = inc::unique_buffer::create_from_binary_file(name_formatted);
    CC_RUNTIME_ASSERT(buffer.is_valid() && "failed to load shader");

    auto pr_shader = ctx.make_shader(buffer, stage);
    return {cc::move(pr_shader), cc::move(buffer)};
}

inc::pre::pr_mesh inc::pre::load_mesh(pr::Context& ctx, const char* path, bool binary)
{
    // load data and memcpy to upload buffer
    auto const data = binary ? inc::assets::load_binary_mesh(path) : inc::assets::load_obj_mesh(path);
    return load_mesh(ctx, data.indices, data.vertices);
}

inc::pre::pr_mesh inc::pre::load_mesh(pr::Context& ctx, cc::span<const uint32_t> indices, cc::span<const inc::assets::simple_vertex> vertices)
{
    auto b_upload = ctx.make_upload_buffer(unsigned(vertices.size_bytes() + indices.size_bytes())).disown();
    auto* const b_upload_map = ctx.map_buffer(b_upload);
    std::memcpy(b_upload_map, vertices.data(), vertices.size_bytes());
    std::memcpy(b_upload_map + vertices.size_bytes(), indices.data(), indices.size_bytes());
    ctx.unmap_buffer(b_upload);

    // create proper buffers
    pr_mesh res;
    res.vertex = ctx.make_buffer(uint32_t(vertices.size_bytes()), sizeof(inc::assets::simple_vertex));
    res.index = ctx.make_buffer(uint32_t(indices.size_bytes()), sizeof(uint32_t));

    auto frame = ctx.make_frame();

    // copy
    frame.copy(b_upload, res.vertex);
    frame.copy(b_upload, res.index, vertices.size_bytes());

    // transition
    frame.transition(res.vertex, phi::resource_state::vertex_buffer);
    frame.transition(res.index, phi::resource_state::index_buffer);

    frame.free_deferred_after_submit(b_upload);

    // submit
    ctx.submit(cc::move(frame));

    return res;
}
