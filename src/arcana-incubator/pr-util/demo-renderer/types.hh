#pragma once

#include <typed-geometry/tg-lean.hh>

namespace inc::pre::dmr
{
// handles
namespace handle
{
struct material
{
    unsigned idx = unsigned(-1);
};
struct mesh
{
    unsigned idx = unsigned(-1);
};
}
struct instance
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
