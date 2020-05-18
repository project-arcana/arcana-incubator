#include "texture_creation.hh"

#include <iostream>

#include <clean-core/capped_vector.hh>
#include <clean-core/defer.hh>
#include <clean-core/utility.hh>

#include <phantasm-hardware-interface/commands.hh>
#include <phantasm-hardware-interface/detail/byte_util.hh>
#include <phantasm-hardware-interface/detail/format_size.hh>
#include <phantasm-hardware-interface/util.hh>

#include <arcana-incubator/phi-util/shader_util.hh>
#include <arcana-incubator/phi-util/texture_util.hh>

using namespace phi;

namespace
{
constexpr auto gc_ibl_cubemap_format = format::rgba16f;
}


void inc::texture_creator::initialize(phi::Backend& backend, char const* shader_path)
{
    char const* const shader_ending = backend.getBackendType() == phi::backend_type::d3d12 ? "dxil" : "spv";

    resources_to_free.reserve(1000);
    shader_views_to_free.reserve(1000);
    pending_cmd_lists.reserve(100);
    align_mip_rows = backend.getBackendType() == phi::backend_type::d3d12;
    this->backend = &backend;

    // create command stream buffer
    {
        auto const buffer_size = 1024ull * 32;
        commandstream_buffer = static_cast<std::byte*>(std::malloc(buffer_size));
        cmd_writer.initialize(commandstream_buffer, buffer_size);
    }

    // load mip generation shaders
    {
        cc::capped_vector<arg::shader_arg_shape, 1> shader_payload;
        {
            arg::shader_arg_shape shape;
            shape.has_cbv = false;
            shape.num_srvs = 1;
            shape.num_uavs = 1;
            shape.num_samplers = 0;
            shader_payload.push_back(shape);
        }

        auto const sb_mipgen = get_shader_binary(shader_path, "mipgen", shader_ending);
        auto const sb_mipgen_gamma = get_shader_binary(shader_path, "mipgen_gamma", shader_ending);
        auto const sb_mipgen_array = get_shader_binary(shader_path, "mipgen_array", shader_ending);

        CC_RUNTIME_ASSERT(sb_mipgen.is_valid() && sb_mipgen_gamma.is_valid() && sb_mipgen_array.is_valid() && "failed to load shaders");

        pso_mipgen = backend.createComputePipelineState(shader_payload, {sb_mipgen.get(), sb_mipgen.size()});
        pso_mipgen_gamma = backend.createComputePipelineState(shader_payload, {sb_mipgen_gamma.get(), sb_mipgen_gamma.size()});
        pso_mipgen_array = backend.createComputePipelineState(shader_payload, {sb_mipgen_array.get(), sb_mipgen_array.size()});
    }

    // load IBL preparation shaders
    {
        auto const sb_equirect_cube = get_shader_binary(shader_path, "equirect_to_cube", shader_ending);
        auto const sb_specular_map_filter = get_shader_binary(shader_path, "specular_map_filter", shader_ending);
        auto const sb_irradiance_map_filter = get_shader_binary(shader_path, "irradiance_map_filter", shader_ending);
        auto const sb_brdf_lut_gen = get_shader_binary(shader_path, "brdf_lut_gen", shader_ending);

        CC_RUNTIME_ASSERT(sb_equirect_cube.is_valid() && sb_specular_map_filter.is_valid() && sb_irradiance_map_filter.is_valid()
                          && sb_brdf_lut_gen.is_valid() && "failed to load shaders");

        cc::capped_vector<arg::shader_arg_shape, 1> arg_shape;
        {
            arg::shader_arg_shape shape;
            shape.has_cbv = false;
            shape.num_srvs = 1;
            shape.num_uavs = 1;
            shape.num_samplers = 1;
            arg_shape.push_back(shape);
        }

        pso_equirect_to_cube = backend.createComputePipelineState(arg_shape, {sb_equirect_cube.get(), sb_equirect_cube.size()});

        pso_specular_map_filter = backend.createComputePipelineState(arg_shape, {sb_specular_map_filter.get(), sb_specular_map_filter.size()}, true);

        pso_irradiance_map_gen = backend.createComputePipelineState(arg_shape, {sb_irradiance_map_filter.get(), sb_irradiance_map_filter.size()});

        cc::capped_vector<arg::shader_arg_shape, 1> arg_shape_single_uav;
        {
            arg::shader_arg_shape shape = {};
            shape.num_uavs = 1;
            arg_shape_single_uav.push_back(shape);
        }

        pso_brdf_lut_gen = backend.createComputePipelineState(arg_shape_single_uav, {sb_brdf_lut_gen.get(), sb_brdf_lut_gen.size()});
    }
}

