#pragma once

#include <cstdint>

#include <typed-geometry/tg-lean.hh>

#include <phantasm-hardware-interface/handles.hh>

namespace inc::pre::dmr
{
namespace handle
{
PHI_DEFINE_HANDLE(mesh);
PHI_DEFINE_HANDLE(material);
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


struct AssetPack;
struct camera_gpudata;
struct frame_index_state;
}
