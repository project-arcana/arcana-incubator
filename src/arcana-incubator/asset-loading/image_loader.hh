#pragma once

#include <cstddef>
#include <cstdint>

namespace inc::assets
{
struct image_size
{
    unsigned width;
    unsigned height;
    unsigned num_mipmaps;
    unsigned array_size;
};

struct image_data
{
    void* raw;
    uint8_t num_channels;
    bool is_hdr;
};

[[nodiscard]] constexpr inline bool is_valid(image_data const& data) { return data.raw != nullptr; }

[[nodiscard]] image_data load_image(char const* filename, image_size& out_size, int desired_channels = 4, bool use_hdr_float = false);

/// copy an image row-by-row to a destination pointer, with a stride per row
/// (usually equal to row_size_bytes, but not in D3D12)
void rowwise_copy(const std::byte* src, std::byte* dest, unsigned dest_row_stride_bytes, unsigned row_size_bytes, unsigned height_pixels);

void write_mipmap(inc::assets::image_data& src_and_dest, unsigned width, unsigned height);

void free(image_data const& data);

}
