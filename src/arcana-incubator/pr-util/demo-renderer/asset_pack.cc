#include "asset_pack.hh"

#include <phantasm-renderer/Context.hh>
#include <phantasm-renderer/Frame.hh>

#include <arcana-incubator/pr-util/texture_processing.hh>

inc::pre::dmr::handle::mesh inc::pre::dmr::AssetPack::loadMesh(pr::Context& ctx, const char* path, bool binary)
{
    auto const res = _meshes.acquire();
    _meshes.get(res) = inc::pre::load_mesh(ctx, path, binary);
    return {res};
}

inc::pre::dmr::handle::material inc::pre::dmr::AssetPack::loadMaterial(
    pr::Context& ctx, inc::pre::texture_processing& tex, const char* p_albedo, const char* p_normal, const char* p_arm)
{
    auto frame = ctx.make_frame();

    auto const res = _materials.acquire();
    material_node& node = _materials.get(res);

    node.albedo = tex.load_texture(frame, p_albedo, pr::format::rgba8un, true, true);
    node.normal = tex.load_texture(frame, p_normal, pr::format::rgba8un, true, false);
    node.ao_rough_metal = tex.load_texture(frame, p_arm, pr::format::rgba8un, true, false);

    frame.transition(node.albedo, pr::state::shader_resource, pr::shader::pixel);
    frame.transition(node.normal, pr::state::shader_resource, pr::shader::pixel);
    frame.transition(node.ao_rough_metal, pr::state::shader_resource, pr::shader::pixel);

    ctx.submit(cc::move(frame));

    node.sv = ctx.build_argument().add(node.albedo).add(node.normal).add(node.ao_rough_metal).make_graphics();

    return {res};
}
