// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "game_memory.h"
#include <array>
#include <cstdint>
namespace vcop2 {
struct PlayerState {
    uint32_t state{},ammo{},capacity{},shots{},weapon{};
    float health{};
    uint32_t native_state{};
    bool joined() const { return state==1 || state==2; }
    bool alive() const { return state==1 && health>0; }
};
struct GameState {
    bool valid{},playing{},counting_shots{};
    uint32_t mode{};
    std::array<PlayerState,2> players{};
    bool ready{},service{},gun_calibration{};
    uint32_t submode{},service_menu{},service_submode{},service_cursor{},service_busy{};
    std::array<GunCalibration,2> calibration{};
    // Observations only; never used to gate gameplay or cabinet controls.
    uint32_t native_game_loops{},native_backlog_events{},native_vblank_pending{},native_video_control{};
};
// Physical i960 program addresses for the verified vcop2 ROM revision.
// Fingerprints cover the mode dispatcher, per-player ammo pointer setup,
// actual accepted on-screen ammo decrement, cumulative shot statistic, and
// native service loop. A mismatched revision fails closed.
template<class Read32> GameState read_game_state(Read32 read) {
    if(read(0x2720)!=0x80183000 || read(0x2724)!=0x00500032 ||
       read(0x19954)!=0x8c883000 || read(0x19958)!=0x00507260 ||
       read(0x19988)!=0x8c883000 || read(0x1998c)!=0x00507264 ||
       read(0x19b6c)!=0x59210981 || read(0x19b70)!=0x92245000 ||
       read(0x37c30)!=0x90703000 || read(0x37c34)!=0x0050a2a4 ||
       read(0x37c7c)!=0x90703000 || read(0x37c80)!=0x0050a2a8 ||
       read(0x530b4)!=0x82a83000 || read(0x530b8)!=0x005d051d ||
       read(0x5313c)!=0x80a03000 || read(0x53140)!=0x005d051d) return {};
    const auto byte=[&](uint32_t address){return (uint32_t(read(address&~3u))>>((address&3)*8))&255u;};
    GameState result;
    result.mode=byte(0x500032);result.submode=byte(0x500038);
    const uint32_t service=byte(0x5d051d);
    if(result.mode>18 || result.submode>25 || service>1)return {};
    result.service=service==1;
    result.service_menu=byte(0x5d0518);result.service_submode=byte(0x5d0500);
    if(result.service_menu>12)return {};
    result.service_cursor=byte(0x5d051c);result.service_busy=read(0x5d0654);
    // Gun Setting itself is a Service/Test navigation list. Aim controls
    // begin only after a gun action is selected (native phase becomes nonzero).
    result.gun_calibration=result.service && result.service_menu==9 &&
        result.service_cursor>=1 && result.service_cursor<=7 && result.service_busy>0;
    result.playing=!result.service && result.mode==8;
    // Mode 3 already calls the cabinet Start handler (0x2c18 -> 0x2fa4).
    // Modes 0..2 execute startup checks; modes 17..18 return to startup/test.
    result.ready=!result.service && result.mode>=3 && result.mode<=16;
    result.counting_shots=result.playing && result.submode==8;
    for(unsigned i=0;i<2;++i) {
        auto& p=result.players[i];
        p.native_state=read(0x502088+4*i);
        p.health=float(read(0x502090+4*i));
        p.ammo=read(0x507260+4*i);p.capacity=read(0x507268+4*i);
        p.weapon=read(0x507270+4*i);p.shots=read(0x50a2a4+4*i);
        if(p.native_state>3 || p.health>255 || p.capacity>256 || p.ammo>p.capacity || p.weapon>6)return {};
        // Attract deliberately sets demo P1 active. Never acknowledge a join
        // from that value until the original game has entered mode 8.
        p.state=result.playing?p.native_state:0;
        result.calibration[i]=read_gun_calibration(read,i);
    }
    // Original loop 0x2034..0x2040 increments 0x500028; sync helper
    // 0x3714..0x3720 increments 0x500020 when >=2 VBLANKs accumulated.
    // IRQ 0x0fe0..0x0fec increments the pending byte at 0x500000.
    result.native_game_loops=read(0x500028);result.native_backlog_events=read(0x500020);
    result.native_vblank_pending=byte(0x500000);
    // native_video_control is supplied separately by the hardware driver.
    result.valid=true;return result;
}
}