void inc::texture_creator::free(Backend& backend)
{
    flush_cmdstream(true, true);

    std::free(commandstream_buffer);

    backend.free(pso_mipgen);
    backend.free(pso_mipgen_gamma);
    backend.free(pso_mipgen_array);

    backend.free(pso_equirect_to_cube);
    backend.free(pso_specular_map_filter);
    backend.free(pso_irradiance_map_gen);
    backend.free(pso_brdf_lut_gen);
}

handle::resource inc::texture_creator::load_texture(char const* path, phi::format format, bool include_mipmaps, bool apply_gamma)
{
    CC_ASSERT((apply_gamma ? include_mipmaps : true) && "gamma setting meaningless without mipmap generation");

    flush_cmdstream(true, false);

    inc::assets::image_size img_size;
    inc::assets::image_data img_data;
    {
        auto const num_components = detail::format_num_components(format);
        auto const is_hdr = detail::format_size_bytes(format) / num_components > 1;
        img_data = inc::assets::load_image(path, img_size, static_cast<int>(num_components), is_hdr);
    }
    CC_DEFER { inc::assets::free(img_data); };

    CC_RUNTIME_ASSERT(inc::assets::is_valid(img_data) && "failed to load texture");

    auto const res_handle = backend->createTexture(format, {int(img_size.width), int(img_size.height)}, include_mipmaps ? img_size.num_mipmaps : 1,
                                                   texture_dimension::t2d, 1, true);

    auto const upbuff_handle = backend->createUploadBuffer(inc::get_mipmap_upload_size(format, img_size, true));
    resources_to_free.push_back(upbuff_handle);

    cmd_writer.add_command(cmd::begin_debug_label{"load_texture"});

    {
        cmd::transition_resources transition_cmd;
        transition_cmd.add(res_handle, resource_state::copy_dest);
        cmd_writer.add_command(transition_cmd);
    }

    inc::copy_data_to_texture(cmd_writer, upbuff_handle, backend->mapBuffer(upbuff_handle), res_handle, format, img_size.width, img_size.height,
                              static_cast<std::byte const*>(img_data.raw), align_mip_rows);

    if (include_mipmaps)
        generate_mips(res_handle, img_size, apply_gamma, format);

    // make writes to the upload buffer visible
    backend->unmapBuffer(upbuff_handle);

    cmd_writer.add_command(cmd::end_debug_label{});

    return res_handle;
}

handle::resource inc::texture_creator::load_filtered_specular_map(const char* hdr_equirect_path)
{
    cmd_writer.add_command(cmd::begin_debug_label{"load_filtered_specular_map"});

    constexpr auto cube_width = 1024u;
    constexpr auto cube_height = 1024u;
    auto const cube_num_mips = phi::util::get_num_mips(cube_width, cube_height);

    // this call full flushes, no need to do it ourselves
    auto const equirect_handle = load_texture(hdr_equirect_path, format::rgba32f, false);
    resources_to_free.push_back(equirect_handle);

    auto const unfiltered_env_handle = backend->createTexture(gc_ibl_cubemap_format, {cube_width, cube_height}, cube_num_mips, texture_dimension::t2d, 6, true);
    resources_to_free.push_back(unfiltered_env_handle);

    auto const filtered_env_handle = backend->createTexture(gc_ibl_cubemap_format, {cube_width, cube_height}, cube_num_mips, texture_dimension::t2d, 6, true);

    // convert equirectangular map to cubemap
    {
        handle::shader_view sv;
        {
            resource_view sve_srv;
            sve_srv.init_as_tex2d(equirect_handle, format::rgba32f);

            resource_view sve_uav;
            sve_uav.init_as_texcube(unfiltered_env_handle, gc_ibl_cubemap_format);

            sampler_config srv_sampler;
            srv_sampler.init_default(sampler_filter::min_mag_mip_linear);

            sv = backend->createShaderView(cc::span{sve_srv}, cc::span{sve_uav}, cc::span{srv_sampler}, true);
            shader_views_to_free.push_back(sv);
        }

        // pre transition
        {
            cmd::transition_resources tcmd;
            tcmd.add(equirect_handle, resource_state::shader_resource, shader_stage::compute);
            tcmd.add(unfiltered_env_handle, resource_state::unordered_access, shader_stage::compute);
            cmd_writer.add_command(tcmd);
        }

        // compute dispatch
        {
            cmd::dispatch dcmd;
            dcmd.init(pso_equirect_to_cube, cube_width / 32, cube_height / 32, 6);
            dcmd.add_shader_arg(handle::null_resource, 0, sv);
            cmd_writer.add_command(dcmd);
        }
    }

    // generate mipmaps for the unfiltered envmap
    inc::assets::image_size env_size = {cube_width, cube_height, 0, 6};
    generate_mips(unfiltered_env_handle, env_size, false, gc_ibl_cubemap_format);

    // generate pre-filtered specular cubemap
    {
        // pre transition
        {
            cmd::transition_resources tcmd;
            tcmd.add(unfiltered_env_handle, resource_state::copy_src);
            tcmd.add(filtered_env_handle, resource_state::copy_dest);
            cmd_writer.add_command(tcmd);
        }

        // copy 0th mip to filtered cubemap
        {
            cmd::copy_texture ccmd;
            ccmd.init_symmetric(unfiltered_env_handle, filtered_env_handle, cube_width, cube_height, 0, 0, 6);
            cmd_writer.add_command(ccmd);
        }

        // post transition
        {
            cmd::transition_resources tcmd;
            tcmd.add(unfiltered_env_handle, resource_state::shader_resource, shader_stage::compute);
            tcmd.add(filtered_env_handle, resource_state::unordered_access, shader_stage::compute);
            cmd_writer.add_command(tcmd);
        }

        // compute dispatch
        {
            resource_view sve_srv;
            sve_srv.init_as_texcube(unfiltered_env_handle, gc_ibl_cubemap_format);

            resource_view sve_uav;
            sve_uav.init_as_texcube(filtered_env_handle, gc_ibl_cubemap_format);
            sve_uav.texture_info.mip_size = 1;

            sampler_config default_sampler;
            default_sampler.init_default(sampler_filter::min_mag_mip_linear);


            const float deltaRoughness = 1.0f / cc::max(float(cube_num_mips - 1), 1.0f);
            for (auto level = 1u, size = 512u; level < cube_num_mips; ++level, size /= 2)
            {
                auto const num_groups = cc::max<unsigned>(1, size / 32);
                float const spmapRoughness = level * deltaRoughness;

                sve_uav.texture_info.mip_start = level;
                auto const sv = backend->createShaderView(cc::span{sve_srv}, cc::span{sve_uav}, cc::span{default_sampler}, true);
                shader_views_to_free.push_back(sv);

                cmd::dispatch dcmd;
                dcmd.init(pso_specular_map_filter, num_groups, num_groups, 6);
                dcmd.add_shader_arg(handle::null_resource, 0, sv);
                dcmd.write_root_constants(spmapRoughness);
                cmd_writer.add_command(dcmd);
            }
        }
    }

    cmd_writer.add_command(cmd::end_debug_label{});

    return filtered_env_handle;
}

