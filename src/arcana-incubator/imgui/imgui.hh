#pragma once

#include <arcana-incubator/imgui/lib/imgui.h>
// order relevant
#include <arcana-incubator/imgui/imguizmo/imguizmo.hh>

namespace inc
{
enum class imgui_theme
{
    classic_source_hl2,
    corporate_grey,
    photoshop_dark,
    light_green, // default theme in glow::viewer
};

void load_imgui_theme(imgui_theme theme);

}
