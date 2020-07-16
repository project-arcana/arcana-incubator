#pragma once

#include <phantasm-hardware-interface/types.hh>

#include "fwd.hh"

namespace inc::frag
{
using res_guid_t = uint64_t;
inline constexpr res_guid_t gc_invalid_guid = uint64_t(-1);

using pass_idx = uint32_t;
inline constexpr pass_idx gc_invalid_pass = uint32_t(-1);
using virtual_res_idx = uint32_t;
inline constexpr virtual_res_idx gc_invalid_virtual_res = uint32_t(-1);
using physical_res_idx = uint32_t;
inline constexpr physical_res_idx gc_invalid_physical_res = uint32_t(-1);

struct res_handle // long name: virtual resource handle
{
    virtual_res_idx resource;
    int version_before;
};

struct access_mode
{
    phi::resource_state required_state = phi::resource_state::undefined;
    phi::shader_stage_flags_t stage_flags = cc::no_flags;

    access_mode() = default;
    access_mode(phi::resource_state state, phi::shader_stage_flags_t flags = cc::no_flags) : required_state(state), stage_flags(flags) {}
    bool is_set() const { return required_state != phi::resource_state::undefined; }
};
}
