// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "projection.h"
#include <cmath>
#include <limits>
namespace vcop2 {
inline double native_zoom_factor(float fov,float reference_fov,bool enabled) {
    if(!enabled||!std::isfinite(fov)||!std::isfinite(reference_fov)||
       fov<=0||fov>150||reference_fov<=0||reference_fov>150)return 1;
    constexpr double radians=3.14159265358979323846/360;
    const double factor=std::tan(double(reference_fov)*radians)/std::tan(double(fov)*radians);
    return std::isfinite(factor)&&factor>0&&factor<=std::numeric_limits<float>::max()?factor:1;
}
// Magnify in the original game-camera basis, before the unchanged eye pose.
// Head rotation, IPD and depth therefore retain their physical tracking scale.
inline Vec3 zoomed_world_position(Vec3 p,double zoom){return {p.x*zoom,p.y*zoom,p.z};}
inline Vec3 unzoomed_world_position(Vec3 p,double zoom){return {p.x/zoom,p.y/zoom,p.z};}
}
