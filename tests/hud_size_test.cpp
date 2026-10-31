// SPDX-License-Identifier: GPL-2.0-or-later
#ifdef NDEBUG
#undef NDEBUG
#endif
#include "../native/settings_io.h"
#include <cassert>
#include <sstream>
#include <limits>
using namespace vcop2;
int main(){
    Settings s;assert(s.hud_size==1);
    for(const char* value:{"", "hud_size=nan\n", "hud_size=inf\n", "hud_size=0.49\n", "hud_size=2.01\n", "hud_size=bad\n"}){std::istringstream in(value);assert(read_settings(in).hud_size==1);}
    for(float size:{.5f,1.f,2.f}){s.hud_size=size;std::ostringstream out;write_settings(out,s);std::istringstream in(out.str());auto loaded=read_settings(in);assert(loaded.hud_size==size);assert(loaded.p1_hand==s.p1_hand);}
    assert(effective_hud_size(2,true,true)==2);assert(effective_hud_size(.5f,true,true)==.5f);
    assert(effective_hud_size(2,false,true)==1);assert(effective_hud_size(2,true,false)==1);
    assert(effective_hud_size(std::numeric_limits<float>::infinity(),true,true)==1);

    // HUD choice cannot influence logical-player/hand ownership or cabinet input.
    Settings normal,changed;changed.hud_size=2;InputPolicy a,b;Hands hands;GameState state;state.valid=true;state.ready=true;state.playing=true;state.players[0].state=state.players[1].state=1;state.players[0].health=state.players[1].health=1;
    for(auto& hand:hands){hand.tracked=true;hand.aim={.5,.5,true,true};}
    for(int frame=0;frame<24;frame++){
        normal.p1_hand=changed.p1_hand=(frame/4)%2;hands[0].trigger=frame%3==1;hands[1].trigger=frame%3==2;
        auto x=a.update(frame*.1,hands,state,normal,frame%7!=0,frame%11==0);
        auto y=b.update(frame*.1,hands,state,changed,frame%7!=0,frame%11==0);
        assert(x.hand==y.hand&&x.coin_mask==y.coin_mask&&x.start_mask==y.start_mask&&x.shots==y.shots);
        for(int n=0;n<2;n++)assert(x.guns[n].x==y.guns[n].x&&x.guns[n].y==y.guns[n].y&&x.guns[n].trigger==y.guns[n].trigger&&x.guns[n].pump==y.guns[n].pump);
        assert(changed.hud_size==2);
    }
    s=Settings{};assert(s.hud_size==1);
}
