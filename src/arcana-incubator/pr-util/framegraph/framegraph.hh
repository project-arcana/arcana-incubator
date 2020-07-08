#pragma once

#include <cstdint>
#include <functional>
#include <type_traits>

#include <rich-log/log.hh>

#include <clean-core/capped_vector.hh>
#include <clean-core/function_ref.hh>
#include <clean-core/vector.hh>

#include <phantasm-hardware-interface/fwd.hh>
#include <phantasm-hardware-interface/types.hh>

#include <phantasm-renderer/Frame.hh>
#include <phantasm-renderer/common/hashable_storage.hh>
#include <phantasm-renderer/resource_types.hh>

#include <arcana-incubator/pr-util/timestamp_bundle.hh>

#include "fwd.hh"
#include "types.hh"

namespace inc::frag
{
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

        virtual_resource_handle import(res_guid_t guid, pr::render_target const& rt, access_mode mode = {})
        {
            return _parent->registerImport(_pass, guid, rt.res, mode, pr::generic_resource_info::create(rt.info));
        }

        virtual_resource_handle read(res_guid_t guid, access_mode mode = {}) { return _parent->registerRead(_pass, guid, mode); }
        virtual_resource_handle write(res_guid_t guid, access_mode mode = {}) { return _parent->registerWrite(_pass, guid, mode); }
        virtual_resource_handle readWrite(res_guid_t guid, access_mode mode = {}) { return _parent->registerReadWrite(_pass, guid, mode); }
        virtual_resource_handle move(res_guid_t src_guid, res_guid_t dest_guid) { return _parent->registerMove(_pass, src_guid, dest_guid); }

        void setRoot() { _parent->makePassRoot(_pass); }
        void setResourceRoot(res_guid_t guid) { _parent->makeResourceRoot(guid); }
        void setQueue(phi::queue_type queue) { _parent->setPassQueue(_pass, queue); }

        tg::isize2 targetSize() const { return _backbuf_size; }

    private:
        friend class GraphBuilder;
        setup_context(pass_idx pass, GraphBuilder* parent, tg::isize2 bb_s) : _pass(pass), _parent(parent), _backbuf_size(bb_s) {}
        setup_context(setup_context const&) = delete;
        setup_context(setup_context&&) = delete;

        pass_idx const _pass;
        GraphBuilder* const _parent;
        tg::isize2 const _backbuf_size;
    };

    struct execute_context
    {
        physical_resource const& get(virtual_resource_handle handle) const { return _parent->getPhysical(handle, _pass); }

        pr::buffer get_buffer(virtual_resource_handle handle) const { return get(handle).as_buffer(); }

        pr::render_target get_target(virtual_resource_handle handle) const { return get(handle).as_target(); }

        pr::texture get_texture(virtual_resource_handle handle) const { return get(handle).as_texture(); }

        pr::raii::Frame& frame() const { return *_frame; }

    private:
        friend class GraphBuilder;
        execute_context(pass_idx pass, GraphBuilder* parent, pr::raii::Frame* frame) : _pass(pass), _parent(parent), _frame(frame) {}
        execute_context(execute_context const&) = delete;
        execute_context(execute_context&&) = delete;

        pass_idx const _pass;
        GraphBuilder* const _parent;
        pr::raii::Frame* const _frame;
    };

private:
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
        std::function<void(GraphBuilder*, pr::raii::Frame*)> execute_func;

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

public:
    void initialize(pr::Context& ctx, unsigned max_num_passes = 50);

    template <class PassDataT, class ExecF>
    pass_idx addPass(char const* debug_name, cc::function_ref<void(PassDataT&, setup_context&)> setup_func, ExecF&& exec_func)
    {
        static_assert(std::is_invocable_r_v<void, ExecF, PassDataT const&, execute_context&>, "exec function has wrong signature");

        // create new pass
        pass_idx const new_pass_idx = pass_idx(mPasses.size());
        internal_pass& new_pass = mPasses.emplace_back(debug_name);

        // immediately execute setup
        PassDataT pass_data = {};
        setup_context setup_ctx = {new_pass_idx, this, mBackbufferSize};
        setup_func(pass_data, setup_ctx);

        // capture pass_data per value, move user exec lambda into capture
        new_pass.execute_func = [this, pass_data, user_func = cc::move(exec_func), new_pass_idx, debug_name](GraphBuilder* builder, pr::raii::Frame* frame) {
            execute_context exec_ctx = {new_pass_idx, builder, frame};

            frame->begin_debug_label(debug_name);
            startTiming(new_pass_idx, frame);

            user_func(pass_data, exec_ctx);

            endTiming(new_pass_idx, frame);
            frame->end_debug_label();
        };

        return new_pass_idx;
    }

    void compile(GraphCache& cache);

    void printState() const;

    void execute(pr::raii::Frame* frame);

    // resets, can now re-record passes
    void reset();

    void setBackbufferSize(tg::isize2 size) { mBackbufferSize = size; }

    //
    // Info
    //

    [[nodiscard]] double getLastTiming(pass_idx pass) const { return mTiming.get_last_timing(pass); }

    void performInfoImgui() const;

private:
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

        void on_create(virtual_res_idx new_res);

        void on_move_create(virtual_res_idx new_res, int new_version);

        void on_move_modify(virtual_res_idx new_res, int new_version);

        virtual_resource_handle get_handle() const { return {virtual_res, virtual_res_version}; }

        virtual_resource_handle get_handle_and_bump_version();

        bool is_valid() const { return state != state_initial; }

        guid_state(res_guid_t g) : guid(g) {}
    };

    // setup-time API
private:
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

    // execute-time API
private:
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
    // Step 1
    void runFloodfillCulling();

    // Step 2
    void realizePhysicalResources(GraphCache& cache);

    // Step 3
    void calculateBarriers();

private:
    void startTiming(pass_idx pass, pr::raii::Frame* frame);
    void endTiming(pass_idx pass, pr::raii::Frame* frame);

private:
    virtual_res_idx addResource(pass_idx producer, res_guid_t guid, phi::arg::create_resource_info const& info);
    virtual_res_idx addResource(pass_idx producer, res_guid_t guid, pr::raw_resource import_resource, pr::generic_resource_info const& info);

    void addVirtualVersion(virtual_res_idx resource, pass_idx producer, int version);

    virtual_resource_version& getVirtualVersion(virtual_res_idx resource, int version, unsigned* out_index = nullptr);

    guid_state& getGuidState(res_guid_t guid);

private:
    tg::isize2 mBackbufferSize;
    inc::pre::timestamp_bundle mTiming;

    cc::vector<internal_pass> mPasses;

    // virtual resource logic
    cc::vector<guid_state> mGuidStates;
    cc::vector<virtual_resource> mVirtualResources;
    cc::vector<virtual_resource_version> mVirtualResourceVersions;

    cc::vector<physical_resource> mPhysicalResources;
};

}
