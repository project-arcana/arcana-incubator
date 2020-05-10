#pragma once

#include <cstdint>
#include <functional>
#include <type_traits>

#include <rich-log/log.hh>

#include <clean-core/capped_vector.hh>
#include <clean-core/function_ptr.hh>
#include <clean-core/vector.hh>

#include <phantasm-hardware-interface/fwd.hh>
#include <phantasm-hardware-interface/types.hh>

#include <phantasm-renderer/resource_types.hh>

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

    phi::arg::create_resource_info create_info;
    pr::raw_resource imported_resource;

    virtual_resource(res_guid_t guid, phi::arg::create_resource_info const& info) : initial_guid(guid), is_imported(false), create_info(info) {}

    virtual_resource(res_guid_t guid, pr::raw_resource import_resource, phi::arg::create_resource_info const& info)
      : initial_guid(guid), is_imported(true), create_info(info), imported_resource(import_resource)
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

class GraphBuilder;
using pass_execute_func_ptr = cc::function_ptr<void(GraphBuilder&, pass_idx, void*)>;

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
    phi::queue_type queue = phi::queue_type::direct;
    std::function<void(GraphBuilder&)> execute_func;

    int num_references = 0;
    bool is_root_pass = false;

    cc::capped_vector<pass_write, 16> writes;
    cc::capped_vector<pass_read, 16> reads;
    cc::capped_vector<pass_create, 16> creates;
    cc::capped_vector<pass_import, 16> imports;
    cc::capped_vector<pass_move, 16> moves;

    cc::capped_vector<phi::transition_info, 64> transitions_before;

    internal_pass(char const* name) : debug_name(name) {}

    bool can_cull() const { return num_references == 0 && !is_root_pass; }
};

struct physical_resource
{
    pr::raw_resource raw_res;
    pr::generic_resource_info info;

    [[nodiscard]] pr::buffer as_buffer() const
    {
        CC_ASSERT(info.type == pr::generic_resource_info::e_resource_buffer && "created with different type, or imported without info");
        return {raw_res, info.info_buffer};
    }

    [[nodiscard]] pr::render_target as_target() const
    {
        CC_ASSERT(info.type == pr::generic_resource_info::e_resource_render_target && "created with different type, or imported without info");
        return {raw_res, info.info_render_target};
    }

    [[nodiscard]] pr::texture as_texture() const
    {
        CC_ASSERT(info.type == pr::generic_resource_info::e_resource_texture && "created with different type, or imported without info");
        return {raw_res, info.info_texture};
    }
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
    struct setup_context
    {
        virtual_resource_handle create(res_guid_t guid, phi::arg::create_resource_info const& info, access_mode mode = {})
        {
            return _parent->registerCreate(_pass, guid, info, mode);
        }

        virtual_resource_handle import(res_guid_t guid, pr::raw_resource raw_resource, access_mode mode = {}, pr::generic_resource_info const& optional_info = {})
        {
            return _parent->registerImport(_pass, guid, raw_resource, mode, optional_info);
        }

        virtual_resource_handle read(res_guid_t guid, access_mode mode = {}) { return _parent->registerRead(_pass, guid, mode); }
        virtual_resource_handle write(res_guid_t guid, access_mode mode = {}) { return _parent->registerWrite(_pass, guid, mode); }
        virtual_resource_handle readWrite(res_guid_t guid, access_mode mode = {}) { return _parent->registerReadWrite(_pass, guid, mode); }
        virtual_resource_handle move(res_guid_t src_guid, res_guid_t dest_guid) { return _parent->registerMove(_pass, src_guid, dest_guid); }

        void setRoot() { _parent->makePassRoot(_pass); }
        void setQueue(phi::queue_type queue) { _parent->setPassQueue(_pass, queue); }

        int targetWidth() const { return _backbuf_w; }
        int targetHeight() const { return _backbuf_h; }

    private:
        friend class GraphBuilder;
        setup_context(pass_idx pass, GraphBuilder* parent, int bb_w, int bb_h) : _pass(pass), _parent(parent), _backbuf_w(bb_w), _backbuf_h(bb_h) {}
        setup_context(setup_context const&) = delete;
        setup_context(setup_context&&) = delete;

        pass_idx const _pass;
        GraphBuilder* const _parent;
        int const _backbuf_w;
        int const _backbuf_h;
    };

    struct execute_context
    {
        physical_resource const& get(virtual_resource_handle handle) { return _parent->getPhysical(handle, _pass); }

    private:
        friend class GraphBuilder;
        execute_context(pass_idx pass, GraphBuilder* parent) : _pass(pass), _parent(parent) {}
        execute_context(setup_context const&) = delete;
        execute_context(execute_context&&) = delete;

        pass_idx const _pass;
        GraphBuilder* const _parent;
    };

public:
    GraphBuilder(int backbuffer_width, int backbuffer_height) : mBackbufferWidth(backbuffer_width), mBackbufferHeight(backbuffer_height) {}


