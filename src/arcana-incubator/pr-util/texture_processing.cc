#include "texture_processing.hh"

#include <clean-core/bits.hh>
#include <clean-core/defer.hh>
#include <clean-core/utility.hh>

#include <phantasm-hardware-interface/common/byte_util.hh>
#include <phantasm-hardware-interface/common/format_size.hh>
#include <phantasm-hardware-interface/util.hh>

#include <phantasm-renderer/Context.hh>
#include <phantasm-renderer/Frame.hh>
#include <phantasm-renderer/pass_info.hh>

#include <arcana-incubator/asset-loading/image_loader.hh>

#include "resource_loading.hh"

void inc::pre::texture_processing::init(pr::Context& ctx, const char* path_prefix, char const* file_ending_override)
{
    {
        auto [cs_mipgen, b1] = load_shader(ctx, "mipgen", phi::shader_stage::compute, path_prefix, file_ending_override);
        auto [cs_mipgen_gamma, b2] = load_shader(ctx, "mipgen_gamma", phi::shader_stage::compute, path_prefix, file_ending_override);
        auto [cs_mipgen_array, b3] = load_shader(ctx, "mipgen_array", phi::shader_stage::compute, path_prefix, file_ending_override);

        pso_mipgen = ctx.make_pipeline_state(pr::compute_pass(cs_mipgen).arg(1, 1));
        pso_mipgen_gamma = ctx.make_pipeline_state(pr::compute_pass(cs_mipgen_gamma).arg(1, 1));
        pso_mipgen_array = ctx.make_pipeline_state(pr::compute_pass(cs_mipgen_array).arg(1, 1));
    }
    {
        auto [cs_equirect_cube, b4] = load_shader(ctx, "equirect_to_cube", phi::shader_stage::compute, path_prefix, file_ending_override);
        auto [cs_spec_filter, b5] = load_shader(ctx, "specular_map_filter", phi::shader_stage::compute, path_prefix, file_ending_override);
        auto [cs_irr_filter, b6] = load_shader(ctx, "irradiance_map_filter", phi::shader_stage::compute, path_prefix, file_ending_override);
        auto [cs_lut_gen, b7] = load_shader(ctx, "brdf_lut_gen", phi::shader_stage::compute, path_prefix, file_ending_override);

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

pr::auto_texture inc::pre::texture_processing::load_texture_from_memory(pr::raii::Frame& frame, cc::span<const std::byte> data, phi::format fmt, bool mips, bool gamma)
{
    inc::assets::image_size img_size;
    inc::assets::image_data img_data;
    {
        unsigned const num_components = phi::util::get_format_num_components(fmt);
        bool const is_hdr = phi::util::get_format_size_bytes(fmt) / num_components > 1;
        img_data = inc::assets::load_image(data, img_size, int(num_components), is_hdr);
        CC_RUNTIME_ASSERT(inc::assets::is_valid(img_data) && "failed to load texture from memory");
    }
    auto res = load_texture(frame, img_data, img_size, fmt, mips, gamma);
    inc::assets::free(img_data);
    return res;
}

pr::auto_texture inc::pre::texture_processing::load_texture_from_file(pr::raii::Frame& frame, const char* path, pr::format fmt, bool mips, bool gamma)
{
    inc::assets::image_size img_size;
    inc::assets::image_data img_data;
    {
        unsigned const num_components = phi::util::get_format_num_components(fmt);
        bool const is_hdr = phi::util::get_format_size_bytes(fmt) / num_components > 1;
        img_data = inc::assets::load_image(path, img_size, int(num_components), is_hdr);
        CC_RUNTIME_ASSERT(inc::assets::is_valid(img_data) && "failed to load texture from file");
    }
    auto res = load_texture(frame, img_data, img_size, fmt, mips, gamma);
    inc::assets::free(img_data);
    return res;
}

pr::auto_texture inc::pre::texture_processing::load_texture(
    pr::raii::Frame& frame, const inc::assets::image_data& data, const inc::assets::image_size& size, pr::format fmt, bool mips, bool gamma)
{
    CC_ASSERT((gamma ? mips : true) && "gamma setting meaningless without mipmap generation");
    auto _label = frame.scoped_debug_label("texture_processing - load texture");

    auto res = frame.context().make_texture({int(size.width), int(size.height)}, fmt, mips ? size.num_mipmaps : 1, true);

    frame.auto_upload_texture_data(cc::span{static_cast<std::byte const*>(data.raw), data.raw_size_bytes}, res);

    if (mips)
        generate_mips(frame, res, gamma);

    return res;
}

void inc::pre::texture_processing::generate_mips(pr::raii::Frame& frame, const pr::texture& texture, bool apply_gamma)
{
    constexpr auto max_array_size = 16u;

    auto const& texInfo = frame.context().get_texture_info(texture);
    CC_ASSERT(texInfo.width == texInfo.height && "non-square textures unimplemented");
    CC_ASSERT(cc::is_pow2(unsigned(texInfo.width)) && "non-power of two textures unimplemented");

    auto _label = frame.scoped_debug_label("texture_processing - generate mips");

    pr::compute_pipeline_state matching_pso;

    if (texInfo.depth_or_array_size > 1)
    {
        CC_ASSERT(!apply_gamma && "gamma mipmap generation for arrays unimplemented");
        matching_pso = pso_mipgen_array;
    }
    else
    {
        matching_pso = apply_gamma ? pso_mipgen_gamma : pso_mipgen;
    }

    frame.transition(texture, pr::state::shader_resource, phi::shader_stage_flags::compute);

    auto pass = frame.make_pass(matching_pso);

    unsigned const num_mips = texInfo.num_mips > 0 ? texInfo.num_mips : phi::util::get_num_mips(texInfo.width, texInfo.height);
    for (auto level = 1u, levelWidth = unsigned(texInfo.width) / 2, levelHeight = unsigned(texInfo.height) / 2; //
         level < num_mips;                                                                                      //
         ++level, levelWidth /= 2, levelHeight /= 2)
    {
        cc::capped_vector<phi::cmd::transition_image_slices::slice_transition_info, max_array_size> pre_dispatch;
        cc::capped_vector<phi::cmd::transition_image_slices::slice_transition_info, max_array_size> post_dispatch;

        for (auto arraySlice = 0u; arraySlice < texInfo.depth_or_array_size; ++arraySlice)
        {
            pre_dispatch.push_back(phi::cmd::transition_image_slices::slice_transition_info{
                texture.handle, pr::state::shader_resource, pr::state::unordered_access, phi::shader_stage_flags::compute,
                phi::shader_stage_flags::compute, int(level), int(arraySlice)});
            post_dispatch.push_back(phi::cmd::transition_image_slices::slice_transition_info{
                texture.handle, pr::state::unordered_access, pr::state::shader_resource, phi::shader_stage_flags::compute,
                phi::shader_stage_flags::compute, int(level), int(arraySlice)});
        }

        pr::argument arg;
        arg.add(pr::view::tex2d_array(texture, texInfo.fmt, 0, texInfo.depth_or_array_size, false, level - 1, 1));
        arg.add_mutable(pr::view::tex2d_array(texture, texInfo.fmt, 0, texInfo.depth_or_array_size, false, level, 1));

        frame.transition_slices(pre_dispatch);

        pass.bind(arg).dispatch(cc::max(1u, levelWidth / 8), cc::max(1u, levelHeight / 8), texInfo.depth_or_array_size);

        frame.transition_slices(post_dispatch);
    }
}

inc::pre::filtered_specular_result inc::pre::texture_processing::load_filtered_specular_map_from_memory(pr::raii::Frame& frame,
                                                                                                        cc::span<const std::byte> data,
                                                                                                        int cube_width_height)
{
    auto tex_specular_map = load_texture_from_memory(frame, data, phi::format::rgba32f, false);
    return load_filtered_specular_map(frame, cc::move(tex_specular_map), cube_width_height);
}

inc::pre::filtered_specular_result inc::pre::texture_processing::load_filtered_specular_map_from_file(pr::raii::Frame& frame, const char* hdr_equirect_path, int cube_width_height)
{
    auto tex_specular_map = load_texture_from_file(frame, hdr_equirect_path, phi::format::rgba32f, false);
    return load_filtered_specular_map(frame, cc::move(tex_specular_map), cube_width_height);
}

inc::pre::filtered_specular_result inc::pre::texture_processing::load_filtered_specular_map(pr::raii::Frame& frame, pr::auto_texture&& specular_map, int cube_width_height)
{
    CC_ASSERT(cc::is_pow2(uint32_t(cube_width_height)) && cube_width_height >= 32 && "cube size invalid");
    int const cube_num_mips = phi::util::get_num_mips(cube_width_height, cube_width_height);

    filtered_specular_result res;
    res.equirect_tex = cc::move(specular_map);
    res.unfiltered_env = frame.context().make_texture_cube({cube_width_height, cube_width_height}, pr::format::rgba16f, cube_num_mips, true);
    res.filtered_env = frame.context().make_texture_cube({cube_width_height, cube_width_height}, pr::format::rgba16f, cube_num_mips, true);

    // equirect to cubemap
    {
        auto _label = frame.scoped_debug_label("texture_processing - load_filtered_specular_map - equirect to cubemap");
        frame.transition(res.equirect_tex, pr::state::shader_resource, phi::shader_stage_flags::compute);
        frame.transition(res.unfiltered_env, pr::state::unordered_access, phi::shader_stage_flags::compute);

        pr::argument arg;
        arg.add(res.equirect_tex);
        arg.add_mutable(res.unfiltered_env);
        arg.add_sampler(phi::sampler_filter::min_mag_mip_linear);

        frame.make_pass(pso_equirect_to_cube).bind(arg).dispatch(cube_width_height / 32, cube_width_height / 32, 6);
    }

    // mips
    generate_mips(frame, res.unfiltered_env, false);

    // prefilter specular
    {
        auto _label = frame.scoped_debug_label("texture_processing - load_filtered_specular_map - prefilter specular");
        frame.copy(res.unfiltered_env, res.filtered_env, 0);

        frame.transition(res.unfiltered_env, pr::state::shader_resource, phi::shader_stage_flags::compute);
        frame.transition(res.filtered_env, pr::state::unordered_access, phi::shader_stage_flags::compute);

        for (auto level = 1, size = 512; level < cube_num_mips; ++level, size /= 2)
        {
            auto const num_groups = cc::max<unsigned>(1, cc::int_div_ceil(size, 32));

            pr::argument arg;
            arg.add(res.unfiltered_env);
            arg.add_mutable(pr::view::texcube(res.filtered_env.data.handle, pr::format::rgba16f, level, 1));
            arg.add_sampler(phi::sampler_filter::min_mag_mip_linear);

            auto pass = frame.make_pass(pso_specular_map_filter).bind(arg);

            struct
            {
                uint32_t mip;
                uint32_t num_mips;
            } constants = {uint32_t(level), uint32_t(cube_num_mips)};

            pass.write_constants(constants);
            pass.dispatch(num_groups, num_groups, 6);
        }
    }

    return res;
}

pr::auto_texture inc::pre::texture_processing::create_diffuse_irradiance_map(pr::raii::Frame& frame, const pr::texture& unfiltered_env_cube, int cube_width_height)
{
    CC_ASSERT(cube_width_height > 0 && cube_width_height % 32 == 0 && "invalid cubemap size");

    auto _label = frame.scoped_debug_label("texture_processing - create_diffuse_irradiance_map");
    auto t_irradiance = frame.context().make_texture_cube({cube_width_height, cube_width_height}, pr::format::rgba16f, 1, true);

    frame.transition(t_irradiance, pr::state::unordered_access, phi::shader_stage_flags::compute);
    frame.transition(unfiltered_env_cube, pr::state::shader_resource_nonpixel, phi::shader_stage_flags::compute);

    pr::argument arg;
    arg.add(unfiltered_env_cube);
    arg.add_mutable(t_irradiance);
    arg.add_sampler(phi::sampler_filter::min_mag_mip_linear);

    frame.make_pass(pso_irradiance_map_gen).bind(arg).dispatch(cube_width_height / 32, cube_width_height / 32, 6);

    return t_irradiance;
}

pr::auto_texture inc::pre::texture_processing::create_brdf_lut(pr::raii::Frame& frame, int width, int height)
{
    CC_ASSERT(width % 32 == 0 && "BRDF LUT size must be divisible by 32");
    CC_ASSERT(height % 32 == 0 && "BRDF LUT size must be divisible by 32");

    auto t_lut = frame.context().make_texture({width, height}, pr::format::rgba16un, 1, true);

    frame.transition(t_lut, pr::state::unordered_access, phi::shader_stage_flags::compute);

    pr::argument arg;
    arg.add_mutable(t_lut);

    frame.make_pass(pso_brdf_lut_gen).bind(arg).dispatch(unsigned(width) / 32, unsigned(height) / 32, 1);

    return t_lut;
}
