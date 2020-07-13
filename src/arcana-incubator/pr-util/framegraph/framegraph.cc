#include "framegraph.hh"

#include <cstdio>

#include <phantasm-hardware-interface/Backend.hh>

#include <phantasm-renderer/Context.hh>
#include <phantasm-renderer/Frame.hh>

#include <arcana-incubator/imgui/imgui.hh>

#include "resource_cache.hh"

inc::frag::virtual_resource_handle inc::frag::GraphBuilder::registerCreate(inc::frag::pass_idx pass_idx,
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
    ++pass.num_references; // increase refcount for each create

    return guidstate.get_handle();
}

inc::frag::virtual_resource_handle inc::frag::GraphBuilder::registerImport(inc::frag::pass_idx pass_idx,
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
    ++pass.num_references; // increase refcount for each import


    return guidstate.get_handle();
}

inc::frag::virtual_resource_handle inc::frag::GraphBuilder::registerWrite(inc::frag::pass_idx pass_idx, inc::frag::res_guid_t guid, inc::frag::access_mode mode)
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

inc::frag::virtual_resource_handle inc::frag::GraphBuilder::registerRead(inc::frag::pass_idx pass_idx, inc::frag::res_guid_t guid, inc::frag::access_mode mode)
{
    auto& guidstate = getGuidState(guid);
    CC_ASSERT(guidstate.is_valid() && "reading from resource without prior create");

    internal_pass& pass = mPasses[pass_idx];
    pass.reads.push_back({guidstate.virtual_res, guidstate.virtual_res_version, mode});

    virtual_resource_version& resver = getVirtualVersion(guidstate.virtual_res, guidstate.virtual_res_version);
    ++resver.num_references; // reads increase refcount for the RESOURCE

    return guidstate.get_handle();
}

inc::frag::virtual_resource_handle inc::frag::GraphBuilder::registerReadWrite(inc::frag::pass_idx pass_idx, inc::frag::res_guid_t guid, inc::frag::access_mode mode)
{
    registerRead(pass_idx, guid, mode);                  // the read takes care of the access_mode
    return registerWrite(pass_idx, guid, access_mode{}); // the write mode is omitted (null state)
}

