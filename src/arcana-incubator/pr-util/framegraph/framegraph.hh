#pragma once

#include <cstdint>
#include <type_traits>

#include <clean-core/capped_vector.hh>
#include <clean-core/function_ref.hh>
#include <clean-core/unique_function.hh>
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
    pr::resource raw_res;
    pr::generic_resource_info info;

    [[nodiscard]] pr::buffer as_buffer() const { return {raw_res}; }

    [[nodiscard]] pr::texture as_texture() const { return {raw_res}; }
};

class GraphBuilder
{
public:
    void initialize(cc::allocator* alloc, unsigned max_num_passes, unsigned max_num_guids);

    void destroy();

public:
    // 1.
    // resets, can now re-record passes
    void reset();

    void setMainTargetSize(tg::isize2 size) { mMainTargetSize = size; }

    // 2.
    template <class PassDataT, class ExecF>
    pass_idx addPass(char const* debug_name, cc::function_ref<void(PassDataT&, setup_context&)> setup_func, ExecF&& exec_func);

    // 3.
    void compile(GraphCache& cache, cc::allocator* alloc);

    void printState() const;

    // 4.
    size_t getNumPasses() const { return mPasses.size(); }

    // execute a subrange of passes
    void executePasses(pr::raii::Frame* frame, size_t start, size_t end, pre::timestamp_bundle* timing = nullptr, int timer_offset = 0);

    // execute all passes
    void execute(pr::raii::Frame* frame, pre::timestamp_bundle* timing = nullptr, int timer_offset = 0);

    // after execute
    void performInfoImgui(pre::timestamp_bundle const* timing, bool* isWindowOpen = nullptr) const;

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

        res_handle get_handle() const { return {virtual_res, virtual_res_version}; }

        res_handle get_handle_and_bump_version();

        bool is_valid() const { return state != state_initial; }

        guid_state(res_guid_t g) : guid(g) {}
    };

    struct virtual_resource
    {
        enum state_bits : uint8_t
        {
            sb_culled = 1 << 0,
            sb_root = 1 << 1,
            sb_imported = 1 << 2,
        };

        res_guid_t const initial_guid; // unique
        uint8_t state = 0;

        physical_res_idx associated_physical = gc_invalid_physical_res;
        phi::arg::resource_description resource_info;
        pr::resource imported_resource;

        virtual_resource(res_guid_t guid, phi::arg::resource_description const& info) : initial_guid(guid), resource_info(info) { }

        virtual_resource(res_guid_t guid, pr::resource import_resource, phi::arg::resource_description const& info)
          : initial_guid(guid), state(sb_imported), resource_info(info), imported_resource(import_resource)
        {
        }

        bool is_root() const { return !!(state & sb_root); }
        bool is_culled() const { return !!(state & sb_culled); }
        bool is_imported() const { return !!(state & sb_imported); }
    };

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
        cc::unique_function<void(exec_context&)> execute_func;
        bool is_root_pass = false;
        bool is_culled = false;

        cc::capped_vector<pass_write, 16> writes;
        cc::capped_vector<pass_read, 16> reads;
        cc::capped_vector<pass_create, 16> creates;
        cc::capped_vector<pass_import, 16> imports;
        cc::capped_vector<pass_move, 16> moves;

        cc::capped_vector<phi::transition_info, 64> transitions_before;

        internal_pass(char const* name) : debug_name(name) {}
    };

    // setup-time API
private:
    friend struct setup_context;
    res_handle registerCreate(pass_idx pass_idx, res_guid_t guid, phi::arg::resource_description const& info, access_mode mode);

    res_handle registerImport(pass_idx pass_idx, res_guid_t guid, pr::resource raw_resource, access_mode mode, pr::generic_resource_info const& optional_info = {});

    res_handle registerWrite(pass_idx pass_idx, res_guid_t guid, access_mode mode);

    res_handle registerRead(pass_idx pass_idx, res_guid_t guid, access_mode mode);

    res_handle registerReadWrite(pass_idx pass_idx, res_guid_t guid, access_mode mode);

    res_handle registerMove(pass_idx pass_idx, res_guid_t source_guid, res_guid_t dest_guid);

    void makeResourceRoot(res_guid_t resource)
    {
        auto const& state = getGuidState(resource);
        CC_ASSERT(state.is_valid() && "attempted to make invalid resource root");
        mVirtualResources[state.virtual_res].state |= virtual_resource::sb_root;
    }

    void makePassRoot(pass_idx pass) { mPasses[pass].is_root_pass = true; }

    void setPassQueue(pass_idx pass, phi::queue_type type) { mPasses[pass].queue = type; }

    // execute-time API
private:
    friend struct exec_context;
    physical_resource const& getPhysical(res_handle handle) const
    {
        CC_ASSERT(handle.resource != gc_invalid_virtual_res && "accessed invalid handle");
        virtual_resource const& virt_res = mVirtualResources[handle.resource];
        CC_ASSERT(virt_res.associated_physical != gc_invalid_physical_res && "resource was never realized");
        return mPhysicalResources[virt_res.associated_physical];
    }

private:
    // Step 1
    void runFloodfillCulling(cc::allocator* alloc);

    // Step 2
    void realizePhysicalResources(GraphCache& cache);

    // Step 3
    void calculateBarriers();

private:
    virtual_res_idx addResource(pass_idx producer, res_guid_t guid, phi::arg::resource_description const& info);
    virtual_res_idx addResource(pass_idx producer, res_guid_t guid, pr::resource import_resource, pr::generic_resource_info const& info);

    guid_state& getGuidState(res_guid_t guid);

