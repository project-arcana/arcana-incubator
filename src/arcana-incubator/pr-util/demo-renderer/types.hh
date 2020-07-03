#pragma once

#include <cstdint>

#include <typed-geometry/tg-lean.hh>

namespace inc::pre::dmr
{
// handles
namespace handle
{
struct material
{
    uint32_t idx = uint32_t(-1);
};
struct mesh
{
    uint32_t idx = uint32_t(-1);
};
}
struct instance_cpu
{
    handle::mesh mesh;
    handle::material mat;
};

struct instance_gpudata
{
    tg::mat4 model;
    tg::mat4 prev_model;

    instance_gpudata(tg::mat4 initial_transform) : model(initial_transform), prev_model(initial_transform) {}
};
}