inc::frag::virtual_resource_handle inc::frag::GraphBuilder::registerMove(inc::frag::pass_idx pass_idx, inc::frag::res_guid_t source_guid, inc::frag::res_guid_t dest_guid)
{
    auto const src_state = getGuidState(source_guid); // copy! a ref would stay alive beyond the next getGuidState, invalidating it
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

void inc::frag::GraphBuilder::realizePhysicalResources(inc::frag::GraphCache& cache)
{
    CC_ASSERT(mPhysicalResources.empty() && "ran twice");

    mPhysicalResources.reserve(mVirtualResources.size());
    for (auto& virt : mVirtualResources)
    {
        if (virt.is_culled)
            continue;

        // passthrough imported resources or call the realize_func
        pr::raw_resource const physical = virt.is_imported ? virt.imported_resource : cache.get(virt.create_info);

        mPhysicalResources.push_back({physical, virt.create_info.get()});
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
        if (pass.can_cull())
            continue;

        auto f_add_barrier = [&](virtual_res_idx virtual_res, access_mode mode) {
            if (!mode.is_set())
                return;

            CC_ASSERT(!mVirtualResources[virtual_res].is_culled && "a written or read resource was culled");

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

void inc::frag::GraphBuilder::startTiming(inc::frag::pass_idx pass, pr::raii::Frame* frame) { mTiming.begin_timing(*frame, pass); }

void inc::frag::GraphBuilder::endTiming(inc::frag::pass_idx pass, pr::raii::Frame* frame) { mTiming.end_timing(*frame, pass); }

void inc::frag::GraphBuilder::execute(pr::raii::Frame* frame)
{
    for (auto const& pass : mPasses)
    {
        if (pass.can_cull())
            continue;

        for (auto const& transition_pre : pass.transitions_before)
            frame->transition(transition_pre.resource, transition_pre.target_state, transition_pre.dependent_shaders);

        pass.execute_func(this, frame);
    }

    mTiming.finalize_frame(*frame);
}

void inc::frag::GraphBuilder::reset()
{
    mPasses.clear();
    mGuidStates.clear();
    mVirtualResources.clear();
    mVirtualResourceVersions.clear();
    mPhysicalResources.clear();
}

void inc::frag::GraphBuilder::performInfoImgui() const
{
    ImGui::Begin("Framegraph passes");

    ImGui::Text(": <pass name>, <#reads/writes/creates/imports> ... <time>");

    ImGui::Separator();

    unsigned num_culled = 0;
    for (pass_idx i = 0; i < mPasses.size(); ++i)
    {
        auto const& pass = mPasses[i];
        if (pass.can_cull())
        {
            ++num_culled;
            continue;
        }

        ImGui::Text("%c %-20s r%2d w%2d c%2d i%2d     ...     %.3fms", pass.is_root_pass ? '>' : ':', pass.debug_name, int(pass.writes.size()),
                    int(pass.reads.size()), int(pass.creates.size()), int(pass.imports.size()), mTiming.get_last_timing(i));
    }

    ImGui::Separator();
    ImGui::Text("%u culled", num_culled);
    ImGui::End();
}

void inc::frag::GraphBuilder::runFloodfillCulling()
{
    cc::vector<unsigned> unreferenced_res_ver_indices;
    unreferenced_res_ver_indices.reserve(mVirtualResourceVersions.size());

    // push all initially unreferenced resource versions on the stack
    for (auto i = 0u; i < mVirtualResourceVersions.size(); ++i)
    {
        if (mVirtualResourceVersions[i].can_cull())
            unreferenced_res_ver_indices.push_back(i);
    }

    // while the stack is nonempty
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
                {
                    // LOG("Culling producer {}, resource {}", producer.debug_name, read.res);
                    unreferenced_res_ver_indices.push_back(read_resource_version_index);
                }
            }
        }
    }

    // merge the per-version cull state into the resource itself (AND)
    for (auto const& res_ver : mVirtualResourceVersions)
    {
        auto& res = mVirtualResources[res_ver.res_idx];
        res.is_culled = res.is_culled && res_ver.can_cull();
    }

    //    for (auto const& virt : mVirtualResources)
    //    {
    //        if (virt.is_culled)
    //            LOG_INFO("Virtual resource {} was culled", virt.initial_guid);
    //    }
}


void inc::frag::GraphBuilder::initialize(pr::Context& ctx, pr::swapchain sc, unsigned max_num_passes)
{
    setBackbufferSize(ctx.get_backbuffer_size(sc));
    mTiming.initialize(ctx, max_num_passes);
    mPasses.reserve(max_num_passes);
}

void inc::frag::GraphBuilder::compile(inc::frag::GraphCache& cache)
{
    runFloodfillCulling();
    realizePhysicalResources(cache);
    calculateBarriers();
}

void inc::frag::GraphBuilder::printState() const
{
    for (auto const& pass : mPasses)
    {
        if (pass.can_cull())
            continue;

        LOG("pass {} ({} refs{})", pass.debug_name, pass.num_references, pass.is_root_pass ? ", root" : "");

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

    for (auto const& resver : mVirtualResourceVersions)
    {
        if (resver.can_cull())
            continue;

        LOG("resource {} at version {} ({} refs{})", resver.res_idx, resver.version, resver.num_references, resver.is_root_resource ? ", root" : "");
    }
}

inc::frag::virtual_res_idx inc::frag::GraphBuilder::addResource(inc::frag::pass_idx producer, inc::frag::res_guid_t guid, const phi::arg::create_resource_info& info)
{
    mVirtualResources.emplace_back(guid, info);
    auto const ret_idx = virtual_res_idx(mVirtualResources.size() - 1);
    addVirtualVersion(ret_idx, producer, 0);
    return ret_idx;
}

inc::frag::virtual_res_idx inc::frag::GraphBuilder::addResource(inc::frag::pass_idx producer,
                                                                inc::frag::res_guid_t guid,
                                                                pr::raw_resource import_resource,
                                                                pr::generic_resource_info const& info)
{
    mVirtualResources.emplace_back(guid, import_resource, info);
    auto const ret_idx = virtual_res_idx(mVirtualResources.size() - 1);
    addVirtualVersion(ret_idx, producer, 0);
    return ret_idx;
}

void inc::frag::GraphBuilder::addVirtualVersion(inc::frag::virtual_res_idx resource, inc::frag::pass_idx producer, int version)
{
    mVirtualResourceVersions.emplace_back(resource, producer, version);
}

inc::frag::virtual_resource_version& inc::frag::GraphBuilder::getVirtualVersion(inc::frag::virtual_res_idx resource, int version, unsigned* out_index)
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

inc::frag::GraphBuilder::guid_state& inc::frag::GraphBuilder::getGuidState(inc::frag::res_guid_t guid)
{
    for (auto& state : mGuidStates)
    {
        if (state.guid == guid)
            return state;
    }

    return mGuidStates.emplace_back(guid);
}

void inc::frag::virtual_resource::_copy_info(const phi::arg::create_resource_info& info)
{
    // this little game is required to maintain the hashable_storage
    create_info.get().type = info.type;

    switch (info.type)
    {
    case phi::arg::create_resource_info::e_resource_render_target:
        create_info.get().info_render_target = info.info_render_target;
        break;
    case phi::arg::create_resource_info::e_resource_texture:
        create_info.get().info_texture = info.info_texture;
        break;
    case phi::arg::create_resource_info::e_resource_buffer:
        create_info.get().info_buffer = info.info_buffer;
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

inc::frag::virtual_resource_handle inc::frag::GraphBuilder::guid_state::get_handle_and_bump_version()
{
    auto const res = get_handle();
    ++virtual_res_version;
    return res;
}
