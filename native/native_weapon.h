// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <vector>
namespace vcop2 {
struct NativeWeaponVertex {float x{},y{},z{},u{},v{};};
struct NativeWeaponPolygon {
    unsigned count{};uint32_t attr{};
    std::array<uint16_t,4> header{};
    std::array<float,3> normal{};
    std::array<NativeWeaponVertex,4> vertices{};
};
struct NativeWeapon {
    bool valid{};
    uint32_t weapon{},model{},object{};
    std::vector<NativeWeaponPolygon> polygons;
};
inline const char* native_weapon_name(unsigned weapon) {
    // Pickup names follow native HUD strings at 0x632dc..6330c, indexed by
    // 0x631a4. Slot 1 explicitly identifies our cosmetic scene-model use.
    constexpr const char* names[]={"Unavailable","Original scene pistol (cosmetic)","Original Rifle pickup","Original Automatic pickup","Original Machine Gun pickup","Original Shot Gun pickup","Original Magnum pickup"};
    return weapon<7?names[weapon]:names[0];
}
// Extract original gun meshes, in their authored local coordinates. Slots 2..6
// are the equipped pickup assets. Slot 1 is the original scene-pistol model
// 0x143, used cosmetically for standard play; it is not claimed to be a native
// player-held standard asset (the original arcade does not render one).
// read_program: physical i960 32-bit address; read_polygon: 32-bit ROM word
// index; read_texture: 16-bit ROM word index. Readers must bounds-check their
// supplied region. This owns decoded geometry/UV/header data and never writes
// memory. Textures/palette remain in the caller's native renderer resources.
template<class ReadProgram,class ReadPolygon,class ReadTexture>
NativeWeapon extract_native_weapon(ReadProgram read_program,ReadPolygon read_polygon,ReadTexture read_texture,unsigned weapon,
                                   uint32_t polygon_words,uint32_t texture_words) {
    if(weapon<1 || weapon>6 || !polygon_words || !texture_words)return {};
    if(read_program(0x388d8)!=0x8c583a10 || read_program(0x388dc)!=0x000391d4 ||
       read_program(0x391b8)!=0x903aa064 || read_program(0x391c0)!=0x09fdcee4 ||
       read_program(0x160e0)!=0x8c503a10 || read_program(0x160e4)!=0x00200000)return {};
    // Spawn stores table[type].model in pickup+0x64; draw loads that member.
    // The +0x68 model is a floating text label, never a gun mesh.
    if(weapon==1 && (read_program(0x31600)!=0x141 || read_program(0x31604)!=0x143 || read_program(0x31608)!=0x14c))return {};
    const uint32_t model=weapon==1?read_program(0x31604):read_program(0x391d4+weapon*16);
    constexpr unsigned expected[]={0,0x143,0x116,0x115,0x19a,0x19b,0x199};
    if(model!=expected[weapon])return {};
    const uint32_t entry=0x200000+model*16;
    uint32_t tp=read_program(entry),th=read_program(entry+4);
    const uint32_t object=read_program(entry+8),count=read_program(entry+12);
    if((object&0x01800000)!=0x00800000 || tp&0x800000 || th&0x800000)return {};
    uint32_t at=object&0x7fffff;
    if(at+6>polygon_words)return {};
    auto point=[&](uint32_t index){
        return std::array<float,3>{std::bit_cast<float>(uint32_t(read_polygon(index))),std::bit_cast<float>(uint32_t(read_polygon(index+1))),std::bit_cast<float>(uint32_t(read_polygon(index+2)))};
    };
    auto p0=point(at),p1=point(at+3);at+=6;
    NativeWeapon result;result.weapon=weapon;result.model=model;result.object=object;
    for(uint32_t link=0;link<4096 && link<(count?count:0xfffff);++link) {
        if(at>=polygon_words)return {};
        const uint32_t attr=read_polygon(at++);
        if(!(attr&3)){result.valid=!result.polygons.empty();return result;}
        if(at+9>polygon_words)return {};
        const auto normal=point(at);auto p2=point(at+3),p3=point(at+6);at+=9;
        const unsigned n=(attr&1)?4:3;if(n==3)p3=p2;
        if(tp+n*2>texture_words || th+4>texture_words)return {};
        NativeWeaponPolygon polygon;polygon.count=n;polygon.attr=attr;polygon.normal=normal;
        for(float value:normal)if(!std::isfinite(value))return {};
        const std::array<std::array<float,3>,4> points={p1,p0,p2,p3};
        for(unsigned i=0;i<4;++i)polygon.header[i]=uint16_t(read_texture(th+i));
        for(unsigned i=0;i<n;++i) {
            const auto& p=points[i];
            for(float value:p)if(!std::isfinite(value) || std::abs(value)>1000000)return {};
            polygon.vertices[i]={p[0],p[1],p[2],float(read_texture(tp+i*2+1)),float(read_texture(tp+i*2))};
        }
        tp+=n*2;int offset=int((attr>>12)&31);if(offset&16)offset-=32;
        const int64_t next=int64_t(th)+offset*4;if(next<0 || next>=texture_words)return {};th=uint32_t(next);
        const unsigned type=(attr>>8)&3;
        if(type)result.polygons.push_back(polygon); // link type 0 is native non-rendering setup
        if(type==0 || type==2){p0=p2;p1=p3;}else if(type==1)p1=p2;else p0=p3;
    }
    return {}; // Every supported original model must have a bounded terminator.
}
}
