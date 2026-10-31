// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <array>
#include <cstdint>
namespace vcop2 {
// MAME 0.289 model2.cpp: vcop2 ports, lightgun_data_r, lightgun_offscreen_r.
// These are native cabinet values. Calibration from a VR ray remains separate.
struct Cabinet {
    std::array<uint16_t,2> x{383,380},y{230,230};
    std::array<bool,2> trigger{};
    uint8_t coins{},starts{};
    bool test{},service{};
};
inline uint8_t in0(uint8_t native,const Cabinet& input) {
    uint8_t active=input.coins&3;
    active|=(input.starts&3)<<4;
    if(input.test)active|=4;
    if(input.service)active|=8;
    return native&~active;
}
inline uint8_t in1(uint8_t native,const Cabinet& input) {
    return native&~((input.trigger[0]?1:0)|(input.trigger[1]?2:0));
}
inline bool offscreen(unsigned player,uint16_t x,uint16_t y) {
    const int min_x=player==0?137:134,max_x=player==0?630:627;
    constexpr int min_y=36,max_y=425;
    // Native driver truncates 5 percent to integers: X 24, Y 19.
    constexpr int border_x=24,border_y=19;
    return x<=min_x+border_x||x>=max_x-border_x||y<=min_y+border_y||y>=max_y-border_y;
}
inline uint8_t serial_byte(unsigned mux,const Cabinet& input) {
    if(mux>=8)return uint8_t(0xfc|(offscreen(0,input.x[0],input.y[0])?1:0)|(offscreen(1,input.x[1],input.y[1])?2:0));
    const uint16_t values[]={input.y[0],input.x[0],input.y[1],input.x[1]};
    return uint8_t(values[mux/2]>>((mux&1)*8));
}
}
