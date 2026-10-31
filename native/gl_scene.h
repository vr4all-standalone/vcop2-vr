// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "model2_capture.h"
#include <glm/glm.hpp>
#include <memory>
namespace vcop2 {
class GLScene {
    struct Impl;std::unique_ptr<Impl> impl_;
public:
    struct UploadStats {
        std::array<uint64_t,6> materials{};
        std::array<uint64_t,3> rasters{};
        uint64_t sky{},bytes{};
        bool operator==(const UploadStats&) const=default;
    };
    GLScene();~GLScene();
    UploadStats upload_stats() const;
    void prepare(std::shared_ptr<const NativeFrame> frame,bool wider_view=false);
    void set_immersive(bool enabled);
    void set_presentation_lens(const NativeLens& lens);
    void draw(const glm::mat4& view_projection,float world_scale,float panel_distance,float zoom=1,float hud_size=1);
};
}
