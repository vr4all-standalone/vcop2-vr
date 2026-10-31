// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <cstdint>
namespace vcop2 {
template<class Read32> bool native_gun_strobe(Read32 read) {
    // Original gun-reader0x19e5c emits one all-white window-selection mask,
    // then clears it during its four-tick cooldown. This is independent of
    // accepted ammo decrements (reload and empty fire also request a read).
    if(uint32_t(read(0x507200))!=4 || ((uint32_t(read(0x500030))>>16)&255)!=8 ||
       ((uint32_t(read(0x5d051c))>>8)&255)!=0)return false;
    if(uint32_t(read(0x502088))!=1 && uint32_t(read(0x50208c))!=1)return false;
    if(!uint32_t(read(0x507248))&&!uint32_t(read(0x50724c)))return false;
    constexpr uint32_t words[][2]={
        {0x19e74,0x90203000},{0x19e78,0x00507248},
        {0x19e90,0x90203000},{0x19e94,0x0050724c},
        {0x19edc,0x90183000},{0x19ee0,0x00507200},
        {0x19ee8,0x8c583000},{0x19eec,0x0100c000},{0x19ef0,0x59201901},
        {0x19f00,0x5948de06},{0x19f04,0xb222d000},{0x19f08,0x595ac810},
        {0x19f0c,0x5a4a4b01},{0x19f10,0x14fffff4},{0x19f14,0x5c781e04},
        {0x19f18,0x92783000},{0x19f1c,0x00507200},
        {0x19f30,0x8c583000},{0x19f34,0x0100c000},{0x19f38,0x5c201e00}
    };
    for(const auto& word:words)if(uint32_t(read(word[0]))!=word[1])return false;
    for(uint32_t a=0x0100c000;a<0x0100cc00;a+=4)
        if(uint32_t(read(a))!=0xffffffffu)return false;
    return true;
}
}
