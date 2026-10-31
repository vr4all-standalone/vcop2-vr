// SPDX-License-Identifier: GPL-2.0-or-later
#include "../native/settings_io.h"
#include <sstream>
#include <iostream>
#define CHECK(x) do{if(!(x)){std::cerr<<"Failed " #x " line "<<__LINE__<<'\n';return 1;}}while(0)
int main(){using namespace vcop2;
    CHECK(!Settings{}.game_zoom);
    for(int version=1;version<=6;version++){
        std::stringstream input("version="+std::to_string(version)+"\ngun=8\ngame_zoom=1\n");CHECK(!read_settings(input).game_zoom);
    }
    std::stringstream previous("version=4\ngun_angle_zero_version=1\np1_hand=1\nreload=2\ngun=1\nlaser=0\nview_fov=125\nwider_view=0\nworld_scale=0.65\npanel_distance=2.7\ngun_angle=-12\ngun_scale=1.25\nhaptics=0.35\n");
    auto s=read_settings(previous);CHECK(!s.game_zoom&&s.gun==1&&s.p1_hand==1&&s.reload==ReloadMode::PointOutside&&!s.laser);
    CHECK(s.world_scale==.65f&&s.view_fov==125&&!s.wider_view&&s.panel_distance==2.7f&&s.gun_angle==-12&&s.gun_scale==1.25f&&s.haptics==.35f);
    for(bool enabled:{false,true}){
        s.game_zoom=enabled;std::stringstream out;write_settings(out,s);CHECK(out.str().find("version=6\n")==0);
        auto restored=read_settings(out);CHECK(!restored.game_zoom&&restored.gun==1&&restored.p1_hand==1&&restored.world_scale==.65f&&restored.view_fov==125&&restored.gun_angle==-12&&restored.gun_scale==1.25f);
    }
    for(const char* invalid:{"2","-1","nan","1junk"}){std::stringstream in(std::string("version=5\ngame_zoom=")+invalid+"\n");CHECK(!read_settings(in).game_zoom);}
    std::cout<<"Every launch starts zoom Off while all other preferences persist\n";
}
