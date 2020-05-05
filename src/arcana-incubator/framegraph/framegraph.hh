#pragma once

#include <cstdint>

#include <rich-log/log.hh>

#include <clean-core/capped_vector.hh>
#include <clean-core/function_ptr.hh>
#include <clean-core/vector.hh>

#include <phantasm-hardware-interface/types.hh>

#include <phantasm-renderer/common/resource_info.hh>

namespace inc::frag
{
using res_guid_t = uint64_t;
inline constexpr res_guid_t gc_invalid_guid = uint64_t(-1);

using pass_idx = uint32_t;
inline constexpr pass_idx gc_invalid_pass = uint32_t(-1);
using virtual_res_idx = uint32_t;
inline constexpr virtual_res_idx gc_invalid_virtual_res = uint32_t(-1);

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

    bool is_set() const { return required_state != phi::resource_state::undefined; }
};

struct virtual_resource
{
    res_guid_t const initial_guid; // unique
    bool const is_imported;

    union {
        phi::arg::create_resource_info create_info;
        phi::handle::resource imported_resource;
    };

    virtual_resource(res_guid_t guid, phi::arg::create_resource_info const& info) : initial_guid(guid), is_imported(false), create_info(info) {}

    virtual_resource(res_guid_t guid, phi::handle::resource import_resource)
      : initial_guid(guid), is_imported(true), imported_resource(import_resource)
    {
    }
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

struct execution_context;
using pass_execute_func_ptr = cc::function_ptr<void(execution_context&, void*)>;

struct internal_pass
{
    struct pass_write
    {
        virtual_res_idx res;
        int version_before;
        access_mode mode;
    };

    struct pass_read
    {
        virtual_res_idx res;
        int version_before;
        access_mode mode;
    };

    struct pass_create
    {
        virtual_res_idx res;
        access_mode mode;
    };

    struct pass_import
    {
        virtual_res_idx res;
        access_mode mode;
    };

    struct pass_move
    {
        virtual_res_idx src_res;
        int src_version_before;
        res_guid_t dest_guid;
    };

    char const* const debug_name;
    void* const userdata;
    pass_execute_func_ptr const execute_func;

    int num_references = 0;
    bool is_root_pass = false;

    cc::capped_vector<pass_write, 16> writes;
    cc::capped_vector<pass_read, 16> reads;
    cc::capped_vector<pass_create, 16> creates;
    cc::capped_vector<pass_import, 16> imports;
    cc::capped_vector<pass_move, 16> moves;

    internal_pass(char const* name, void* userdata, pass_execute_func_ptr execute_func)
      : debug_name(name), userdata(userdata), execute_func(execute_func)
    {
    }

    bool can_cull() const { return num_references == 0 && !is_root_pass; }
};

struct physical_resource
{
    phi::handle::resource res;
};

struct guid_state
{
    enum e_state
    {
        state_initial,
        state_valid_created,
        state_valid_move_created,
        state_valid_moved_to
    };

    res_guid_t const guid;
    e_state state = state_initial;
    virtual_res_idx virtual_res = gc_invalid_virtual_res;
    int virtual_res_version = 0;

    void on_create(virtual_res_idx new_res)
    {
        CC_ASSERT(!is_valid());
        state = state_valid_created;
        virtual_res = new_res;
    }

    void on_move_create(virtual_res_idx new_res, int new_version)
    {
        CC_ASSERT(!is_valid());
        state = state_valid_move_created;
        virtual_res = new_res;
        virtual_res_version = new_version;
    }

    void on_move_modify(virtual_res_idx new_res, int new_version)
    {
        CC_ASSERT(is_valid());
        state = state_valid_moved_to;
        virtual_res = new_res;
        virtual_res_version = new_version;
    }

    virtual_resource_handle get_handle() const { return {virtual_res, virtual_res_version}; }

    virtual_resource_handle get_handle_and_bump_version()
    {
        auto const res = get_handle();
        ++virtual_res_version;
        return res;
    }

    bool is_valid() const { return state != state_initial; }

    guid_state(res_guid_t g) : guid(g) {}
};

class GraphBuilder
{
public:
    pass_idx addPass(pass_execute_func_ptr execfunc, void* userdata = nullptr, char const* debug_name = nullptr)
    {
        mPasses.emplace_back(debug_name, userdata, execfunc);
        return pass_idx(mPasses.size() - 1);
    }

