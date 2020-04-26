#include "texture_processing.hh"

#include <clean-core/defer.hh>
#include <clean-core/utility.hh>

#include <phantasm-hardware-interface/detail/byte_util.hh>
#include <phantasm-hardware-interface/detail/format_size.hh>

#include <phantasm-renderer/Context.hh>
#include <phantasm-renderer/Frame.hh>
#include <phantasm-renderer/pass_info.hh>

#include <arcana-incubator/asset-loading/image_loader.hh>

#include "resource_loading.hh"

namespace
{
constexpr unsigned ceil_to_2_5mb(unsigned bytes)
{
    constexpr unsigned multiple = 2'500'000;
    return ((bytes + multiple - 1) / multiple) * multiple;
}

}

void inc::pre::texture_processing::init(pr::Context& ctx, const char* path_prefix)
{
    {
        auto [cs_mipgen, b1] = load_shader(ctx, "mipgen", phi::shader_stage::compute, path_prefix);
        auto [cs_mipgen_gamma, b2] = load_shader(ctx, "mipgen_gamma", phi::shader_stage::compute, path_prefix);
        auto [cs_mipgen_array, b3] = load_shader(ctx, "mipgen_array", phi::shader_stage::compute, path_prefix);

        pso_mipgen = ctx.make_pipeline_state(pr::compute_pass(cs_mipgen).arg(1, 1));
        pso_mipgen_gamma = ctx.make_pipeline_state(pr::compute_pass(cs_mipgen_gamma).arg(1, 1));
        pso_mipgen_array = ctx.make_pipeline_state(pr::compute_pass(cs_mipgen_array).arg(1, 1));
    }
    {
        auto [cs_equirect_cube, b4] = load_shader(ctx, "equirect_to_cube", phi::shader_stage::compute, path_prefix);
        auto [cs_spec_filter, b5] = load_shader(ctx, "specular_map_filter", phi::shader_stage::compute, path_prefix);
        auto [cs_irr_filter, b6] = load_shader(ctx, "irradiance_map_filter", phi::shader_stage::compute, path_prefix);
        auto [cs_lut_gen, b7] = load_shader(ctx, "brdf_lut_gen", phi::shader_stage::compute, path_prefix);

        pso_equirect_to_cube = ctx.make_pipeline_state(pr::compute_pass(cs_equirect_cube).arg(1, 1, 1));
        pso_specular_map_filter = ctx.make_pipeline_state(pr::compute_pass(cs_spec_filter).arg(1, 1, 1).enable_constants());
        pso_irradiance_map_gen = ctx.make_pipeline_state(pr::compute_pass(cs_irr_filter).arg(1, 1, 1));
        pso_brdf_lut_gen = ctx.make_pipeline_state(pr::compute_pass(cs_lut_gen).arg(0, 1));
    }
}

void inc::pre::texture_processing::free()
{
    pso_mipgen.free();
    pso_mipgen_gamma.free();
    pso_mipgen_array.free();
    pso_equirect_to_cube.free();
    pso_specular_map_filter.free();
    pso_irradiance_map_gen.free();
    pso_brdf_lut_gen.free();
}

pr::auto_texture inc::pre::texture_processing::load_texture(pr::raii::Frame& frame, const char* path, phi::format fmt, bool mips, bool gamma)
{
    CC_ASSERT((gamma ? mips : true) && "gamma setting meaningless without mipmap generation");

    inc::assets::image_size img_size;
    inc::assets::image_data img_data;
    {
        unsigned const num_components = phi::detail::format_num_components(fmt);
        bool const is_hdr = phi::detail::format_size_bytes(fmt) / num_components > 1;
        img_data = inc::assets::load_image(path, img_size, static_cast<int>(num_components), is_hdr);
        CC_RUNTIME_ASSERT(inc::assets::is_valid(img_data) && "failed to load texture");
    }
    CC_DEFER { inc::assets::free(img_data); };


    auto res = frame.context().make_texture({int(img_size.width), int(img_size.height)}, fmt, mips ? img_size.num_mipmaps : 1, true);

    // ceil size to 2.5MB to make cache hits more likely in the future
    unsigned const ceiled_upload_size = ceil_to_2_5mb(frame.context().calculate_texture_upload_size(res, 1));

    // get a cached upload buffer so we can just drop it
    auto b_upload = frame.context().get_upload_buffer(ceiled_upload_size);

    frame.upload_texture_data(static_cast<std::byte const*>(img_data.raw), b_upload, res);

    if (mips)
        generate_mips(frame, res, gamma);

    return res;
}

