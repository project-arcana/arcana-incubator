#include "floodcull.hh"

#include <clean-core/alloc_vector.hh>

void inc::frag::run_floodcull(cc::span<int> producers,
                              cc::span<int> resources,
                              cc::span<const inc::frag::floodcull_relation> writes,
                              cc::span<const inc::frag::floodcull_relation> reads,
                              cc::allocator* alloc)
{
    // phase one - initial refcount values are expected as 0 (default) or 1 (root)
    {
        for (floodcull_relation const& write : writes)
        {
            producers[write.producer_index]++;
        }
        for (floodcull_relation const& read : reads)
        {
            if (producers[read.producer_index] == 0)
                continue;

            resources[read.resource_index]++;
        }
    }

    // phase two - floodfill from unreferenced resources
    {
        cc::alloc_vector<unsigned> residx_stack(alloc);
        residx_stack.reserve(resources.size() / 2);

        for (auto i = 0u; i < resources.size(); ++i)
            if (resources[i] == 0)
                residx_stack.push_back(i);

        while (residx_stack.size() > 0)
        {
            size_t popped = residx_stack.back();
            residx_stack.pop_back();

            CC_ASSERT(resources[popped] == 0 && "unexpected resource refcount");

            // go over producers of this resource
            for (floodcull_relation const& write : writes)
            {
                if (write.resource_index != popped)
                    continue;

                int& producer_refcount = producers[write.producer_index];
                producer_refcount--;

                CC_ASSERT(producer_refcount >= 0 && "producer at unexpected refcount");
                if (producer_refcount == 0)
                {
                    // go over reads of this pass
                    for (floodcull_relation const& read : reads)
                    {
                        if (read.producer_index != write.producer_index)
                            continue;

                        int& resource_refcount = resources[read.resource_index];
                        resource_refcount--;

                        //CC_ASSERT(resource_refcount > 0 && "read resource at unexpected refcount");

                        if (resource_refcount == 0)
                            residx_stack.push_back(read.resource_index);
                    }
                }
            }
        }
    }

    // phase three - write fixup
    // even if a written resource is not required down the line, it must not still not be culled
    // because the pass needs it (internally or simply for GPU layout / logistical reasons)
    for (floodcull_relation const& write : writes)
    {
        if (producers[write.producer_index] == 0)
            continue;

        int& written_res_refcount = resources[write.resource_index];
        if (written_res_refcount == 0)
            written_res_refcount = 1;
    }
}
