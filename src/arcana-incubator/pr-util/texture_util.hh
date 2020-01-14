#pragma once

#include <cstddef>

#include <phantasm-renderer/backend/fwd.hh>
#include <phantasm-renderer/backend/types.hh>

#include <arcana-incubator/asset-loading/image_loader.hh>

namespace inc
{
void copy_data_to_texture(pr::backend::command_stream_writer& writer,
                          pr::backend::handle::resource upload_buffer,
                          std::byte* upload_buffer_map,
                          pr::backend::handle::resource dest_texture,
                          pr::backend::format dest_format,
                          unsigned dest_width,
                          unsigned dest_height,
                          const std::byte* img_data,
                          bool use_d3d12_per_row_alingment);

unsigned get_mipmap_upload_size(pr::backend::format format, inc::assets::image_size const& img_size, bool no_mips = false);

}
