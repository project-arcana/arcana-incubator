#include "texture_util.hh"

#include <phantasm-renderer/backend/commands.hh>
#include <phantasm-renderer/backend/detail/byte_util.hh>
#include <phantasm-renderer/backend/detail/format_size.hh>

#include <typed-geometry/tg.hh>

void inc::copy_data_to_texture(pr::backend::command_stream_writer& writer,
                               pr::backend::handle::resource upload_buffer,
                               std::byte* upload_buffer_map,
                               pr::backend::handle::resource dest_texture,
                               pr::backend::format dest_format,
                               unsigned dest_width,
                               unsigned dest_height,
                               const std::byte* img_data,
                               bool use_d3d12_per_row_alingment)
{
    using namespace pr::backend;
    auto const bytes_per_pixel = detail::pr_format_size_bytes(dest_format);

    // CC_RUNTIME_ASSERT(img_size.array_size == 1 && "array upload unimplemented");
    // NOTE: image_data is currently always a single array slice

    cmd::copy_buffer_to_texture command;
    command.source = upload_buffer;
    command.destination = dest_texture;
    command.dest_width = dest_width;
    command.dest_height = dest_height;
    command.dest_mip_index = 0;

    auto accumulated_offset_bytes = 0u;

    // for (auto a = 0u; a < img_size.array_size; ++a)
    {
        command.dest_array_index = 0u; // a;
        command.source_offset = accumulated_offset_bytes;

        writer.add_command(command);

        auto const mip_row_size_bytes = bytes_per_pixel * command.dest_width;
        auto mip_row_stride_bytes = mip_row_size_bytes;

        if (use_d3d12_per_row_alingment)
            // MIP maps are 256-byte aligned per row in d3d12
            mip_row_stride_bytes = mem::align_up(mip_row_stride_bytes, 256);

        auto const mip_offset_bytes = mip_row_stride_bytes * command.dest_height;
        accumulated_offset_bytes += mip_offset_bytes;

        inc::assets::rowwise_copy(img_data, upload_buffer_map + command.source_offset, mip_row_stride_bytes, mip_row_size_bytes, command.dest_height);
    }
}

unsigned inc::get_mipmap_upload_size(pr::backend::format format, const inc::assets::image_size& img_size, bool no_mips)
{
    using namespace pr::backend;
    auto const bytes_per_pixel = detail::pr_format_size_bytes(format);
    auto res_bytes = 0u;

    for (auto a = 0u; a < img_size.array_size; ++a)
    {
        auto const num_mips = no_mips ? 1 : img_size.num_mipmaps;
        for (auto mip = 0u; mip < num_mips; ++mip)
        {
            auto const mip_width = cc::max(unsigned(tg::floor(img_size.width / tg::pow(2.f, float(mip)))), 1u);
            auto const mip_height = cc::max(unsigned(tg::floor(img_size.height / tg::pow(2.f, float(mip)))), 1u);

            auto const row_pitch = mem::align_up(bytes_per_pixel * mip_width, 256);
            auto const custom_offset = row_pitch * mip_height;
            res_bytes += custom_offset;
        }
    }

    return res_bytes;
}
