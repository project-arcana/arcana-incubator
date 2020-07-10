#pragma once

#include <phantasm-hardware-interface/detail/linked_pool.hh>

#include <phantasm-renderer/argument.hh>
#include <phantasm-renderer/resource_types.hh>

#include <arcana-incubator/pr-util/resource_loading.hh>

#include "types.hh"

namespace inc::pre
{
struct texture_processing;
}

namespace inc::pre::dmr
{
struct AssetPack
{
public:
    AssetPack() = default;
    explicit AssetPack(unsigned max_num_meshes, unsigned max_num_materials) { initialize(max_num_meshes, max_num_materials); }

    AssetPack(AssetPack const&) = delete;
    AssetPack(AssetPack&&) = delete;

    ~AssetPack() { destroy(); }

    void initialize(unsigned max_num_meshes = 1024, unsigned max_num_materials = 1024)
    {
        _meshes.initialize(max_num_meshes);
        _materials.initialize(max_num_materials);
    }

    void destroy()
    {
        _meshes.release_all();
        _meshes.destroy();
        _materials.release_all();
        _materials.destroy();
    }

    [[nodiscard]] handle::mesh loadMesh(pr::Context& ctx, char const* path, bool binary = false);
    [[nodiscard]] handle::mesh loadMesh(pr::Context& ctx, cc::span<std::byte const> data);

    [[nodiscard]] handle::material loadMaterial(pr::Context& ctx, inc::pre::texture_processing& tex, char const* p_albedo, char const* p_normal, char const* p_arm);
    [[nodiscard]] handle::material loadMaterial(pr::Context& ctx,
                                                inc::pre::texture_processing& tex,
                                                cc::span<std::byte const> d_albedo,
                                                cc::span<std::byte const> d_normal,
                                                cc::span<std::byte const> d_arm);

    pr::prebuilt_argument const& getMaterial(handle::material mat) const { return _materials.get(mat._value).sv; }

    inc::pre::pr_mesh const& getMesh(handle::mesh mesh) const { return _meshes.get(mesh._value); }

    void free(handle::mesh m) { _meshes.release(m._value); }

    void free(handle::material m) { _materials.release(m._value); }

    void freeAll()
    {
        _meshes.release_all();
        _materials.release_all();
    }

private:
    struct material_node
    {
        pr::auto_texture albedo;
        pr::auto_texture normal;
        pr::auto_texture ao_rough_metal;
        pr::auto_prebuilt_argument sv;
    };

    phi::detail::linked_pool<material_node> _materials;
    phi::detail::linked_pool<inc::pre::pr_mesh> _meshes;
};

}
