#include "framegraph.hh"

#include <cinttypes>
#include <cstdio>

#include <clean-core/alloc_vector.hh>

#include <phantasm-hardware-interface/Backend.hh>

#include <phantasm-renderer/Context.hh>
#include <phantasm-renderer/Frame.hh>

#include <arcana-incubator/imgui/imgui.hh>

#include "floodcull.hh"
#include "resource_cache.hh"

inc::frag::res_handle inc::frag::GraphBuilder::registerCreate(inc::frag::pass_idx pass_idx,
                                                              inc::frag::res_guid_t guid,
                                                              const phi::arg::create_resource_info& info,
                                                              inc::frag::access_mode mode)
{
    CC_CONTRACT(info.type != phi::arg::create_resource_info::e_resource_undefined);
    auto const new_idx = addResource(pass_idx, guid, info);

    auto& guidstate = getGuidState(guid);
    CC_ASSERT(!guidstate.is_valid() && "resource guid was already created, imported, or moved to");
    guidstate.on_create(new_idx);

    internal_pass& pass = mPasses[pass_idx];
    pass.creates.push_back({new_idx, mode});
    ++mNumWritesTotal;

    return guidstate.get_handle();
}

inc::frag::res_handle inc::frag::GraphBuilder::registerImport(inc::frag::pass_idx pass_idx,
                                                              inc::frag::res_guid_t guid,
                                                              pr::raw_resource raw_resource,
                                                              inc::frag::access_mode mode,
                                                              const pr::generic_resource_info& optional_info)
{
    auto const new_idx = addResource(pass_idx, guid, raw_resource, optional_info);

    auto& guidstate = getGuidState(guid);
    CC_ASSERT(!guidstate.is_valid() && "resource guid was already created, imported or moved to");
    guidstate.on_create(new_idx);

    internal_pass& pass = mPasses[pass_idx];
    pass.imports.push_back({new_idx, mode});


    return guidstate.get_handle();
}

inc::frag::res_handle inc::frag::GraphBuilder::registerWrite(inc::frag::pass_idx pass_idx, inc::frag::res_guid_t guid, inc::frag::access_mode mode)
{
    auto& guidstate = getGuidState(guid);
    CC_ASSERT(guidstate.is_valid() && "writing to resource without prior create");

    internal_pass& pass = mPasses[pass_idx];

    ++mNumWritesTotal;
    pass.writes.push_back({guidstate.virtual_res, guidstate.virtual_res_version, mode});

    // writes increase resource version
    auto const return_handle = guidstate.get_handle_and_bump_version();
    return return_handle;
}

inc::frag::res_handle inc::frag::GraphBuilder::registerRead(inc::frag::pass_idx pass_idx, inc::frag::res_guid_t guid, inc::frag::access_mode mode)
{
    auto& guidstate = getGuidState(guid);
    CC_ASSERT(guidstate.is_valid() && "reading from resource without prior create");

    internal_pass& pass = mPasses[pass_idx];
    pass.reads.push_back({guidstate.virtual_res, guidstate.virtual_res_version, mode});
    ++mNumReadsTotal;

    return guidstate.get_handle();
}

inc::frag::res_handle inc::frag::GraphBuilder::registerReadWrite(inc::frag::pass_idx pass_idx, inc::frag::res_guid_t guid, inc::frag::access_mode mode)
{
    registerRead(pass_idx, guid, mode);                  // the read takes care of the access_mode
    return registerWrite(pass_idx, guid, access_mode{}); // the write mode is omitted (null state)
}

