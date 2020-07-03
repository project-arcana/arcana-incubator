#pragma once

#include <clean-core/function_ptr.hh>

#include <phantasm-renderer/common/hashable_storage.hh>
#include <phantasm-renderer/resource_types.hh>

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

using write_flags = uint32_t;
using read_flags = uint32_t;

struct virtual_resource_handle
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

struct virtual_resource
{
    res_guid_t const initial_guid; // unique
    bool const is_imported;
    bool is_culled = true; ///< whether this resource is culled, starts out as true, result is loop-AND over its version structs
    physical_res_idx associated_physical = gc_invalid_physical_res;

    pr::hashable_storage<phi::arg::create_resource_info> create_info;
    pr::raw_resource imported_resource;

    virtual_resource(res_guid_t guid, phi::arg::create_resource_info const& info) : initial_guid(guid), is_imported(false) { _copy_info(info); }

    virtual_resource(res_guid_t guid, pr::raw_resource import_resource, phi::arg::create_resource_info const& info)
      : initial_guid(guid), is_imported(true), imported_resource(import_resource)
    {
        _copy_info(info);
    }

private:
    void _copy_info(phi::arg::create_resource_info const& info);
};

struct virtual_resource_version
{
    virtual_res_idx const res_idx; // constant across versions
    pass_idx const producer_pass;  // new producer with each version
    int const version;
    int num_references = 0;
    bool is_root_resource = false;

    bool can_cull() const { return num_references == 0 && !is_root_resource; }

    virtual_resource_version(virtual_res_idx res, pass_idx producer, int ver) : res_idx(res), producer_pass(producer), version(ver) {}
};

using pass_execute_func_ptr = cc::function_ptr<void(GraphBuilder&, pass_idx, void*)>;

}