void inc::pre::texture_processing::generate_mips(pr::raii::Frame& frame, const pr::texture& texture, bool apply_gamma)
{
    constexpr auto max_array_size = 16u;
    CC_ASSERT(texture.info.width == texture.info.height && "non-square textures unimplemented");
    CC_ASSERT(phi::mem::is_power_of_two(texture.info.width) && "non-power of two textures unimplemented");

    pr::compute_pipeline_state matching_pso;

    if (texture.info.depth_or_array_size > 1)
    {
        CC_ASSERT(!apply_gamma && "gamma mipmap generation for arrays unimplemented");
        matching_pso = pso_mipgen_array;
    }
    else
    {
        matching_pso = apply_gamma ? pso_mipgen_gamma : pso_mipgen;
    }

    frame.transition(texture, phi::resource_state::shader_resource, phi::shader_stage::compute);

    auto pass = frame.make_pass(matching_pso);

    unsigned const num_mips
        = texture.info.num_mips > 0 ? texture.info.num_mips : frame.context().calculate_num_mip_levels({texture.info.width, texture.info.height});
    for (auto level = 1u, levelWidth = unsigned(texture.info.width) / 2, levelHeight = unsigned(texture.info.height) / 2; //
         level < num_mips;                                                                                                //
         ++level, levelWidth /= 2, levelHeight /= 2)
    {
        cc::capped_vector<phi::cmd::transition_image_slices::slice_transition_info, max_array_size> pre_dispatch;
        cc::capped_vector<phi::cmd::transition_image_slices::slice_transition_info, max_array_size> post_dispatch;

        for (auto arraySlice = 0u; arraySlice < texture.info.depth_or_array_size; ++arraySlice)
        {
            pre_dispatch.push_back(phi::cmd::transition_image_slices::slice_transition_info{
                texture.res.handle, phi::resource_state::shader_resource, phi::resource_state::unordered_access, phi::shader_stage::compute,
                phi::shader_stage::compute, int(level), int(arraySlice)});
            post_dispatch.push_back(phi::cmd::transition_image_slices::slice_transition_info{
                texture.res.handle, phi::resource_state::unordered_access, phi::resource_state::shader_resource, phi::shader_stage::compute,
                phi::shader_stage::compute, int(level), int(arraySlice)});
        }

        pr::argument arg;
        arg.add(pr::resource_view_2d(texture, level - 1, 1).tex_array(0, texture.info.depth_or_array_size));
        arg.add_mutable(pr::resource_view_2d(texture, level, 1).tex_array(0, texture.info.depth_or_array_size));

        frame.transition_slices(pre_dispatch);

        pass.bind(arg).dispatch(cc::max(1u, levelWidth / 8), cc::max(1u, levelHeight / 8), texture.info.depth_or_array_size);

        frame.transition_slices(post_dispatch);
    }
}

inc::pre::filtered_specular_result inc::pre::texture_processing::load_filtered_specular_map(pr::raii::Frame& frame, const char* hdr_equirect_path)
{
    constexpr auto cube_width = 1024;
    constexpr auto cube_height = 1024;
    auto const cube_num_mips = inc::assets::get_num_mip_levels(cube_width, cube_height);

    filtered_specular_result res;
    res.equirect_tex = load_texture(frame, hdr_equirect_path, phi::format::rgba32f, false);
    res.unfiltered_env = frame.context().make_texture_cube({cube_width, cube_height}, pr::format::rgba16f, cube_num_mips, true);
    res.filtered_env = frame.context().make_texture_cube({cube_width, cube_height}, pr::format::rgba16f, cube_num_mips, true);

    // equirect to cubemap
    {
        frame.transition(res.equirect_tex, phi::resource_state::shader_resource, phi::shader_stage::compute);
        frame.transition(res.unfiltered_env, phi::resource_state::unordered_access, phi::shader_stage::compute);

        pr::argument arg;
        arg.add(res.equirect_tex);
        arg.add_mutable(res.unfiltered_env);
        arg.add_sampler(phi::sampler_filter::min_mag_mip_linear);

        frame.make_pass(pso_equirect_to_cube).bind(arg).dispatch(cube_width / 32, cube_height / 32, 6);
    }

    // mips
    generate_mips(frame, res.unfiltered_env, false);

    // prefilter specular
    {
        frame.copy(res.unfiltered_env, res.filtered_env);

        frame.transition(res.unfiltered_env, phi::resource_state::shader_resource, phi::shader_stage::compute);
        frame.transition(res.filtered_env, phi::resource_state::unordered_access, phi::shader_stage::compute);

        const float deltaRoughness = 1.0f / cc::max(float(cube_num_mips - 1), 1.0f);
        for (auto level = 1u, size = 512u; level < cube_num_mips; ++level, size /= 2)
        {
            auto const num_groups = cc::max<unsigned>(1, size / 32);
            float const spmapRoughness = level * deltaRoughness;

            pr::argument arg;
            arg.add(res.unfiltered_env);
            arg.add_mutable(pr::resource_view_cube(res.filtered_env).mips(level, 1));
            arg.add_sampler(phi::sampler_filter::min_mag_mip_linear);

            auto pass = frame.make_pass(pso_specular_map_filter).bind(arg);
            pass.write_constants(spmapRoughness);
            pass.dispatch(num_groups, num_groups, 6);
        }
    }

    return res;
}

pr::auto_texture inc::pre::texture_processing::create_diffuse_irradiance_map(pr::raii::Frame& frame, const pr::texture& filtered_specular)
{
    constexpr auto cube_width = 32u;
    constexpr auto cube_height = 32u;

    auto t_irradiance = frame.context().make_texture_cube({cube_width, cube_height}, pr::format::rgba16f, 1, true);

    frame.transition(t_irradiance, phi::resource_state::unordered_access, phi::shader_stage::compute);
    frame.transition(filtered_specular, phi::resource_state::shader_resource, phi::shader_stage::compute);

    pr::argument arg;
    arg.add(filtered_specular);
    arg.add_mutable(t_irradiance);
    arg.add_sampler(phi::sampler_filter::min_mag_mip_linear);

    frame.make_pass(pso_irradiance_map_gen).bind(arg).dispatch(cube_width / 32, cube_height / 32, 6);

    return t_irradiance;
}

pr::auto_texture inc::pre::texture_processing::create_brdf_lut(pr::raii::Frame& frame, int width_height)
{
    auto t_lut = frame.context().make_texture({width_height, width_height}, pr::format::rg16f, 1, true);

    frame.transition(t_lut, phi::resource_state::unordered_access, phi::shader_stage::compute);

    pr::argument arg;
    arg.add_mutable(t_lut);

    frame.make_pass(pso_brdf_lut_gen).bind(arg).dispatch(unsigned(width_height) / 32, unsigned(width_height) / 32, 1);

    return t_lut;
}
