// https://github.com/CedricGuillemet/ImGuizmo
// v 1.61 WIP
//
// The MIT License(MIT)
//
// Copyright(c) 2016 Cedric Guillemet
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files(the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and / or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions :
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
//
// -------------------------------------------------------------------------------------------
// History :
// 2019/11/03 View gizmo
// 2016/09/11 Behind camera culling. Scaling Delta matrix not multiplied by source matrix scales. local/world rotation and translation fixed. Display
// message is incorrect (X: ... Y:...) in local mode. 2016/09/09 Hatched negative axis. Snapping. Documentation update. 2016/09/04 Axis switch and
// translation plan autohiding. Scale transform stability improved 2016/09/01 Mogwai changed to Manipulate. Draw debug cube. Fixed inverted scale.
// Mixing scale and translation/rotation gives bad results. 2016/08/31 First version
//
// -------------------------------------------------------------------------------------------
// Future (no order):
//
// - Multi view
// - display rotation/translation/scale infos in local/world space and not only local
// - finish local/world matrix application
// - OPERATION as bitmask
//
// -------------------------------------------------------------------------------------------
// Example
#if 0
void EditTransform(const Camera& camera, matrix_t& matrix)
{
        static ImGuizmo::OPERATION mCurrentGizmoOperation(ImGuizmo::ROTATE);
        static ImGuizmo::MODE mCurrentGizmoMode(ImGuizmo::WORLD);
        if (ImGui::IsKeyPressed(90))
                mCurrentGizmoOperation = ImGuizmo::TRANSLATE;
        if (ImGui::IsKeyPressed(69))
                mCurrentGizmoOperation = ImGuizmo::ROTATE;
        if (ImGui::IsKeyPressed(82)) // r Key
                mCurrentGizmoOperation = ImGuizmo::SCALE;
        if (ImGui::RadioButton("Translate", mCurrentGizmoOperation == ImGuizmo::TRANSLATE))
                mCurrentGizmoOperation = ImGuizmo::TRANSLATE;
        ImGui::SameLine();
        if (ImGui::RadioButton("Rotate", mCurrentGizmoOperation == ImGuizmo::ROTATE))
                mCurrentGizmoOperation = ImGuizmo::ROTATE;
        ImGui::SameLine();
        if (ImGui::RadioButton("Scale", mCurrentGizmoOperation == ImGuizmo::SCALE))
                mCurrentGizmoOperation = ImGuizmo::SCALE;
        float matrixTranslation[3], matrixRotation[3], matrixScale[3];
        ImGuizmo::DecomposeMatrixToComponents(matrix.m16, matrixTranslation, matrixRotation, matrixScale);
        ImGui::InputFloat3("Tr", matrixTranslation, 3);
        ImGui::InputFloat3("Rt", matrixRotation, 3);
        ImGui::InputFloat3("Sc", matrixScale, 3);
        ImGuizmo::RecomposeMatrixFromComponents(matrixTranslation, matrixRotation, matrixScale, matrix.m16);

        if (mCurrentGizmoOperation != ImGuizmo::SCALE)
        {
                if (ImGui::RadioButton("Local", mCurrentGizmoMode == ImGuizmo::LOCAL))
                        mCurrentGizmoMode = ImGuizmo::LOCAL;
                ImGui::SameLine();
                if (ImGui::RadioButton("World", mCurrentGizmoMode == ImGuizmo::WORLD))
                        mCurrentGizmoMode = ImGuizmo::WORLD;
        }
        static bool useSnap(false);
        if (ImGui::IsKeyPressed(83))
                useSnap = !useSnap;
        ImGui::Checkbox("", &useSnap);
        ImGui::SameLine();
        vec_t snap;
        switch (mCurrentGizmoOperation)
        {
        case ImGuizmo::TRANSLATE:
                snap = config.mSnapTranslation;
                ImGui::InputFloat3("Snap", &snap.x);
                break;
        case ImGuizmo::ROTATE:
                snap = config.mSnapRotation;
                ImGui::InputFloat("Angle Snap", &snap.x);
                break;
        case ImGuizmo::SCALE:
                snap = config.mSnapScale;
                ImGui::InputFloat("Scale Snap", &snap.x);
                break;
        }
        ImGuiIO& io = ImGui::GetIO();
        ImGuizmo::SetRect(0, 0, io.DisplaySize.x, io.DisplaySize.y);
        ImGuizmo::Manipulate(camera.mView.m16, camera.mProjection.m16, mCurrentGizmoOperation, mCurrentGizmoMode, matrix.m16, NULL, useSnap ? &snap.x : NULL);
}
#endif
#pragma once

#ifdef USE_IMGUI_API
#include "imconfig.h"
#endif
#ifndef IMGUI_API
#define IMGUI_API
#endif

namespace ImGuizmo
{
// call inside your own window and before Manipulate() in order to draw gizmo to that window.
IMGUI_API void SetDrawlist();

// call BeginFrame right after ImGui_XXXX_NewFrame();
IMGUI_API void BeginFrame();

// return true if mouse cursor is over any gizmo control (axis, plan or screen component)
IMGUI_API bool IsOver();

// return true if mouse IsOver or if the gizmo is in moving state
IMGUI_API bool IsUsing();

// enable/disable the gizmo. Stay in the state until next call to Enable.
// gizmo is rendered with gray half transparent color when disabled
IMGUI_API void Enable(bool enable);

// helper functions for manualy editing translation/rotation/scale with an input float
// translation, rotation and scale float points to 3 floats each
// Angles are in degrees (more suitable for human editing)
// example:
// float matrixTranslation[3], matrixRotation[3], matrixScale[3];
// ImGuizmo::DecomposeMatrixToComponents(gizmoMatrix.m16, matrixTranslation, matrixRotation, matrixScale);
// ImGui::InputFloat3("Tr", matrixTranslation, 3);
// ImGui::InputFloat3("Rt", matrixRotation, 3);
// ImGui::InputFloat3("Sc", matrixScale, 3);
// ImGuizmo::RecomposeMatrixFromComponents(matrixTranslation, matrixRotation, matrixScale, gizmoMatrix.m16);
//
// These functions have some numerical stability issues for now. Use with caution.
IMGUI_API void DecomposeMatrixToComponents(const float* matrix, float* translation, float* rotation, float* scale);
IMGUI_API void RecomposeMatrixFromComponents(const float* translation, const float* rotation, const float* scale, float* matrix);

IMGUI_API void SetRect(float x, float y, float width, float height);

// Set the colors for the directions (usually red, green, blue)
IMGUI_API void SetDirectionColors(unsigned colorX, unsigned colorY, unsigned colorZ);
// Set the color for the dotted effect if a translation axis is negated (0 = disable). Default: 0x80000000
IMGUI_API void SetNegativeAxisHatchColor(unsigned color);

// default is false
IMGUI_API void SetOrthographic(bool isOrthographic);

// Render a cube with face color corresponding to face normal. Usefull for debug/tests
IMGUI_API void DrawCubes(const float* view, const float* projection, const float* matrices, int matrixCount);
IMGUI_API void DrawGrid(const float* view, const float* projection, const float* matrix, const float gridSize);

// call it when you want a gizmo
// Needs view and projection matrices.
// matrix parameter is the source matrix (where will be gizmo be drawn) and might be transformed by the function. Return deltaMatrix is optional
// translation is applied in world space
enum OPERATION
{
    TRANSLATE,
    ROTATE,
    SCALE,
    BOUNDS,
};

enum MODE
{
    LOCAL,
    WORLD
};

IMGUI_API bool Manipulate(const float* __restrict view,
                          const float* __restrict projection,
                          OPERATION operation,
                          MODE mode,
                          float* __restrict matrix,
                          float* __restrict deltaMatrix = nullptr,
                          float const* __restrict snap = nullptr,
                          float const* __restrict localBounds = nullptr,
                          float const* __restrict boundsSnap = nullptr);

// returns true if the user started dragging the gizmo this frame
IMGUI_API bool IsGizmoActivated();

// returns true if the user stopped dragging the gizmo this frame
IMGUI_API bool IsGizmoDeactivatedAfterDrag();

//
// Please note that this cubeview is patented by Autodesk : https://patents.google.com/patent/US7782319B2/en
// It seems to be a defensive patent in the US. I don't think it will bring troubles using it as
// other software are using the same mechanics. But just in case, you are now warned!
//
IMGUI_API void ViewManipulate(float* view, float length, ImVec2 position, ImVec2 size, ImU32 backgroundColor);

IMGUI_API void SetID(int id);
}; // namespace ImGuizmo