    template <class PassDataT, class SetupF, class ExecF>
    pass_idx addPass(char const* debug_name, SetupF&& setup_func, ExecF&& exec_func)
    {
        static_assert(std::is_invocable_r_v<void, SetupF, PassDataT&, setup_context&>, "setup function has wrong signature");
        static_assert(std::is_invocable_r_v<void, ExecF, PassDataT const&, execute_context&>, "exec function has wrong signature");

        // create new pass
        pass_idx const new_pass_idx = pass_idx(mPasses.size());
        mPasses.emplace_back(debug_name);

        // immediately execute setup
        PassDataT pass_data = {};
        setup_context setup_ctx = {new_pass_idx, this, mBackbufferWidth, mBackbufferHeight};
        setup_func(pass_data, setup_ctx);

        // capture pass_data per value, move user exec lambda into capture
        mPasses.back().execute_func = [pass_data, user_func = cc::move(exec_func), new_pass_idx](GraphBuilder& builder) {
            execute_context exec_ctx = {new_pass_idx, &builder};
            user_func(pass_data, exec_ctx);
        };

        return new_pass_idx;
    }

    virtual_resource_handle registerCreate(pass_idx pass_idx, res_guid_t guid, phi::arg::create_resource_info const& info, access_mode mode);

    virtual_resource_handle registerImport(
        pass_idx pass_idx, res_guid_t guid, pr::raw_resource raw_resource, access_mode mode, pr::generic_resource_info const& optional_info = {});

    virtual_resource_handle registerWrite(pass_idx pass_idx, res_guid_t guid, access_mode mode);

    virtual_resource_handle registerRead(pass_idx pass_idx, res_guid_t guid, access_mode mode);

    virtual_resource_handle registerReadWrite(pass_idx pass_idx, res_guid_t guid, access_mode mode);

    virtual_resource_handle registerMove(pass_idx pass_idx, res_guid_t source_guid, res_guid_t dest_guid);

    void makeResourceRoot(res_guid_t resource)
    {
        auto const& state = getGuidState(resource);
        CC_ASSERT(state.is_valid() && "attempted to make invalid resource root");

        auto& resver = getVirtualVersion(state.virtual_res, state.virtual_res_version);
        resver.is_root_resource = true;
    }

    void makePassRoot(pass_idx pass) { mPasses[pass].is_root_pass = true; }
    void setPassQueue(pass_idx pass, phi::queue_type type) { mPasses[pass].queue = type; }

    void cull()
    {
        runFloodfillCulling();
        printState();
    }

    template <class F>
    void realizePhysicalResources(F&& realize_func)
    {
        static_assert(std::is_invocable_r_v<pr::raw_resource, F, pr::generic_resource_info const&>, "realize_func has wrong signature");
        CC_ASSERT(mPhysicalResources.empty() && "ran twice");

        mPhysicalResources.reserve(mVirtualResources.size());
        for (auto& virt : mVirtualResources)
        {
            if (virt.is_culled)
                continue;

            // passthrough imported resources or call the realize_func
            pr::raw_resource const physical = virt.is_imported ? virt.imported_resource : realize_func(virt.create_info);

            mPhysicalResources.push_back({physical, virt.create_info});
            virt.associated_physical = physical_res_idx(mPhysicalResources.size() - 1);
        }
    }

    void calculateBarriers();

    void execute()
    {
        for (auto const& pass : mPasses)
        {
            if (pass.can_cull())
                continue;

            pass.execute_func(*this);
        }
    }

    physical_resource const& getPhysical(virtual_resource_handle handle, pass_idx pass) const
    {
        // NOTE: we could perform some asserts here with the pass and handle version
        (void)pass;

        // unneccesary double indirection right now
        auto const physical_idx = mVirtualResources[handle.resource].associated_physical;
        CC_ASSERT(physical_idx != gc_invalid_physical_res && "resource was never realized");
        return mPhysicalResources[physical_idx];
    }

private:
    // reduces ::num_references to zero for all unreferenced passes and resource versions
    void runFloodfillCulling();

    void printState() const;

private:
    virtual_res_idx addResource(pass_idx producer, res_guid_t guid, phi::arg::create_resource_info const& info);
    virtual_res_idx addResource(pass_idx producer, res_guid_t guid, pr::raw_resource import_resource, pr::generic_resource_info const& info);

    void addVirtualVersion(virtual_res_idx resource, pass_idx producer, int version);

    virtual_resource_version& getVirtualVersion(virtual_res_idx resource, int version, unsigned* out_index = nullptr);

    guid_state& getGuidState(res_guid_t guid);

private:
    int mBackbufferWidth;
    int mBackbufferHeight;
    cc::vector<internal_pass> mPasses;

    // virtual resource logic
    cc::vector<guid_state> mGuidStates;
    cc::vector<virtual_resource> mVirtualResources;
    cc::vector<virtual_resource_version> mVirtualResourceVersions;

    cc::vector<physical_resource> mPhysicalResources;
};

}
