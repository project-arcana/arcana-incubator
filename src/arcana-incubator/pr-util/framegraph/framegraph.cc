#include "framegraph.hh"

#include <cinttypes>
#include <cstdio>

#include <clean-core/alloc_vector.hh>

#include <rich-log/log.hh>

#include <phantasm-hardware-interface/Backend.hh>

#include <phantasm-renderer/Context.hh>
#include <phantasm-renderer/Frame.hh>

#include <arcana-incubator/imgui/imgui.hh>

#include "floodcull.hh"
#include "resource_cache.hh"

inc::frag::res_handle inc::frag::GraphBuilder::registerCreate(inc::frag::pass_idx pass_idx,
                                                              inc::frag::res_guid_t guid,
                                                              const phi::arg::resource_description& info,
                                                              inc::frag::access_mode mode)
{
    CC_CONTRACT(info.type != phi::arg::resource_description::e_resource_undefined);
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
                                                              pr::resource raw_resource,
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
        {
            // RICH_LOG_INFO("skipped virtual resource with initial GUID {} (culled)", virt.initial_guid);
            continue;
        }

        // passthrough imported resources or call the realize_func
        pr::resource physical;
        if (virt.is_imported())
        {
            physical = virt.imported_resource;
            // RICH_LOG_INFO("imported virtual resource with initial GUID {}", virt.initial_guid);
        }
        else
        {
            char namebuf[256];
            std::snprintf(namebuf, sizeof(namebuf), "[fgraph-guid:%" PRIu64 "]", virt.initial_guid);
            physical = cache.get(virt.resource_info, namebuf);
            // RICH_LOG_INFO("cache-realized virtual resource with initial GUID {}", virt.initial_guid);
        }

        mPhysicalResources.push_back({physical, virt.resource_info});
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

        auto f_add_barrier = [&](virtual_res_idx virtual_res, access_mode mode)
        {
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

void inc::frag::GraphBuilder::executePasses(pr::raii::Frame* frame, size_t start, size_t end, pre::timestamp_bundle* timing, int timer_offset)
{
    CC_CONTRACT(frame != nullptr);
    CC_ASSERT(end <= mPasses.size() && "pass amount out of bounds");

    for (auto i = start; i < end; ++i)
    {
        auto const& pass = mPasses[i];
        if (pass.is_culled)
            continue;

        for (auto const& transition_pre : pass.transitions_before)
            frame->transition(transition_pre.resource, transition_pre.target_state, transition_pre.dependent_shaders);

        {
            frame->begin_debug_label(pass.debug_name);
            if (timing)
                timing->begin_timing(*frame, i + timer_offset);

            exec_context exec_ctx = {(pass_idx)i, this, frame};
            pass.execute_func(exec_ctx);

            if (timing)
                timing->end_timing(*frame, i + timer_offset);
            frame->end_debug_label();
        }
    }
}

void inc::frag::GraphBuilder::execute(pr::raii::Frame* frame, inc::pre::timestamp_bundle* timing, int timer_offset)
{
    executePasses(frame, 0, mPasses.size(), timing, timer_offset);
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

void inc::frag::GraphBuilder::performInfoImgui(const pre::timestamp_bundle* timing, bool* isWindowOpen) const
{
    if (isWindowOpen && !*isWindowOpen)
    {
        return;
    }

    if (ImGui::Begin("Framegraph Timings", isWindowOpen))
    {
        ImGuiTableFlags const tableFlags
            = ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_Hideable;
        unsigned num_culled = 0;
        unsigned num_root = 0;
        float time_sum = 0.f;

        if (ImGui::BeginTable("Passes", 9, tableFlags))
        {
            ImGui::TableSetupColumn("Idx", 0, 25.f);
            ImGui::TableSetupColumn("Name");
            ImGui::TableSetupColumn("Reads");
            ImGui::TableSetupColumn("Writes");
            ImGui::TableSetupColumn("Creates");
            ImGui::TableSetupColumn("Imports");
            ImGui::TableSetupColumn("Time", 0, 60.f);
            ImGui::TableSetupColumn("Root", 0, 25.f);
            ImGui::TableSetupColumn("Culled", 0, 45.f);
            ImGui::TableHeadersRow();

            for (pass_idx i = 0; i < mPasses.size(); ++i)
            {
                ImGui::PushID(i);
                ImGui::TableNextRow();

                auto const& pass = mPasses[i];
                if (pass.is_culled)
                    ++num_culled;
                else if (pass.is_root_pass)
                    ++num_root;


                ImGui::TableSetColumnIndex(0);
                ImGui::Text("%u", i);

                ImGui::TableSetColumnIndex(1);
                ImGui::TextUnformatted(pass.debug_name);

                ImGui::TableSetColumnIndex(2);
                ImGui::Text("%2d", int(pass.reads.size()));

                ImGui::TableSetColumnIndex(3);
                ImGui::Text("%2d", int(pass.writes.size()));

                ImGui::TableSetColumnIndex(4);
                ImGui::Text("%2d", int(pass.creates.size()));

                ImGui::TableSetColumnIndex(5);
                ImGui::Text("%2d", int(pass.imports.size()));

                ImGui::TableSetColumnIndex(6);
                float const time = timing ? timing->get_last_timing(i) : 0.f;
                time_sum += time;
                ImGui::Text("% 2.3fms", time);

                ImGui::TableSetColumnIndex(7);
                bool isRoot = pass.is_root_pass;
                ImGui::Checkbox("##isRoot", &isRoot);

                ImGui::TableSetColumnIndex(8);
                bool isCulled = pass.is_culled;
                ImGui::Checkbox("##isCulled", &isCulled);


                ImGui::PopID();
            }

            ImGui::EndTable();
        }

        ImGui::Text("%u passes culled, %u root passes, total time: % 2.3fms", num_culled, num_root, time_sum);
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
        for (auto const& im : mPasses[i].imports)
        {
            writes.push_back(floodcull_relation{i, im.res});
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

void inc::frag::GraphBuilder::destroy()
{
    reset();
}

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

        RICH_LOG("pass {}{}", pass.debug_name, pass.is_root_pass ? ", root" : "");

        for (auto const& create : pass.creates)
        {
            RICH_LOG("  <* create {} v0", create.res);
        }
        for (auto const& import : pass.imports)
        {
            RICH_LOG("  <# import {} v0", import.res);
        }
        for (auto const& read : pass.reads)
        {
            RICH_LOG("  -> read {} v{}", read.res, read.version_before);
        }
        for (auto const& write : pass.writes)
        {
            RICH_LOG("  <- write {} v{}", write.res, write.version_before);
        }
        for (auto const& move : pass.moves)
        {
            RICH_LOG("  -- move {} v{} -> g{}", move.src_res, move.src_version_before, move.dest_guid);
        }
    }

    for (auto const& guidstate : mGuidStates)
    {
        RICH_LOG("guid {}, state {}, virtual res {}, v{}", guidstate.guid, guidstate.state, guidstate.virtual_res, guidstate.virtual_res_version);
    }

    for (auto const& res : mVirtualResources)
    {
        if (res.is_culled())
            continue;

        RICH_LOG("resource {}", res.initial_guid);
    }
}

inc::frag::virtual_res_idx inc::frag::GraphBuilder::addResource(inc::frag::pass_idx producer, inc::frag::res_guid_t guid, const phi::arg::resource_description& info)
{
    (void)producer; // might be useful later
    mVirtualResources.emplace_back(guid, info);
    return virtual_res_idx(mVirtualResources.size() - 1);
}

inc::frag::virtual_res_idx inc::frag::GraphBuilder::addResource(inc::frag::pass_idx producer,
                                                                inc::frag::res_guid_t guid,
                                                                pr::resource import_resource,
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
