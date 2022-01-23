#pragma once

#include <stdint.h>

#include <clean-core/fwd.hh>

#include <phantasm-hardware-interface/fwd.hh>

#ifndef IMGUI_API
#define IMGUI_API
#endif
#ifndef IMGUI_IMPL_API
#define IMGUI_IMPL_API IMGUI_API
#endif

struct ImDrawData;

IMGUI_IMPL_API bool ImGui_ImplPHI_Init(phi::Backend* backend, int num_frames_in_flight, phi::format target_format);
IMGUI_IMPL_API void ImGui_ImplPHI_Shutdown();
IMGUI_IMPL_API void ImGui_ImplPHI_NewFrame();

// writes draw commands to out_cmdbuffer
IMGUI_IMPL_API void ImGui_ImplPHI_RenderDrawData(ImDrawData const* draw_data, cc::span<std::byte> out_cmdbuffer);

// writes draw commands to out_cmdbuffer, drawing with a custom PSO
IMGUI_IMPL_API void ImGui_ImplPHI_RenderDrawDataWithPSO(ImDrawData const* draw_data,
                                                        phi::handle::pipeline_state pso,
#if INC_ENABLE_IMGUI_PHI_BINDLESS
                                                        phi::handle::shader_view sv,
#endif
                                                        cc::span<std::byte> out_cmdbuffer);

// returns the minimum size of out_cmdbuffer when calling _RenderDrawData
IMGUI_IMPL_API uint32_t ImGui_ImplPHI_GetDrawDataCommandSize(ImDrawData const* draw_data);

// Initializes with provided shader binaries, without compiling embedded ones
// out_upload_buffer: if non-null, the internally created upload buffer is written to this address instead of flushing and destroying it
// opt_root_sig: if non-null, use this root signature description instead of the default one
IMGUI_IMPL_API bool ImGui_ImplPHI_InitWithShaders(phi::Backend* backend,
                                                  int num_frames_in_flight,
                                                  phi::format target_format,
                                                  cc::span<std::byte const> ps_data,
                                                  cc::span<std::byte const> vs_data,
                                                  phi::handle::resource* out_upload_buffer = nullptr,
                                                  phi::arg::root_signature_description const* opt_root_sig = nullptr);

// Initializes without creating a PSO at all, must be used with _RenderDrawDataWithPSO
// out_font_texture:  in bindless mode, the texture created for fonts is written to this address
//                    it must be converted to an SRV index and written to io.Fonts->TexID (upper 4B) via your bindless system
// out_upload_buffer: if non-null, the upload buffer created to upload the font texture is written to this address instead of flushing and destroying it
IMGUI_IMPL_API bool ImGui_ImplPHI_InitWithoutPSO(phi::Backend* backend,
                                                 int num_frames_in_flight,
                                                 phi::format target_format,
#if INC_ENABLE_IMGUI_PHI_BINDLESS
                                                 phi::handle::resource* out_font_texture,
#endif
                                                 phi::handle::resource* out_upload_buffer = nullptr);

// Returns the config info for the required PSO if you want to completely supply it yourself (use with ImGui_ImplPHI_InitWithoutPSO and _RenderDrawDataWithPSO)
IMGUI_IMPL_API void ImGui_ImplPHI_GetDefaultPSOConfig(phi::format target_format,
                                                      phi::vertex_attribute_info out_vert_attrs[3],
                                                      uint32_t* out_vert_size,
                                                      phi::arg::framebuffer_config* out_framebuf_conf,
                                                      phi::arg::pipeline_config* out_raster_conf);
