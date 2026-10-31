// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "input_policy.h"
#include "button_latch.h"
namespace vcop2 {
class ControllerHelpPolicy {
    bool intro_{true};
public:
    bool show(bool manual=false,bool menu=false,bool service=false) const{return (manual&&menu)||(intro_&&!service&&!menu);}
    void observe(const CabinetInput& input){if(input.coin_mask||input.start_mask||input.guns[0].trigger||input.guns[1].trigger)intro_=false;}
};
struct Stick {float x{},y{};};
struct ExtraControls {std::array<Stick,2> stick;std::array<bool,2> grip{},click{};};
struct ShortcutResult {bool recenter{},laser{},save{};};
class GripZoomPolicy {
    std::array<bool,2> waiting_{true,true},down_{},used_{};
public:
    void suspend(){waiting_={true,true};down_=used_={};}
    bool update(const ExtraControls& in,const Hands& hands,bool enabled) {
        bool toggle=false;
        for(int h=0;h<2;h++) {
            if(!enabled||!hands[h].tracked){waiting_[h]=true;down_[h]=used_[h]=false;continue;}
            if(!in.grip[h]) {
                toggle|=down_[h]&&!used_[h];
                waiting_[h]=down_[h]=used_[h]=false;continue;
            }
            if(waiting_[h])continue;
            down_[h]=true;
            if(std::hypot(in.stick[h].x,in.stick[h].y)>=.3f||in.click[h])used_[h]=true;
        }
        return toggle;
    }
};
class MenuHoldPolicy {
    ButtonLatch button_;bool armed_{},opened_{};double began_{};
public:
    struct Result {bool toggle{},start{};};
    void suspend(){button_.suspend();armed_=opened_=false;}
    Result update(double now,bool down,bool focused) {
        if(!focused)suspend();
        Result out;
        if(button_.edge(down,focused)){armed_=true;opened_=false;began_=now;}
        if(armed_&&down&&!opened_&&now-began_>=.8){out.toggle=true;opened_=true;}
        if(!down&&armed_){out.start=!opened_;armed_=false;}
        return out;
    }
};
class ServiceGunPolicy {
    std::array<bool,2> blocked_{true,true},pump_blocked_{true,true};bool enabled_{};int layout_{-1};
public:
    void suspend(){enabled_=false;blocked_={true,true};pump_blocked_={true,true};}
    CabinetInput update(const Hands& hands,const Settings& settings,bool enabled) {
        if(!enabled||enabled!=enabled_||settings.p1_hand!=layout_){blocked_={true,true};pump_blocked_={true,true};}
        enabled_=enabled;layout_=settings.p1_hand;
        CabinetInput out;out.hand={layout_,1-layout_};
        for(int p=0;p<2;p++) {
            int h=out.hand[p];const auto& in=hands[h];auto& gun=out.guns[p];
            if(!in.trigger)blocked_[h]=false;
            if(!in.pump)pump_blocked_[h]=false;
            if(!in.tracked){blocked_[h]=true;pump_blocked_[h]=true;}
            if(!enabled||!in.tracked)continue;
            if(in.aim.valid&&std::isfinite(in.aim.u)&&std::isfinite(in.aim.v)) {
                gun.x=uint16_t(std::lround(std::clamp(in.aim.u,0.,1.)*65535));gun.y=uint16_t(std::lround(std::clamp(in.aim.v,0.,1.)*65535));
                gun.trigger=in.trigger&&!blocked_[h]&&in.aim.inside;
            }
            gun.pump=in.pump&&!pump_blocked_[h];
        }
        return out;
    }
};
class ShortcutPolicy {
    GripZoomPolicy zoom_;
    std::array<ButtonLatch,2> click_;
    std::array<bool,2> blocked_{true,true},armed_{};
    double last_{};bool dirty_{};
public:
    void suspend(){for(auto& b:click_)b.suspend();blocked_={true,true};armed_={};last_=0;zoom_.suspend();}
    ShortcutResult update(double now,const ExtraControls& in,const Hands& hands,Settings& settings,bool enabled) {
        ShortcutResult out;double dt=last_?std::clamp(now-last_,0.,.1):0;last_=now;
        if(zoom_.update(in,hands,enabled))settings.game_zoom=!settings.game_zoom;
        bool adjusting=false;
        for(int h=0;h<2;h++) {
            bool active=enabled&&hands[h].tracked;
            if(!active){blocked_[h]=true;armed_[h]=false;}
            if(!in.grip[h]){blocked_[h]=false;armed_[h]=false;}
            bool edge=click_[h].edge(in.click[h],active&&!in.grip[h]);
            if(h==0)out.recenter=edge;else out.laser=edge;
            const auto stick=in.stick[h];
            if(active&&in.grip[h]&&!blocked_[h]&&std::hypot(stick.x,stick.y)<.3)armed_[h]=true;
            if(!active||!in.grip[h]||!armed_[h])continue;
            bool vertical=std::abs(stick.y)>=std::abs(stick.x);
            float amount=vertical?stick.y:stick.x;
            if(std::abs(amount)<=.5)continue;
            auto& value=vertical?settings.world_scale:settings.gun_scale;
            value=std::clamp(float(value*std::exp((amount>0?1:-1)*(vertical?.6:.5)*dt)),vertical?.01f:.4f,vertical?2.f:1.6f);
            adjusting=dirty_=true;break;
        }
        if(dirty_&&!adjusting){out.save=true;dirty_=false;}
        return out;
    }
};
class OperatorPolicy {
    std::array<ButtonLatch,2> trigger_,start_,pump_;
    bool axes_wait_{true};int previous_{};double repeat_{};
public:
    void suspend(){for(auto* buttons:{&trigger_,&start_,&pump_})for(auto& b:*buttons)b.suspend();axes_wait_=true;previous_=0;}
    uint16_t update(double now,const ExtraControls& axes,const Hands& hands,bool enabled,bool guns=false,int p1_hand=0) {
        if(!enabled)suspend();
        uint16_t result=0;
        for(int h=0;h<2;h++) {
            bool active=enabled&&hands[h].tracked;
            bool trigger=trigger_[h].edge(hands[h].trigger,active),start=start_[h].edge(hands[h].start,active),pump=pump_[h].edge(hands[h].pump,active);
            if((trigger&&!guns)||(start&&!guns))result|=4;
            if(start&&guns)result|=h==p1_hand?0x10:0x20; // Calibration has real P1/P2 Start functions.
            if(pump&&!guns)result|=8;
        }
        float y=0;
        for(int h=0;h<2;h++)if(hands[h].tracked&&std::abs(axes.stick[h].y)>std::abs(y))y=axes.stick[h].y;
        if(enabled&&std::abs(y)<.25f)axes_wait_=false;
        int direction=enabled&&!axes_wait_&&std::abs(y)>.65f?(y>0?1:-1):0;
        if(direction&&(direction!=previous_||now>=repeat_)) {
            // The Model 2 Service switch advances one row in either direction.
            result|=8;repeat_=now+(direction!=previous_?.5:.3);
        }
        previous_=direction;return result;
    }
};
}