    virtual_resource_handle registerCreate(pass_idx pass_idx, res_guid_t guid, phi::arg::create_resource_info const& info, access_mode mode)
    {
        auto const new_idx = addResource(pass_idx, guid, info);

        auto& guidstate = getGuidState(guid);
        CC_ASSERT(!guidstate.is_valid() && "resource guid was already created, imported, or moved to");
        guidstate.on_create(new_idx);

        internal_pass& pass = mPasses[pass_idx];
        pass.creates.push_back({new_idx, mode});
        ++pass.num_references; // increase refcount for each create

        return guidstate.get_handle();
    }

    virtual_resource_handle registerImport(pass_idx pass_idx, res_guid_t guid, phi::handle::resource raw_resource, access_mode mode)
    {
        auto const new_idx = addResource(pass_idx, guid, raw_resource);

        auto& guidstate = getGuidState(guid);
        CC_ASSERT(!guidstate.is_valid() && "resource guid was already created, imported or moved to");
        guidstate.on_create(new_idx);

        internal_pass& pass = mPasses[pass_idx];
        pass.imports.push_back({new_idx, mode});
        ++pass.num_references; // increase refcount for each import

        return guidstate.get_handle();
    }

    virtual_resource_handle registerWrite(pass_idx pass_idx, res_guid_t guid, access_mode mode)
    {
        auto& guidstate = getGuidState(guid);
        CC_ASSERT(guidstate.is_valid() && "writing to resource without prior create");

        internal_pass& pass = mPasses[pass_idx];

        ++pass.num_references; // writes increase refcount for the PASS
        pass.writes.push_back({guidstate.virtual_res, guidstate.virtual_res_version, mode});

        // writes increase resource version
        auto const return_handle = guidstate.get_handle_and_bump_version();

        // add the new version marking this pass as its producer
        addVirtualVersion(guidstate.virtual_res, pass_idx, guidstate.virtual_res_version);

        return return_handle;
    }

    virtual_resource_handle registerRead(pass_idx pass_idx, res_guid_t guid, access_mode mode)
    {
        auto& guidstate = getGuidState(guid);
        CC_ASSERT(guidstate.is_valid() && "reading from resource without prior create");

        internal_pass& pass = mPasses[pass_idx];
        pass.reads.push_back({guidstate.virtual_res, guidstate.virtual_res_version, mode});

        virtual_resource_version& resver = getVirtualVersion(guidstate.virtual_res, guidstate.virtual_res_version);
        ++resver.num_references; // reads increase refcount for the RESOURCE

        return guidstate.get_handle();
    }

    virtual_resource_handle registerReadWrite(pass_idx pass_idx, res_guid_t guid, access_mode mode)
    {
        registerRead(pass_idx, guid, mode);                  // the read takes care of the access_mode
        return registerWrite(pass_idx, guid, access_mode{}); // the write mode is omitted (null state)
    }

    virtual_resource_handle registerMove(pass_idx pass_idx, res_guid_t source_guid, res_guid_t dest_guid)
    {
        auto const& src_state = getGuidState(source_guid);
        CC_ASSERT(src_state.is_valid() && "moving from invalid resource");
        auto const src_idx = src_state.virtual_res;

        // add the move and increase pass references
        // a move has no effects on refcounts
        internal_pass& pass = mPasses[pass_idx];
        pass.moves.push_back({src_state.virtual_res, src_state.virtual_res_version, dest_guid});

        auto& dest_state = getGuidState(dest_guid);
        if (dest_state.is_valid())
        {
            // moving into an existing GUID state, the previous virtual resource is no longer accessible via dest_guid
            dest_state.on_move_modify(src_idx, src_state.virtual_res_version);
            return dest_state.get_handle();
        }
        else
        {
            // moving into a new GUID state, there is no virtual resource being shadowed
            dest_state.on_move_create(src_idx, src_state.virtual_res_version);
            return dest_state.get_handle();
        }
    }

    void makeResourceRoot(res_guid_t resource)
    {
        auto const& state = getGuidState(resource);
        CC_ASSERT(state.is_valid() && "attempted to make invalid resource root");

        auto& resver = getVirtualVersion(state.virtual_res, state.virtual_res_version);
        resver.is_root_resource = true;
    }

    void makePassRoot(pass_idx pass) { mPasses[pass].is_root_pass = true; }