inc::frag::res_handle inc::frag::GraphBuilder::registerMove(inc::frag::pass_idx pass_idx, inc::frag::res_guid_t source_guid, inc::frag::res_guid_t dest_guid)
{
    auto const src_state = getGuidState(source_guid); // copy! a ref would stay alive beyond the next getGuidState, invalidating it
    CC_ASSERT(src_state.is_valid() && "moving from invalid resource");
    auto const src_idx = src_state.virtual_res;

    // add the move
    internal_pass& pass = mPasses[pass_idx];
    pass.moves.push_back({src_state.virtual_res, src_state.virtual_res_version, dest_guid});
    ++mNumWritesTotal;

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


void inc::frag::GraphBuilder::realizePhysicalResources(inc::frag::GraphCache& cache)
{
    CC_ASSERT(mPhysicalResources.empty() && "ran twice");

    mPhysicalResources.reserve(mVirtualResources.size());
    for (auto& virt : mVirtualResources)
    {
        if (virt.is_culled())
            continue;

        // passthrough imported resources or call the realize_func
        pr::raw_resource physical;
        if (virt.is_imported())
        {
            physical = virt.imported_resource;
        }
        else
        {
            char namebuf[256];
            std::snprintf(namebuf, sizeof(namebuf), "[fgraph-guid:%" PRIu64 "]", virt.initial_guid);
            physical = cache.get(virt.resource_info, namebuf);
        }

        mPhysicalResources.push_back({physical, virt.resource_info.get()});
        virt.associated_physical = physical_res_idx(mPhysicalResources.size() - 1);
    }
}


void inc::frag::GraphBuilder::calculateBarriers()
{
    // this is extremely naive right now
    // things to eventually consider:
    // - batching accross passes
    // - split barriers (requires phi features)
    // - lowest-common-denominator states (might require vk/d3d12 specific implementations)
    // - separate invalidate/flush barriers
    // - compatibility with eventual aliasing
    // - cross-queue concerns


    for (auto& pass : mPasses)
    {
        if (pass.is_culled)
            continue;

        auto f_add_barrier = [&](virtual_res_idx virtual_res, access_mode mode) {
            if (!mode.is_set())
                return;

            CC_ASSERT(!mVirtualResources[virtual_res].is_culled() && "a written or read resource was culled");

            CC_ASSERT(virtual_res != gc_invalid_virtual_res);
            auto const physical_idx = mVirtualResources[virtual_res].associated_physical;
            CC_ASSERT(physical_idx != gc_invalid_physical_res);
            auto const res = mPhysicalResources[physical_idx].raw_res.handle;
            pass.transitions_before.push_back({res, mode.required_state, mode.stage_flags});
        };

        for (auto const& read : pass.reads)
            f_add_barrier(read.res, read.mode);

        for (auto const& write : pass.writes)
            f_add_barrier(write.res, write.mode);

        for (auto const& create : pass.creates)
            f_add_barrier(create.res, create.mode);
    }
}

void inc::frag::GraphBuilder::execute(pr::raii::Frame* frame, inc::pre::timestamp_bundle* timing)
{
    CC_CONTRACT(frame != nullptr);
    CC_CONTRACT(timing != nullptr);

    for (auto i = 0u; i < mPasses.size(); ++i)
    {
        auto const& pass = mPasses[i];
        if (pass.is_culled)
            continue;

        for (auto const& transition_pre : pass.transitions_before)
            frame->transition(transition_pre.resource, transition_pre.target_state, transition_pre.dependent_shaders);

        {
            frame->begin_debug_label(pass.debug_name);
            timing->begin_timing(*frame, i);

            exec_context exec_ctx = {i, this, frame};
            pass.execute_func(exec_ctx);

            timing->end_timing(*frame, i);
            frame->end_debug_label();
        }
    }

    timing->finalize_frame(*frame);
}

void inc::frag::GraphBuilder::reset()
{
    mPasses.clear();
    mGuidStates.clear();
    mVirtualResources.clear();
    mPhysicalResources.clear();
    mNumReadsTotal = 0;
    mNumWritesTotal = 0;
}

void inc::frag::GraphBuilder::performInfoImgui(const pre::timestamp_bundle* timing) const
{
    if (ImGui::Begin("Framegraph Timings"))
    {
        ImGui::Text(": <pass name>, <#reads/writes/creates/imports> ... <time>");

        ImGui::Separator();

        unsigned num_culled = 0;
        unsigned num_root = 0;
        for (pass_idx i = 0; i < mPasses.size(); ++i)
        {
            auto const& pass = mPasses[i];
            if (pass.is_culled)
                ++num_culled;
            else if (pass.is_root_pass)
                ++num_root;

            ImGui::Text("%c %-20s r%2d w%2d c%2d i%2d     ...     %.3fms", pass.is_culled ? 'X' : (pass.is_root_pass ? '>' : ':'), pass.debug_name,
                        int(pass.writes.size()), int(pass.reads.size()), int(pass.creates.size()), int(pass.imports.size()), timing->get_last_timing(i));
        }

        ImGui::Separator();
        ImGui::Text("%u culled (X), %u root (>)", num_culled, num_root);
    }
    ImGui::End();
}

void inc::frag::GraphBuilder::runFloodfillCulling(cc::allocator* alloc)
{
    auto pass_refcounts = cc::alloc_vector<int>::filled(mPasses.size(), 0, alloc);
    auto res_refcounts = cc::alloc_vector<int>::filled(mVirtualResources.size(), 0, alloc);

    cc::alloc_vector<floodcull_relation> writes(alloc);
    writes.reserve(mNumWritesTotal);

    cc::alloc_vector<floodcull_relation> reads(alloc);
    reads.reserve(mNumReadsTotal);

    // mark root passes, fill reads and writes
    for (auto i = 0u; i < mPasses.size(); ++i)
    {
        if (mPasses[i].is_root_pass)
            pass_refcounts[i] = 1;

        // writes
        for (auto const& wr : mPasses[i].writes)
        {
            writes.push_back(floodcull_relation{i, wr.res});
        }
        for (auto const& cr : mPasses[i].creates)
        {
            writes.push_back(floodcull_relation{i, cr.res});
        }
        for (auto const& mv : mPasses[i].moves)
        {
            // moving X somewhere acts like a write to X
            // this way, subsequent reads from X keep the mover alive
            // NOTE: this means that moving a used resource to an unused GUID
            // will keep the pass alive!
            writes.push_back(floodcull_relation{i, mv.src_res});
        }
        // reads
        for (auto const& rd : mPasses[i].reads)
        {
            reads.push_back(floodcull_relation{i, rd.res});
        }
    }

    // mark root resources
    for (auto i = 0u; i < mVirtualResources.size(); ++i)
    {
        if (mVirtualResources[i].is_root())
            res_refcounts[i] = 1;
    }

    // run the the algorithm
    run_floodcull(pass_refcounts, res_refcounts, writes, reads, alloc);

    // mark culled resources and passes
    for (auto i = 0u; i < mPasses.size(); ++i)
    {
        mPasses[i].is_culled = (pass_refcounts[i] == 0);
    }

    for (auto i = 0u; i < mVirtualResources.size(); ++i)
    {
        if (res_refcounts[i] == 0)
            mVirtualResources[i].state |= virtual_resource::sb_culled;
    }
}


void inc::frag::GraphBuilder::initialize(cc::allocator* alloc, unsigned max_num_passes, unsigned max_num_guids)
{
    mPasses.reset_reserve(alloc, max_num_passes);
    mGuidStates.reset_reserve(alloc, max_num_guids);
    mVirtualResources.reset_reserve(alloc, max_num_guids);
    mPhysicalResources.reset_reserve(alloc, max_num_guids);
}

void inc::frag::GraphBuilder::destroy() { reset(); }

void inc::frag::GraphBuilder::compile(inc::frag::GraphCache& cache, cc::allocator* alloc)
{
    runFloodfillCulling(alloc);
    realizePhysicalResources(cache);
    calculateBarriers();
}

void inc::frag::GraphBuilder::printState() const
{
    for (auto const& pass : mPasses)
    {
        if (pass.is_culled)
            continue;

        LOG("pass {}{}", pass.debug_name, pass.is_root_pass ? ", root" : "");

        for (auto const& create : pass.creates)
        {
            LOG("  <* create {} v0", create.res);
        }
        for (auto const& import : pass.imports)
        {
            LOG("  <# import {} v0", import.res);
        }
        for (auto const& read : pass.reads)
        {
            LOG("  -> read {} v{}", read.res, read.version_before);
        }
        for (auto const& write : pass.writes)
        {
            LOG("  <- write {} v{}", write.res, write.version_before);
        }
        for (auto const& move : pass.moves)
        {
            LOG("  -- move {} v{} -> g{}", move.src_res, move.src_version_before, move.dest_guid);
        }
    }

    for (auto const& guidstate : mGuidStates)
    {
        LOG("guid {}, state {}, virtual res {}, v{}", guidstate.guid, guidstate.state, guidstate.virtual_res, guidstate.virtual_res_version);
    }

    for (auto const& res : mVirtualResources)
    {
        if (res.is_culled())
            continue;

        LOG("resource {}", res.initial_guid);
    }
}

inc::frag::virtual_res_idx inc::frag::GraphBuilder::addResource(inc::frag::pass_idx producer, inc::frag::res_guid_t guid, const phi::arg::create_resource_info& info)
{
    (void)producer; // might be useful later
    mVirtualResources.emplace_back(guid, info);
    return virtual_res_idx(mVirtualResources.size() - 1);
}

inc::frag::virtual_res_idx inc::frag::GraphBuilder::addResource(inc::frag::pass_idx producer,
                                                                inc::frag::res_guid_t guid,
                                                                pr::raw_resource import_resource,
                                                                pr::generic_resource_info const& info)
{
    (void)producer; // might be useful later
    mVirtualResources.emplace_back(guid, import_resource, info);
    return virtual_res_idx(mVirtualResources.size() - 1);
}

inc::frag::GraphBuilder::guid_state& inc::frag::GraphBuilder::getGuidState(inc::frag::res_guid_t guid)
{
    for (auto& state : mGuidStates)
    {
        if (state.guid == guid)
            return state;
    }

    return mGuidStates.emplace_back(guid);
}

void inc::frag::GraphBuilder::virtual_resource::_copy_info(const phi::arg::create_resource_info& info)
{
    // this little game is required to maintain the hashable_storage
    resource_info.get().type = info.type;

    switch (info.type)
    {
    case phi::arg::create_resource_info::e_resource_render_target:
        resource_info.get().info_render_target = info.info_render_target;
        break;
    case phi::arg::create_resource_info::e_resource_texture:
        resource_info.get().info_texture = info.info_texture;
        break;
    case phi::arg::create_resource_info::e_resource_buffer:
        resource_info.get().info_buffer = info.info_buffer;
        break;
    case phi::arg::create_resource_info::e_resource_undefined:
        break;
    }
}

void inc::frag::GraphBuilder::guid_state::on_create(inc::frag::virtual_res_idx new_res)
{
    CC_ASSERT(!is_valid());
    state = state_valid_created;
    virtual_res = new_res;
}

void inc::frag::GraphBuilder::guid_state::on_move_create(inc::frag::virtual_res_idx new_res, int new_version)
{
    CC_ASSERT(!is_valid());
    state = state_valid_move_created;
    virtual_res = new_res;
    virtual_res_version = new_version;
}

void inc::frag::GraphBuilder::guid_state::on_move_modify(inc::frag::virtual_res_idx new_res, int new_version)
{
    CC_ASSERT(is_valid());
    state = state_valid_moved_to;
    virtual_res = new_res;
    virtual_res_version = new_version;
}

inc::frag::res_handle inc::frag::GraphBuilder::guid_state::get_handle_and_bump_version()
{
    auto const res = get_handle();
    ++virtual_res_version;
    return res;
}
