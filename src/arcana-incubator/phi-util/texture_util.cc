#include "texture_util.hh"

#include <clean-core/utility.hh>

#include <phantasm-hardware-interface/commands.hh>
#include <phantasm-hardware-interface/common/byte_util.hh>
#include <phantasm-hardware-interface/common/format_size.hh>
#include <phantasm-hardware-interface/util.hh>

#include <typed-geometry/tg.hh>

void inc::copy_data_to_texture(phi::command_stream_writer& writer,
                               phi::handle::resource upload_buffer,
                               std::byte* upload_buffer_map,
                               phi::handle::resource dest_texture,
                               phi::format dest_format,
                               unsigned dest_width,
                               unsigned dest_height,
                               const std::byte* img_data,
                               bool use_d3d12_per_row_alingment)
{
    using namespace phi;
    auto const bytes_per_pixel = phi::util::get_format_size_bytes(dest_format);

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
        command.source_offset_bytes = accumulated_offset_bytes;

        writer.add_command(command);

        auto const mip_row_size_bytes = bytes_per_pixel * command.dest_width;
        auto mip_row_stride_bytes = mip_row_size_bytes;

        if (use_d3d12_per_row_alingment)
            // MIP maps are 256-byte aligned per row in d3d12
            mip_row_stride_bytes = phi::util::align_up(mip_row_stride_bytes, 256);

        auto const mip_offset_bytes = mip_row_stride_bytes * command.dest_height;
        accumulated_offset_bytes += mip_offset_bytes;

        inc::assets::rowwise_copy(img_data, upload_buffer_map + command.source_offset_bytes, mip_row_stride_bytes, mip_row_size_bytes, command.dest_height);
    }
}