handle::resource inc::texture_creator::create_diffuse_irradiance_map(handle::resource filtered_specular_map)
{
    flush_cmdstream(true, false);
    constexpr auto cube_width = 32u;
    constexpr auto cube_height = 32u;

    auto const irradiance_map_handle = backend->createTexture(gc_ibl_cubemap_format, {cube_width, cube_height}, 1, texture_dimension::t2d, 6, true);

    cmd_writer.add_command(cmd::begin_debug_label{"create_diffuse_irradiance_map"});
    // prepare for UAV
    {
        cmd::transition_resources tcmd;
        tcmd.add(irradiance_map_handle, resource_state::unordered_access, shader_stage::compute);
        tcmd.add(filtered_specular_map, resource_state::shader_resource, shader_stage::compute);
        cmd_writer.add_command(tcmd);
    }

    // dispatch
    {
        resource_view sve_uav;
        sve_uav.init_as_texcube(irradiance_map_handle, gc_ibl_cubemap_format);

        resource_view sve_srv;
        sve_srv.init_as_texcube(filtered_specular_map, gc_ibl_cubemap_format);

        sampler_config default_sampler;
        default_sampler.init_default(sampler_filter::min_mag_mip_linear);

        auto const sv = backend->createShaderView(cc::span{sve_srv}, cc::span{sve_uav}, cc::span{default_sampler}, true);
        shader_views_to_free.push_back(sv);

        cmd::dispatch dcmd;
        dcmd.init(pso_irradiance_map_gen, cube_width / 32, cube_height / 32, 6);
        dcmd.add_shader_arg(handle::null_resource, 0, sv);
        cmd_writer.add_command(dcmd);
    }

    cmd_writer.add_command(cmd::end_debug_label{});

    return irradiance_map_handle;
}

handle::resource inc::texture_creator::create_brdf_lut(int width_height)
{
    flush_cmdstream(true, false);

    auto const brdf_lut_handle = backend->createTexture(format::rg16f, {width_height, width_height}, 1, texture_dimension::t2d, 1, true);

    cmd_writer.add_command(cmd::begin_debug_label{"create_brdf_lut"});
    // prepare for UAV
    {
        cmd::transition_resources tcmd;
        tcmd.add(brdf_lut_handle, resource_state::unordered_access, shader_stage::compute);
        cmd_writer.add_command(tcmd);
    }

    // dispatch
    {
        resource_view sve_uav;
        sve_uav.init_as_tex2d(brdf_lut_handle, format::rg16f);

        auto const sv = backend->createShaderView({}, cc::span{sve_uav}, {}, true);
        shader_views_to_free.push_back(sv);

        cmd::dispatch dcmd;
        dcmd.init(pso_brdf_lut_gen, width_height / 32, width_height / 32, 1);
        dcmd.add_shader_arg(handle::null_resource, 0, sv);
        cmd_writer.add_command(dcmd);
    }

    cmd_writer.add_command(cmd::end_debug_label{});


    return brdf_lut_handle;
}

