// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
namespace vcop2 {
// No positively identified native player reticle exists in this adapter.
// Keep shared persisted values 1/2; never present fabricated native artwork.
enum class ReticleMode {Custom=1,None=2};
inline ReticleMode reticle_mode_value(int value){return value==2?ReticleMode::None:ReticleMode::Custom;}
inline ReticleMode next_reticle_mode(ReticleMode mode,int direction){return ReticleMode(1+(int(reticle_mode_value(int(mode)))-1+direction%2+2)%2);}
inline const char* reticle_mode_label(ReticleMode mode){return reticle_mode_value(int(mode))==ReticleMode::None?"None":"Custom";}
}