private:
    tg::isize2 mMainTargetSize;
    unsigned mNumReadsTotal = 0;
    unsigned mNumWritesTotal = 0;

    cc::alloc_vector<internal_pass> mPasses;

    // virtual resource logic
    cc::alloc_vector<guid_state> mGuidStates;
    cc::alloc_vector<virtual_resource> mVirtualResources;

    cc::alloc_vector<physical_resource> mPhysicalResources;
};

struct setup_context
{
    res_handle create(res_guid_t guid, phi::arg::resource_description const& info, access_mode mode = {})
    {
        return _parent->registerCreate(_pass, guid, info, mode);
    }

    res_handle create_target(res_guid_t guid,
                             phi::format fmt,
                             tg::isize2 size,
                             unsigned num_samples = 1,
                             unsigned array_size = 1,
                             phi::rt_clear_value clear_val = {0.f, 0.f, 0.f, 1.f},
                             access_mode mode = phi::resource_state::render_target)
    {
        return create(guid, phi::arg::resource_description::render_target(fmt, size, num_samples, array_size, clear_val), mode);
    }

    res_handle create_texture(res_guid_t guid,
                              phi::format fmt,
                              tg::isize2 size,
                              unsigned num_mips = 1,
                              phi::texture_dimension dim = phi::texture_dimension::t2d,
                              unsigned depth_or_array_size = 1,
                              bool allow_uav = false,
                              access_mode mode = phi::resource_state::render_target)
    {
        return create(guid, phi::arg::resource_description::texture(fmt, size, num_mips, dim, depth_or_array_size, allow_uav), mode);
    }

    res_handle create_buffer(res_guid_t guid,
                             unsigned size_bytes,
                             unsigned stride_bytes,
                             phi::resource_heap heap = phi::resource_heap::gpu,
                             bool allow_uav = false,
                             access_mode mode = {})
    {
        return create(guid, phi::arg::resource_description::buffer(size_bytes, stride_bytes, heap, allow_uav), mode);
    }

    res_handle import(res_guid_t guid, pr::resource raw_resource, access_mode mode = {}, pr::generic_resource_info const& optional_info = {})
    {
        return _parent->registerImport(_pass, guid, raw_resource, mode, optional_info);
    }

    res_handle import(res_guid_t guid, pr::texture const& texture, access_mode mode = {})
    {
        return _parent->registerImport(_pass, guid, texture, mode, {});
    }

    res_handle read(res_guid_t guid, access_mode mode = {}) { return _parent->registerRead(_pass, guid, mode); }
    res_handle write(res_guid_t guid, access_mode mode = {}) { return _parent->registerWrite(_pass, guid, mode); }
    res_handle read_write(res_guid_t guid, access_mode mode = {}) { return _parent->registerReadWrite(_pass, guid, mode); }
    res_handle move(res_guid_t src_guid, res_guid_t dest_guid) { return _parent->registerMove(_pass, src_guid, dest_guid); }

    void set_root() { _parent->makePassRoot(_pass); }
    void set_queue(phi::queue_type queue) { _parent->setPassQueue(_pass, queue); }

    tg::isize2 target_size() const { return _backbuf_size; }
    tg::isize2 target_size_half() const { return {_backbuf_size.width / 2, _backbuf_size.height / 2}; }

private:
    friend class GraphBuilder;
    setup_context(pass_idx pass, GraphBuilder* parent, tg::isize2 bb_s) : _pass(pass), _parent(parent), _backbuf_size(bb_s) {}
    setup_context(setup_context const&) = delete;
    setup_context(setup_context&&) = delete;

    pass_idx const _pass;
    GraphBuilder* const _parent;
    tg::isize2 const _backbuf_size;
};

struct exec_context
{
    physical_resource const& get(res_handle handle) const { return _parent->getPhysical(handle); }

    pr::buffer get_buffer(res_handle handle) const { return get(handle).as_buffer(); }

    pr::texture get_texture(res_handle handle) const { return get(handle).as_texture(); }

    pr::raii::Frame& frame() const { return *_frame; }

    pass_idx get_pass_index() const { return _pass; }

private:
    friend class GraphBuilder;
    exec_context(pass_idx pass, GraphBuilder* parent, pr::raii::Frame* frame) : _pass(pass), _parent(parent), _frame(frame) {}
    exec_context(exec_context const&) = delete;
    exec_context(exec_context&&) = delete;

    pass_idx const _pass;
    GraphBuilder* const _parent;
    pr::raii::Frame* const _frame;
};

template <class PassDataT, class ExecF>
pass_idx GraphBuilder::addPass(char const* debug_name, cc::function_ref<void(PassDataT&, setup_context&)> setup_func, ExecF&& exec_func)
{
    static_assert(std::is_invocable_r_v<void, ExecF, PassDataT const&, exec_context&>, "exec function has wrong signature");

    // create new pass
    pass_idx const new_pass_idx = pass_idx(mPasses.size());
    internal_pass& new_pass = mPasses.emplace_back(debug_name);

    // immediately execute setup
    PassDataT pass_data = {};
    setup_context setup_ctx = {new_pass_idx, this, mMainTargetSize};
    setup_func(pass_data, setup_ctx);

    // capture pass_data per value, move user exec lambda into capture
    new_pass.execute_func = cc::unique_function<void(exec_context&)>(
        [pass_data, user_func = cc::move(exec_func)](exec_context& exec_ctx) { user_func(pass_data, exec_ctx); }, cc::system_allocator);

    return new_pass_idx;
}
} // namespace inc::frag
