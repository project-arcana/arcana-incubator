#include "asset_pack.hh"

#include <phantasm-renderer/Context.hh>
#include <phantasm-renderer/Frame.hh>

#include <arcana-incubator/asset-loading/mesh_loader.hh>
#include <arcana-incubator/pr-util/texture_processing.hh>

inc::pre::dmr::handle::mesh inc::pre::dmr::AssetPack::loadMesh(pr::Context& ctx, const char* path, bool binary)
{
    auto const res = _meshes.acquire();
    _meshes.get(res) = inc::pre::load_mesh(ctx, path, binary);
    return {res};
}

inc::pre::dmr::handle::mesh inc::pre::dmr::AssetPack::loadMesh(pr::Context& ctx, cc::span<const std::byte> data)
{
    auto const res = _meshes.acquire();
    auto const data_parsed = inc::assets::parse_binary_mesh(data);
    _meshes.get(res) = inc::pre::load_mesh(ctx, data_parsed.indices, data_parsed.vertices);
    return {res};
}

inc::pre::dmr::handle::material inc::pre::dmr::AssetPack::loadMaterial(
    pr::Context& ctx, inc::pre::texture_processing& tex, const char* p_albedo, const char* p_normal, const char* p_arm)
{
    auto frame = ctx.make_frame();

    auto const res = _materials.acquire();
    material_node& node = _materials.get(res);

    auto arg_builder = ctx.build_argument();

    if (p_albedo)
    {
        node.albedo = tex.load_texture_from_file(frame, p_albedo, pr::format::rgba8un, true, true);
        frame.transition(node.albedo, pr::state::shader_resource, pr::shader::pixel);
        arg_builder.add(node.albedo);
    }
    if (p_normal)
    {
        node.normal = tex.load_texture_from_file(frame, p_normal, pr::format::rgba8un, true, false);
        frame.transition(node.normal, pr::state::shader_resource, pr::shader::pixel);
        arg_builder.add(node.normal);
    }
    if (p_arm)
    {
        node.ao_rough_metal = tex.load_texture_from_file(frame, p_arm, pr::format::rgba8un, true, false);
        frame.transition(node.ao_rough_metal, pr::state::shader_resource, pr::shader::pixel);
        arg_builder.add(node.ao_rough_metal);
    }

    ctx.submit(cc::move(frame));

    node.sv = arg_builder.make_graphics();

    return {res};
}

inc::pre::dmr::handle::material inc::pre::dmr::AssetPack::loadMaterial(pr::Context& ctx,
                                                                       inc::pre::texture_processing& tex,
                                                                       cc::span<const std::byte> d_albedo,
                                                                       cc::span<const std::byte> d_normal,
                                                                       cc::span<const std::byte> d_arm)
{
    auto frame = ctx.make_frame();

    auto const res = _materials.acquire();
    material_node& node = _materials.get(res);

    auto arg_builder = ctx.build_argument();

    if (d_albedo.size() > 0)
    {
        node.albedo = tex.load_texture_from_memory(frame, d_albedo, pr::format::rgba8un, true, true);
        frame.transition(node.albedo, pr::state::shader_resource, pr::shader::pixel);
        arg_builder.add(node.albedo);
    }
    if (d_normal.size() > 0)
    {
        node.normal = tex.load_texture_from_memory(frame, d_normal, pr::format::rgba8un, true, false);
        frame.transition(node.normal, pr::state::shader_resource, pr::shader::pixel);
        arg_builder.add(node.normal);
    }
    if (d_arm.size() > 0)
    {
        node.ao_rough_metal = tex.load_texture_from_memory(frame, d_arm, pr::format::rgba8un, true, false);
        frame.transition(node.ao_rough_metal, pr::state::shader_resource, pr::shader::pixel);
        arg_builder.add(node.ao_rough_metal);
    }

    ctx.submit(cc::move(frame));

    node.sv = arg_builder.make_graphics();

    return {res};
}
