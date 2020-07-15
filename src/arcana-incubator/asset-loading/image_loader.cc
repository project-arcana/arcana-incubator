#include "image_loader.hh"

#include <cstdint>
#include <cstring>

#include <clean-core/assert.hh>
#include <clean-core/bit_cast.hh>
#include <clean-core/utility.hh>

#include <typed-geometry/tg.hh>

#include <arcana-incubator/asset-loading/lib/stb_image.hh>

#include <phantasm-hardware-interface/util.hh>

namespace
{
inc::assets::image_data load_image_internal(void* stbi_data, int w, int h, inc::assets::image_size& out_size, int desired_channels, bool use_hdr_float)
{
    inc::assets::image_data res;
    res.raw = stbi_data;
    res.is_hdr = use_hdr_float;

    if (!res.raw)
        return res;

    res.num_channels = uint8_t(desired_channels);

    out_size.width = unsigned(w);
    out_size.height = unsigned(h);
    out_size.num_mipmaps = phi::util::get_num_mips(out_size.width, out_size.height);
    out_size.array_size = 1;

    return res;
}
}

inc::assets::image_data inc::assets::load_image(cc::span<const std::byte> data, inc::assets::image_size& out_size, int desired_channels, bool use_hdr_float)
{
    int w, h, num_ch;
    void* stbi_data;

    if (use_hdr_float)
        stbi_data = ::stbi_loadf_from_memory(reinterpret_cast<stbi_uc const*>(data.data()), int(data.size()), &w, &h, &num_ch, desired_channels);
    else
        stbi_data = ::stbi_load_from_memory(reinterpret_cast<stbi_uc const*>(data.data()), int(data.size()), &w, &h, &num_ch, desired_channels);

    return load_image_internal(stbi_data, w, h, out_size, desired_channels, use_hdr_float);
}

inc::assets::image_data inc::assets::load_image(const char* filename, inc::assets::image_size& out_size, int desired_channels, bool use_hdr_float)
{
    int w, h, num_ch;
    void* stbi_data;

    if (use_hdr_float)
        stbi_data = ::stbi_loadf(filename, &w, &h, &num_ch, desired_channels);
    else
        stbi_data = ::stbi_load(filename, &w, &h, &num_ch, desired_channels);

    return load_image_internal(stbi_data, w, h, out_size, desired_channels, use_hdr_float);
}

void inc::assets::rowwise_copy(std::byte const* __restrict src, std::byte* __restrict dest, unsigned dest_row_stride_bytes, unsigned row_size_bytes, unsigned height_pixels)
{
    for (auto y = 0u; y < height_pixels; ++y)
    {
        std::memcpy(dest + y * dest_row_stride_bytes, src + y * row_size_bytes, row_size_bytes);
    }
}

void inc::assets::free(const inc::assets::image_data& data)
{
    if (data.raw)
        ::stbi_image_free(data.raw);
}

void inc::assets::write_mipmap(image_data& src_and_dest, unsigned width, unsigned height)
{
    CC_RUNTIME_ASSERT(!src_and_dest.is_hdr && "HDR mipmap generation unimplemented");
    constexpr unsigned offsetsX[] = {0, 1, 0, 1};
    constexpr unsigned offsetsY[] = {0, 0, 1, 1};

    auto const get_byte = [](auto color, int component) { return (color >> (8 * component)) & 0xff; };
    auto const get_color = [width](auto const* img_data, unsigned x, unsigned y) -> uint32_t { return img_data[x + y * width]; };
    auto const set_color = [width](auto* img_data, unsigned x, unsigned y, auto color) -> void { img_data[x + y * width / 2] = color; };

    auto* const image_data = static_cast<uint32_t*>(src_and_dest.raw);

    for (auto y = 0u; y < height; y += 2)
    {
        for (auto x = 0u; x < width; x += 2)
        {
            uint32_t color = 0;
            for (auto c = 0; c < src_and_dest.num_channels; ++c)
            {
                // average over the four pixels
                uint32_t color_component = 0;
                for (auto i = 0; i < 4; ++i)
                    color_component += get_byte(get_color(image_data, x + offsetsX[i], y + offsetsY[i]), (src_and_dest.num_channels - 1) - c);
                color = (color << 8) | (color_component / 4);
            }
            set_color(image_data, x / 2, y / 2, color);
        }
    }
}
