// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cmath>
#include <glm/gtc/matrix_transform.hpp>
#include <optional>
#include <array>
namespace vcop2 {
// Displayed zero is the former -10 degree pose. The caller supplies the
// existing palm/aim pivot; every gun, muzzle, menu ray and native aim shares it.
inline glm::mat4 pitched_gun_pose(const glm::mat4& pose,float displayed_angle) {
    const float angle=std::isfinite(displayed_angle)?std::clamp(displayed_angle,-30.f,30.f):0.f;
    return pose*glm::rotate(glm::mat4(1),glm::radians(angle-10.f),glm::vec3(1,0,0));
}

inline glm::vec3 native_gun_grip(unsigned weapon) {
    // Source-space metres: closed skin-volume centroid for the scene pistol;
    // authored handle-profile area centroids for the five pickup meshes.
    // In particular, exclude the machine gun's magazine and shotgun's barrel.
    static const std::array<glm::vec3,7> grips{{{},
        {.06839659f,.00209731f,.00852075f},
        {-.10663576f,-.10265571f,.0025f},
        {-.07158723f,-.04097231f,-.001f},
        {-.01233291f,-.08039945f,.001f},
        {-.32185672f,-.09179732f,0},
        {-.07509995f,-.06535676f,0}}};
    return weapon<grips.size()?grips[weapon]:glm::vec3(0);
}
inline glm::vec3 native_gun_muzzle(unsigned weapon) {
    // Centroid of the forward barrel cap; Magnum excludes sight and shroud.
    static const std::array<glm::vec3,7> tips{{{},
        {.252f,.0615f,.0015f}, {.526f,.0215f,.0025f},
        {.148f,.0465f,-.001f}, {.362f,.0155f,.0005f},
        {.471f,.027f,0}, {.171f,.03828449f,0}}};
    return weapon<tips.size()?tips[weapon]:glm::vec3(0);
}
// The aim origin is above/in front of the palm. Keep its barrel orientation,
// but anchor the model to the runtime's physical grip position.
inline glm::mat4 gun_pose(glm::mat4 aim,const std::optional<glm::vec3>& grip) {
    if(grip)aim[3]=glm::vec4(*grip,1);
    return aim;
}
inline glm::mat4 gun_model_transform(const glm::mat4& pose,float scale,int hand,const glm::mat4& placement) {
    // Reflect in controller space AFTER aligning the authored grip and barrel.
    // This swaps the embedded right hand without reversing the barrel.
    return pose*glm::scale(glm::mat4(1),glm::vec3(hand==0?-scale:scale,scale,scale))*placement;
}
}
