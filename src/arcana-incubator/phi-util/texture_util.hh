#pragma once

#include <cstddef>

#include <phantasm-hardware-interface/fwd.hh>
#include <phantasm-hardware-interface/types.hh>

#include <arcana-incubator/asset-loading/image_loader.hh>

namespace inc
{
void copy_data_to_texture(phi::command_stream_writer& writer,
                          phi::handle::resource upload_buffer,
                          std::byte* upload_buffer_map,
                          phi::handle::resource dest_texture,
                          phi::format dest_format,
                          unsigned dest_width,
                          unsigned dest_height,
                          const std::byte* img_data,
                          bool use_d3d12_per_row_alingment);

unsigned get_mipmap_upload_size(phi::format format, inc::assets::image_size const& img_size, bool no_mips = false);

}
