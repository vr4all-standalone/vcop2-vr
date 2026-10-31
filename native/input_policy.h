// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "game_state.h"
#include "reticle_mode.h"
#include <algorithm>
#include <array>
#include <cmath>
namespace vcop2 {
struct Aim { double u{.5},v{.5};bool valid{},inside{}; };
struct HandInput { bool tracked{},trigger{},start{},pump{}; Aim aim; };
using Hands=std::array<HandInput,2>; // Left, right
enum class ReloadMode { Button, ShootOutside, PointOutside };
enum class BackgroundMode {Original,Depth3D};
inline constexpr std::array<float,4> HapticLevels{0,.35f,.65f,1};
inline int haptic_level(float value){int best=0;for(int i=1;i<4;i++)if(std::abs(value-HapticLevels[i])<std::abs(value-HapticLevels[best]))best=i;return best;}
inline float effective_hud_size(float saved,bool large_view,bool gameplay){return large_view&&gameplay&&std::isfinite(saved)&&saved>=.5f&&saved<=2.f?saved:1.f;}
struct Settings {
    float hud_size{1.f};
    ReticleMode reticle_mode{ReticleMode::Custom};
    float reticle_min_size{.20f},reticle_max_size{.35f},reticle_shrink_distance{7.f};
    int p1_hand{0}; ReloadMode reload{ReloadMode::ShootOutside};
    float world_scale{1.f},panel_distance{1.8f},gun_angle{},gun_scale{1},haptics{.65f};
    bool laser{true}; int gun{0}; // Arcade; explicit saved In-game/Zapper choices remain available.
    BackgroundMode background{BackgroundMode::Original};
    bool wider_view{true};
    bool game_zoom{false};
    float view_fov{110};
};
struct CabinetGun { uint16_t x{32768},y{32768};bool trigger{},pump{}; };
struct CabinetInput {
    std::array<CabinetGun,2> guns;
    std::array<int,2> hand{1,-1};
    std::array<uint32_t,2> shots{};
    uint8_t coin_mask{},start_mask{};
};
// Runs at native vblank, so a held-trigger handoff gets a complete release
// frame even when the independently paced XR thread updates faster than MAME.
class CabinetLatch {
    CabinetInput previous_;
public:
    CabinetInput update(const CabinetInput& desired) {
        auto out=desired;
        for(int p=0;p<2;p++)if(previous_.hand[p]!=desired.hand[p]&&previous_.guns[p].trigger&&desired.guns[p].trigger)out.guns[p].trigger=false;
        for(int p=0;p<2;p++)if(desired.guns[p].pump&&previous_.guns[p].trigger){out.guns[p].pump=false;out.guns[p].trigger=false;}
        previous_=out;return out;
    }
};
class OperatorLatch {
    std::array<unsigned,4> ticks_{};
public:
    uint16_t update(uint16_t pending) {
        uint16_t result=0;
        constexpr uint16_t buttons[]={4,8,0x10,0x20};
        for(int i=0;i<4;i++) {
            if(pending&buttons[i])ticks_[i]=6; // Measured operator debounce needs a sustained press.
            if(ticks_[i]){result|=buttons[i];--ticks_[i];}
        }
        return result;
    }
};
class InputPolicy {
    Hands previous_{};
    bool initialized_{},was_enabled_{},native_start_ready_{};
    int previous_layout_{},previous_join_{},solo_hand_{1};
    std::array<bool,2> blocked_{true,true},pump_blocked_{true,true},pull_outside_{},outside_{};
    std::array<bool,2> start_waiting_{true,true};
    std::array<int,2> start_owner_{-1,-1};
    struct StartPulse { double time{-100.};int player{-1}; };
    std::array<StartPulse,2> starts_;
    GameState previous_state_;
    std::array<double,2> reload_until_{};
public:
    void suspend(){initialized_=false;was_enabled_=false;blocked_={true,true};pump_blocked_={true,true};start_waiting_={true,true};start_owner_={-1,-1};starts_={};}
    CabinetInput update(double now,const Hands& hands,const GameState& state,
                        const Settings& settings,bool focused,bool menu) {
        CabinetInput out;
        int joined=state.valid?((state.players[0].joined()?1:0)|(state.players[1].joined()?2:0)):0;
        native_start_ready_=state.valid&&state.ready;
        const bool dual=joined==3,enabled=focused&&!menu&&native_start_ready_;
        const int p1hand=settings.p1_hand==1?1:0;
        bool transition=!initialized_ || !enabled || enabled!=was_enabled_ || joined!=previous_join_ || p1hand!=previous_layout_;
        if(transition) { blocked_={true,true};pump_blocked_={true,true};pull_outside_={};outside_={};reload_until_={}; }
        for(int h=0;h<2;h++) {
            const auto& in=hands[h];
            if(!in.trigger) { blocked_[h]=false;pull_outside_[h]=false; }
            if(!in.pump)pump_blocked_[h]=false;
            if(!in.tracked){blocked_[h]=true;pump_blocked_[h]=true;}
            // Sample physical Start even while the game cannot receive it.
            // Loading/menu/focus/tracking holds must be released before reuse.
            const bool start_ready=enabled&&native_start_ready_&&in.tracked;
            if(!start_ready){start_waiting_[h]=true;start_owner_[h]=-1;starts_[h]={};}
            else if(!in.start)start_waiting_[h]=false;
            if(!in.start) start_owner_[h]=-1;
            if(start_ready&&!start_waiting_[h]&&in.start&&!previous_[h].start) {
                const int owner=dual?(h==p1hand?0:1):(h==1?0:1);
                // Solo P2 Start is a fresh native join request, never a queued
                // boot press. The native acknowledgement alone changes layout.
                if(owner==0||dual||((joined&1)&&hands[0].tracked&&hands[1].tracked)) {
                    start_owner_[h]=owner;
                    // A fast re-press must not insert credit again or move the
                    // pending native Start past its original delivery window.
                    const auto& pending=starts_[h];
                    if(pending.player!=owner||now<pending.time||now-pending.time>=.26)
                        starts_[h]={now,owner};
                }
            }
            if(in.trigger&&!previous_[h].trigger) pull_outside_[h]=!in.aim.inside;
        }
        // The solo gun stays where P1 last held it after P2 leaves. Transfer
        // only after its current pull releases, or to the sole tracked hand.
        if(dual)solo_hand_=p1hand;
        else if(enabled) {
            const int other=1-solo_hand_;
            const bool was_pulled=previous_[solo_hand_].tracked&&previous_[solo_hand_].trigger;
            const bool pulled=hands[solo_hand_].tracked&&hands[solo_hand_].trigger;
            if(!was_pulled&&hands[other].tracked&&(!hands[solo_hand_].tracked||
                (hands[other].trigger&&!pulled&&!blocked_[other])))solo_hand_=other;
        }
        if(dual) out.hand={p1hand,1-p1hand};
        else out.hand=joined==2?std::array<int,2>{-1,solo_hand_}:std::array<int,2>{solo_hand_,-1};
        for(int p=0;p<2;p++) {
            const int h=out.hand[p];
            if(h<0)continue;
            const auto& in=hands[h];auto& gun=out.guns[p];
            const bool aim=in.tracked&&in.aim.valid&&std::isfinite(in.aim.u)&&std::isfinite(in.aim.v);
            if(aim) { gun.x=uint16_t(std::lround(std::clamp(in.aim.u,0.,1.)*65535));gun.y=uint16_t(std::lround(std::clamp(in.aim.v,0.,1.)*65535)); }
            const bool edge=in.trigger&&!previous_[h].trigger;
            const bool off=aim&&!in.aim.inside;
            if(enabled) {
                for(int pump_hand=0;pump_hand<2;pump_hand++)if((!dual||pump_hand==h)&&hands[pump_hand].tracked&&hands[pump_hand].pump&&!pump_blocked_[pump_hand])gun.pump=true;
            }
            if(enabled && aim && !blocked_[h]) {
                gun.trigger=in.trigger&&in.aim.inside&&!pull_outside_[h];
                gun.pump=gun.pump || (settings.reload==ReloadMode::ShootOutside && off&&edge) ||
                    (settings.reload==ReloadMode::PointOutside && off&&!outside_[h]);
            }
            if(gun.pump)reload_until_[p]=now+.12;
            gun.pump=enabled&&now<reload_until_[p];
            if(gun.pump)gun.trigger=false;
            outside_[h]=off;
        }
        if(enabled) for(int h=0;h<2;h++) {
            const auto& pulse=starts_[h];if(pulse.player<0)continue;
            double elapsed=now-pulse.time;
            if(elapsed>=0 && elapsed<.1)out.coin_mask|=1<<pulse.player;
            // Retain a short tap through credit insertion, then hold the native
            // Start for the whole physical press with its original player.
            if(elapsed>=.14 && (elapsed<.26||(hands[h].start&&start_owner_[h]==pulse.player)))out.start_mask|=1<<pulse.player;
        }
        if(enabled && !transition && state.valid && previous_state_.valid && state.counting_shots && previous_state_.counting_shots) {
            for(int p=0;p<2;p++) {
                auto count=state.players[p].shots,before=previous_state_.players[p].shots;
                if(count>=before && count-before<=64 && state.players[p].alive()) out.shots[p]=count-before;
            }
        }
        previous_=hands;previous_state_=state;previous_layout_=p1hand;previous_join_=joined;
        was_enabled_=enabled;initialized_=true;
        return out;
    }
};
}
