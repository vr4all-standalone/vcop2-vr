// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <array>
#include <cmath>
#include <optional>

namespace vcop2 {
struct Vec3 {
    double x{}, y{}, z{};
    Vec3 operator+(Vec3 b) const { return {x+b.x,y+b.y,z+b.z}; }
    Vec3 operator-(Vec3 b) const { return {x-b.x,y-b.y,z-b.z}; }
    Vec3 operator*(double s) const { return {x*s,y*s,z*s}; }
};
inline double dot(Vec3 a, Vec3 b) { return a.x*b.x+a.y*b.y+a.z*b.z; }
inline Vec3 cross(Vec3 a, Vec3 b) { return {a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x}; }
inline bool finite(Vec3 v) { return std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z); }
struct Ray { Vec3 origin, direction; };
struct AimPoint { bool valid{}, inside{}; double u{}, v{}; };

// Camera coordinates use metres (or a uniform game-unit scale), +Y up, -Z forward.
// Callers must supply a measured lens. No game-specific camera constants are guessed here.
struct NativeLens {
    int width{}, height{};
    double fx{}, fy{}, cx{}, cy{};
    bool valid() const {
        return width>1 && height>1 && fx>0 && fy>0 && std::isfinite(fx)
            && std::isfinite(fy) && std::isfinite(cx) && std::isfinite(cy);
    }
    AimPoint project_point(Vec3 point) const {
        if (!valid() || !finite(point) || point.z>=-1e-9) return {};
        const double u=(fx*point.x/-point.z+cx)/(width-1);
        const double v=(-fy*point.y/-point.z+cy)/(height-1);
        if (!std::isfinite(u)||!std::isfinite(v)) return {};
        return {true,u>=0 && u<=1 && v>=0 && v<=1,u,v};
    }
    Vec3 unproject(double pixel_x, double pixel_y, double depth) const {
        if (!valid() || !std::isfinite(depth) || depth<=0
            || !std::isfinite(pixel_x) || !std::isfinite(pixel_y)) return {NAN,NAN,NAN};
        return {(pixel_x-cx)*depth/fx,-(pixel_y-cy)*depth/fy,-depth};
    }
};

inline std::optional<Vec3> intersect_triangle(Ray ray, const std::array<Vec3,3>& vertex) {
    if (!finite(ray.origin)||!finite(ray.direction)||!finite(vertex[0])
        ||!finite(vertex[1])||!finite(vertex[2])) return {};
    const Vec3 e1=vertex[1]-vertex[0], e2=vertex[2]-vertex[0];
    const Vec3 p=cross(ray.direction,e2);
    const double det=dot(e1,p);
    if (std::abs(det)<1e-12) return {};
    const double inv=1./det;
    const Vec3 t=ray.origin-vertex[0];
    const double u=dot(t,p)*inv;
    if (u<0 || u>1) return {};
    const Vec3 q=cross(t,e1);
    const double v=dot(ray.direction,q)*inv;
    if (v<0 || u+v>1) return {};
    const double distance=dot(e2,q)*inv;
    if (!std::isfinite(distance) || distance<1e-9) return {};
    return ray.origin+ray.direction*distance;
}
}