    void compile()
    {
        runFloodfillCulling();
        printState();
    }

private:
    // reduces ::num_references to zero for all unreferenced passes and resource versions
    void runFloodfillCulling()
    {
        cc::vector<unsigned> unreferenced_res_ver_indices;
        unreferenced_res_ver_indices.reserve(mVirtualResourceVersions.size());

        for (auto i = 0u; i < mVirtualResourceVersions.size(); ++i)
        {
            if (mVirtualResourceVersions[i].can_cull())
                unreferenced_res_ver_indices.push_back(i);
        }

        while (!unreferenced_res_ver_indices.empty())
        {
            virtual_res_idx const idx = unreferenced_res_ver_indices.back();
            unreferenced_res_ver_indices.pop_back();

            virtual_resource_version const& resver = mVirtualResourceVersions[idx];

            internal_pass& producer = mPasses[resver.producer_pass];
            --producer.num_references; // decrement refcount of this resource's producer
            CC_ASSERT(producer.num_references >= 0 && "programmer error");

            if (producer.can_cull())
            {
                // producer culled, decrement refcount of all resources it read
                for (auto& read : producer.reads)
                {
                    unsigned read_resource_version_index;
                    auto& read_resource_version = getVirtualVersion(read.res, read.version_before, &read_resource_version_index);
                    --read_resource_version.num_references;
                    CC_ASSERT(read_resource_version.num_references >= 0 && "programmer error");

                    // resource now unreferenced, add to stack
                    if (read_resource_version.can_cull())
                        unreferenced_res_ver_indices.push_back(read_resource_version_index);
                }
            }
        }
    }

    void printState() const
    {
        for (auto const& pass : mPasses)
        {
            if (pass.can_cull())
                continue;

            LOG(info)("pass {} ({} refs{})", pass.debug_name, pass.num_references, pass.is_root_pass ? ", root" : "");

            for (auto const& create : pass.creates)
            {
                LOG(info)("  <* create {} v0", create.res);
            }
            for (auto const& import : pass.imports)
            {
                LOG(info)("  <# import {} v0", import.res);
            }
            for (auto const& read : pass.reads)
            {
                LOG(info)("  -> read {} v{}", read.res, read.version_before);
            }
            for (auto const& write : pass.writes)
            {
                LOG(info)("  <- write {} v{}", write.res, write.version_before);
            }
            for (auto const& move : pass.moves)
            {
                LOG(info)("  -- move {} v{} -> g{}", move.src_res, move.src_version_before, move.dest_guid);
            }
        }

        for (auto const& resver : mVirtualResourceVersions)
        {
            if (resver.can_cull())
                continue;

            LOG(info)
            ("resource {} at version {} ({} refs{})", resver.res_idx, resver.version, resver.num_references, resver.is_root_resource ? ", root" : "");
        }
    }

private:
    virtual_res_idx addResource(pass_idx producer, res_guid_t guid, phi::arg::create_resource_info const& info)
    {
        mVirtualResources.emplace_back(guid, info);
        auto const ret_idx = virtual_res_idx(mVirtualResources.size() - 1);
        addVirtualVersion(ret_idx, producer, 0);
        return ret_idx;
    }
    virtual_res_idx addResource(pass_idx producer, res_guid_t guid, phi::handle::resource import_resource)
    {
        mVirtualResources.emplace_back(guid, import_resource);
        auto const ret_idx = virtual_res_idx(mVirtualResources.size() - 1);
        addVirtualVersion(ret_idx, producer, 0);
        return ret_idx;
    }

    void addVirtualVersion(virtual_res_idx resource, pass_idx producer, int version)
    {
        mVirtualResourceVersions.emplace_back(resource, producer, version);
    }

    virtual_resource_version& getVirtualVersion(virtual_res_idx resource, int version, unsigned* out_index = nullptr)
    {
        for (auto i = 0u; i < mVirtualResourceVersions.size(); ++i)
        {
            auto& virtual_version = mVirtualResourceVersions[i];
            if (virtual_version.res_idx == resource && virtual_version.version == version)
            {
                if (out_index != nullptr)
                    *out_index = i;

                return virtual_version;
            }
        }

        CC_UNREACHABLE("version not found");
        return mVirtualResourceVersions.back();
    }

    guid_state& getGuidState(res_guid_t guid)
    {
        for (auto& state : mGuidStates)
        {
            if (state.guid == guid)
                return state;
        }

        return mGuidStates.emplace_back(guid);
    }

private:
    cc::vector<internal_pass> mPasses;

    // virtual resource logic
    cc::vector<guid_state> mGuidStates;
    cc::vector<virtual_resource> mVirtualResources;
    cc::vector<virtual_resource_version> mVirtualResourceVersions;

    cc::vector<physical_resource> mPhysicalResources;
};

}
