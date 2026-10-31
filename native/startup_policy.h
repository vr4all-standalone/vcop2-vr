// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "model2_capture.h"
namespace vcop2 {
// A native picture can exist while boot/credits still reject cabinet Start.
// Keep visual readiness independent from the game's input-ready mode.
inline bool native_picture_available(const NativeFrame* frame) {
    if(!frame||!frame->complete||frame->width<1||frame->height<1||frame->width>2048||frame->height>2048)return false;
    const size_t pixels=size_t(frame->width)*frame->height;
    return frame->native.size()==pixels||frame->background.size()==pixels;
}
}
