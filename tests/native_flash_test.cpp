// SPDX-License-Identifier: GPL-2.0-or-later
#include "../native/native_flash.h"
#include <fstream>
#include <iostream>
#include <unordered_map>
#include <vector>
#define CHECK(x) do{if(!(x)){std::cerr<<"Failed " #x " line "<<__LINE__<<'\n';return 1;}}while(0)
int main(int argc,char** argv){
    if(argc!=2)return 2;
    std::ifstream in(argv[1],std::ios::binary);std::vector<uint8_t> rom{std::istreambuf_iterator<char>(in),{}};
    CHECK(rom.size()==0x200000);
    std::unordered_map<uint32_t,uint32_t> ram{{0x500030,8u<<16},{0x502088,1},{0x507200,4},{0x507248,0x100}};
    for(uint32_t a=0x100c000;a<0x100cc00;a+=4)ram[a]=0xffffffffu;
    auto read=[&](uint32_t a){if(a<rom.size())return uint32_t(rom[a])|uint32_t(rom[a+1])<<8|uint32_t(rom[a+2])<<16|uint32_t(rom[a+3])<<24;auto it=ram.find(a);return it!=ram.end()?it->second:0u;};
    const auto original=ram;
    CHECK(vcop2::native_gun_strobe(read));
    CHECK(ram==original);
    // The same native strobe precedes a reload or an empty shot, so ammo and
    // the accepted-shot counter must not be used as filters.
    ram[0x507260]=0;ram[0x50a2a4]=31;CHECK(vcop2::native_gun_strobe(read));
    ram[0x502088]=0;ram[0x50208c]=1;ram[0x507248]=0;ram[0x50724c]=0x200;CHECK(vcop2::native_gun_strobe(read));
    ram[0x5d051c]=1u<<8;CHECK(!vcop2::native_gun_strobe(read));ram[0x5d051c]=0;
    ram[0x500030]=5u<<16;CHECK(!vcop2::native_gun_strobe(read));ram[0x500030]=8u<<16;
    for(uint32_t count:{0u,1u,3u,5u,0xffffffffu}){ram[0x507200]=count;CHECK(!vcop2::native_gun_strobe(read));}ram[0x507200]=4;
    ram[0x50724c]=0;CHECK(!vcop2::native_gun_strobe(read));ram[0x50724c]=0x200;
    ram[0x100c7fc]=0xfffffff0u;CHECK(!vcop2::native_gun_strobe(read));ram[0x100c7fc]=0xffffffffu;
    ram[0x100cbfc]=0;CHECK(!vcop2::native_gun_strobe(read));ram[0x100cbfc]=0xffffffffu;
    rom[0x19f04]^=1;CHECK(!vcop2::native_gun_strobe(read));
    std::cout<<"Owned-ROM native shot/reload strobe, both players, mask, service and read-only guards passed\n";
}