void inc::texture_creator::generate_mips(handle::resource resource, const inc::assets::image_size& size, bool apply_gamma, format pf)
{
    constexpr auto max_array_size = 16u;
    CC_ASSERT(size.width == size.height && "non-square textures unimplemented");
    CC_ASSERT(phi::mem::is_power_of_two(size.width) && "non-power of two textures unimplemented");

    handle::pipeline_state matching_pso;
    if (size.array_size > 1)
    {
        CC_ASSERT(!apply_gamma && "gamma mipmap generation for arrays unimplemented");
        matching_pso = pso_mipgen_array;
    }
    else
    {
        matching_pso = apply_gamma ? pso_mipgen_gamma : pso_mipgen;
    }

    auto const record_barriers = [this](cc::span<cmd::transition_image_slices::slice_transition_info const> slice_barriers) {
        cmd::transition_image_slices tcmd;

        for (auto const& ti : slice_barriers)
        {
            if (tcmd.transitions.size() == limits::max_resource_transitions)
            {
                cmd_writer.add_command(tcmd);
                tcmd.transitions.clear();
            }

            tcmd.transitions.push_back(ti);
        }

        if (!tcmd.transitions.empty())
            cmd_writer.add_command(tcmd);
    };


    cmd::transition_resources starting_tcmd;
    starting_tcmd.add(resource, resource_state::shader_resource, shader_stage::compute);
    cmd_writer.add_command(starting_tcmd);

    auto const num_mipmaps = size.num_mipmaps == 0 ? phi::util::get_num_mips(size.width, size.height) : size.num_mipmaps;

    for (auto level = 1u, levelWidth = size.width / 2, levelHeight = size.height / 2; level < num_mipmaps; ++level, levelWidth /= 2, levelHeight /= 2)
    {
        resource_view sve;
        sve.init_as_tex2d(resource, pf);
        sve.texture_info.mip_start = level - 1;
        sve.texture_info.mip_size = 1;
        if (size.array_size > 1)
        {
            sve.dimension = resource_view_dimension::texture2d_array;
            sve.texture_info.array_size = size.array_size;
        }

        resource_view sve_uav = sve;
        sve_uav.texture_info.mip_start = level;

        auto const sv = backend->createShaderView(cc::span{sve}, cc::span{sve_uav}, {}, true);
        shader_views_to_free.push_back(sv);

        cc::capped_vector<cmd::transition_image_slices::slice_transition_info, max_array_size> pre_dispatch;
        cc::capped_vector<cmd::transition_image_slices::slice_transition_info, max_array_size> post_dispatch;

        for (auto arraySlice = 0u; arraySlice < size.array_size; ++arraySlice)
        {
            pre_dispatch.push_back(cmd::transition_image_slices::slice_transition_info{resource, resource_state::shader_resource,
                                                                                       resource_state::unordered_access, shader_stage::compute,
                                                                                       shader_stage::compute, int(level), int(arraySlice)});
            post_dispatch.push_back(cmd::transition_image_slices::slice_transition_info{resource, resource_state::unordered_access,
                                                                                        resource_state::shader_resource, shader_stage::compute,
                                                                                        shader_stage::compute, int(level), int(arraySlice)});
        }

        // record pre-dispatch barriers
        record_barriers(pre_dispatch);

        // record compute dispatch
        cmd::dispatch dcmd;
        dcmd.init(matching_pso, cc::max(1u, levelWidth / 8), cc::max(1u, levelHeight / 8), size.array_size);
        dcmd.add_shader_arg(handle::null_resource, 0, sv);
        cmd_writer.add_command(dcmd);

        // record post-dispatch barriers
        record_barriers(post_dispatch);
    }
}

void inc::texture_creator::flush_cmdstream(bool dispatch, bool stall)
{
    if (!cmd_writer.empty())
    {
        pending_cmd_lists.push_back(backend->recordCommandList(cmd_writer.buffer(), cmd_writer.size()));
        cmd_writer.reset();
    }

    if (dispatch)
    {
        backend->submit(pending_cmd_lists);
        pending_cmd_lists.clear();

        if (stall)
        {
            backend->flushGPU();

            backend->freeRange(shader_views_to_free);
            shader_views_to_free.clear();

            backend->freeRange(resources_to_free);
            resources_to_free.clear();
        }
    }
    else
    {
        CC_ASSERT(!stall && "cannot stall if not dispatching");
    }
}
