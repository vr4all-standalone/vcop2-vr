// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <algorithm>
#include <bit>
#include <cmath>
#include <cstdint>

namespace vcop2 {
inline constexpr uint32_t CameraFocusAddress=0x506044;
inline constexpr uint32_t CameraTangentAddress=0x506140;

// Original i960 camera routine 0x1be54 reads an animated horizontal FOV in
// degrees at 0x506034, submits TGP command 0x70, then stores tan(FOV/2) and
// 248/tan(FOV/2) at these two addresses. Leave that animation state intact:
// changing it would interfere with its completion tests at 0x1c8f4..1cb24.
// GEO focus submission and the native projected sphere hit test both consume
// the derived focal value; the distance/culling helper consumes its tangent.
template<class Read32> bool camera_revision_matches(Read32 read) {
    constexpr uint32_t words[][2]={
        {0x1be54,0x90183000},{0x1be58,0x00506034},
        {0x1be5c,0x8c783000},{0x1be60,0x38007070},
        {0x1be74,0x92203000},{0x1be78,0x00506140},
        {0x1be7c,0x92283000},{0x1be80,0x00506044},
        {0x15fb0,0x90803000},{0x15fb4,0x00506044},
        {0x15fe0,0x921ea090},{0x15fe4,0x92869c1c},{0x15fe8,0x928e9c1c},
        {0x15498,0x90583000},{0x1549c,0x00506140},
        {0x3a7d4,0x90403000},{0x3a7d8,0x00506044},
        {0x3a7ec,0x78210608},{0x3a7f0,0x78294608},
        {0x3a7f8,0x78210586},{0x3a800,0x785ac586},
        {0x3a80c,0x98a46008},{0x3a810,0x8c783000},
        {0x3a814,0x15802b2b},{0x3a840,0x6802e286}
    };
    for(const auto& word:words)if(uint32_t(read(word[0]))!=word[1])return false;
    return true;
}

template<class Read32> bool camera_world_phase(Read32 read) {
    const uint32_t mode=(uint32_t(read(0x500030))>>16)&255;
    const uint32_t submode=uint32_t(read(0x500038))&255;
    const uint32_t service=(uint32_t(read(0x5d051c))>>8)&255;
    // Mode 5 is the original 3D attract sequence. Mode 8 contains gameplay and
    // its surrounding cinematics; submode 2 is the native flat stage card.
    return service==0 && submode<=25 && (mode==5 || (mode==8 && submode!=2));
}

inline float camera_target_tangent(float horizontal_fov) {
    if(!std::isfinite(horizontal_fov)||horizontal_fov<35||horizontal_fov>150)return 0;
    return std::tan(horizontal_fov*(3.14159265358979323846f/360));
}

// Apply only to complete native float stores after a verified ROM match.
// The two independently written derived values widen in a reciprocal pair;
// a native view already wider than the target remains unchanged. No game code,
// animation target, camera position, player state or calibration is patched.
inline uint32_t widened_camera_write(uint32_t address,uint32_t value,uint32_t mask,
                                    float horizontal_fov,bool world_phase) {
    if(!world_phase||mask!=0xffffffffu)return value;
    const float tangent=camera_target_tangent(horizontal_fov);
    const float source=std::bit_cast<float>(value);
    if(tangent<=0||!std::isfinite(source)||source<=0)return value;
    if(address==CameraFocusAddress && source>=1 && source<=100000)
        return std::bit_cast<uint32_t>(std::min(source,248.f/tangent));
    if(address==CameraTangentAddress && source>=.00001f && source<=100)
        return std::bit_cast<uint32_t>(std::max(source,tangent));
    return value;
}

template<class Read32> uint32_t camera_write(uint32_t address,uint32_t value,uint32_t mask,
                                           Read32 read,float horizontal_fov) {
    if(!camera_revision_matches(read))return value;
    return widened_camera_write(address,value,mask,horizontal_fov,camera_world_phase(read));
}
struct NativeCameraZoom {float fov{},reference_fov{};};
template<class Read32> NativeCameraZoom read_native_camera_zoom(Read32 read) {
    // Reset routine loads the stage's original reference FOV, then writes both
    // current and target. These are native animation values, never widened.
    constexpr uint32_t words[][2]={
        {0x1b890,0x90183000},{0x1b894,0x00502010},{0x1b898,0x90183903},{0x1b89c,0x0001d364},
        {0x1b8a0,0x92183000},{0x1b8a4,0x00506034},{0x1b8a8,0x92183000},{0x1b8ac,0x00506038},
        {0x1d364,0x42080000},{0x1d368,0x42180000},{0x1d36c,0x42080000},
        {0x1d370,0x42080000},{0x1d374,0x42080000},{0x1d378,0x42080000},{0x1d37c,0x42080000}
    };
    if(!camera_revision_matches(read)||!camera_world_phase(read))return {};
    for(const auto& word:words)if(uint32_t(read(word[0]))!=word[1])return {};
    const uint32_t stage=read(0x502010);if(stage>=7)return {};
    const float fov=std::bit_cast<float>(uint32_t(read(0x506034)));
    const float reference=std::bit_cast<float>(uint32_t(read(0x1d364+stage*4)));
    // Native balcony cameras reach below five degrees; positive finite angles
    // are valid even during the deepest authored zoom.
    if(!std::isfinite(fov)||fov<=0||fov>150)return {};
    return {fov,reference};
}
}
