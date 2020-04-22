#pragma once

#include <phantasm-renderer/format.hh>
#include <phantasm-renderer/fwd.hh>
#include <phantasm-renderer/resource_types.hh>

namespace inc::pre
{
struct filtered_specular_result
{
    pr::auto_texture equirect_tex;   // the equirectangular texture
    pr::auto_texture unfiltered_env; // the unfiltered specular cubemap
    pr::auto_texture filtered_env;   // the filtered specular cubemap, likely what is desired
};

struct texture_processing
{
    void init(pr::Context& ctx, char const* path_prefix);
    void free();

    //
    // Textures

    [[nodiscard]] pr::auto_texture load_texture(pr::raii::Frame& frame, char const* path, pr::format fmt, bool mips = false, bool gamma = false);

    void generate_mips(pr::raii::Frame& frame, pr::texture const& texture, bool apply_gamma = false);

    //
    // IBL

    [[nodiscard]] filtered_specular_result load_filtered_specular_map(pr::raii::Frame& frame, char const* hdr_equirect_path);

    [[nodiscard]] pr::auto_texture create_diffuse_irradiance_map(pr::raii::Frame& frame, pr::texture const& filtered_specular);

    [[nodiscard]] pr::auto_texture create_brdf_lut(pr::raii::Frame& frame, int width_height);

private:
    pr::auto_compute_pipeline_state pso_mipgen;
    pr::auto_compute_pipeline_state pso_mipgen_gamma;
    pr::auto_compute_pipeline_state pso_mipgen_array;

    pr::auto_compute_pipeline_state pso_equirect_to_cube;
    pr::auto_compute_pipeline_state pso_specular_map_filter;
    pr::auto_compute_pipeline_state pso_irradiance_map_gen;
    pr::auto_compute_pipeline_state pso_brdf_lut_gen;
};

}
