#pragma once

#include <phantasm-hardware-interface/types.hh>

namespace phi
{
class Backend;
}

namespace inc
{
struct phi_mesh
{
    phi::handle::resource vertex_buffer;
    phi::handle::resource index_buffer;
    unsigned num_indices;
};

// loads a mesh, internally blocking (flushes GPU, resources immediately usable)
[[nodiscard]] phi_mesh load_mesh(phi::Backend& backend, char const* path, bool binary = false);

}
