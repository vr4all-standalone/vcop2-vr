// SPDX-License-Identifier: GPL-2.0-or-later
#include "../native/controller_controls.h"
#include <iostream>
#define CHECK(x) do{if(!(x)){std::cerr<<"Failed " #x " line "<<__LINE__<<'\n';return 1;}}while(0)
int main(){using namespace vcop2;
    Settings s;Hands hands;for(auto& h:hands)h.tracked=true;
    ExtraControls in;ShortcutPolicy shortcuts;double now=1;
    auto tick=[&](bool enabled=true){now+=.02;return shortcuts.update(now,in,hands,s,enabled);};
    tick();CHECK(!s.game_zoom);
    for(int hand:{0,1}){
        in.grip[hand]=true;tick();for(int i=0;i<20;i++)tick();CHECK(!s.game_zoom);
        in.grip[hand]=false;tick();CHECK(s.game_zoom);for(int i=0;i<10;i++)tick();CHECK(s.game_zoom);
        in.grip[hand]=true;tick();in.grip[hand]=false;tick();CHECK(!s.game_zoom);
    }
    // Size chords consume the grip gesture, including stick motion before it
    // reaches the adjustment threshold or returns to neutral before release.
    const float world=s.world_scale,gun=s.gun_scale;
    in.grip[0]=true;tick();in.stick[0]={0,1};tick();tick();in.stick[0]={};CHECK(tick().save);in.grip[0]=false;tick();CHECK(!s.game_zoom&&s.world_scale>world);
    in.grip[1]=true;in.stick[1]={1,0};tick();in.stick[1]={};tick();in.stick[1]={1,0};tick();in.grip[1]=false;tick();in.stick[1]={};CHECK(!s.game_zoom&&s.gun_scale>gun);
    in.grip[0]=true;tick();in.stick[0]={.4f,0};tick();in.stick[0]={};tick();in.grip[0]=false;tick();CHECK(!s.game_zoom);
    // No deferred taps from menu/service, focus, tracking or a held input at
    // session entry; a fresh release and tap are required.
    in.grip[0]=true;tick();tick(false);tick();in.grip[0]=false;tick();CHECK(!s.game_zoom);
    in.grip[1]=true;tick();hands[1].tracked=false;tick();hands[1].tracked=true;tick();in.grip[1]=false;tick();CHECK(!s.game_zoom);
    in.grip[0]=true;tick();shortcuts.suspend();tick();in.grip[0]=false;tick();CHECK(!s.game_zoom);
    in.grip={true,true};tick();in.grip={false,false};tick();CHECK(s.game_zoom);
    // A grip with a stick click is not a tap; the original recenter/laser
    // shortcuts still work once the grip has been released.
    in.grip[0]=true;tick();in.click[0]=true;tick();in.click[0]=false;tick();in.grip[0]=false;tick();CHECK(s.game_zoom);
    in.click[0]=true;CHECK(tick().recenter);in.click[0]=false;tick();in.click[1]=true;CHECK(tick().laser);
    CHECK(s.p1_hand==0&&s.gun==0&&s.reload==ReloadMode::ShootOutside);
    std::cout<<"Either-grip zoom taps, no repeats, size chords and focus/menu/tracking suppression passed\n";
}
