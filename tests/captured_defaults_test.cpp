// SPDX-License-Identifier: GPL-2.0-or-later
#include "../native/settings_io.h"
#include <iostream>
#include <sstream>
#include <stdexcept>
using namespace vcop2;
static void require(bool ok,const char* what){if(!ok)throw std::runtime_error(what);}
static void approved(const Settings& s){
 require(s.gun==0&&s.gun_angle==0&&s.reload==ReloadMode::ShootOutside&&s.p1_hand==0&&s.laser,"New/missing settings must use Arcade0/angle0/Shoot off-screen/P1left/LaserOn");
 require(s.reticle_mode==ReticleMode::Custom&&s.reticle_min_size==.20f&&s.reticle_max_size==.35f&&s.reticle_shrink_distance==7,"New/missing settings must use Custom20/35percent/7m");
 require(s.haptics==.65f&&s.world_scale==1&&s.panel_distance==1.8f&&s.hud_size==1&&s.gun_scale==1&&s.wider_view&&s.view_fov==110&&!s.game_zoom,"New/missing settings remaining captured profile mismatch");
}
int main()try{
 approved(Settings{});
 for(const char* text:{"", "version=6\ngun_angle_zero_version=1\n", "gun=9\nreload=3\nreticle_min_size=nan\nreticle_max_size=garbage\nreticle_shrink_distance=nan\n", "gun=bad\nreload=-1\nreticle_min_size=.01\nreticle_max_size=3.1\nreticle_shrink_distance=20.1\n"}){std::istringstream in(text);approved(read_settings(in));}
 for(int gun:{0,1,8})for(int reload:{0,1,2}){
  std::ostringstream text;text<<"version=6\ngun_angle_zero_version=1\ngun="<<gun<<"\nreload="<<reload<<"\np1_hand=1\nlaser=0\nreticle_mode=2\nreticle_min_size=.77\nreticle_max_size=2.25\nreticle_shrink_distance=13.4\nhaptics=.35\nworld_scale=.7\npanel_distance=3.2\nhud_size=1.5\ngun_angle=-12\ngun_scale=1.3\nwider_view=0\nview_fov=125\ngame_zoom=1\n";
  std::istringstream in(text.str());auto chosen=read_settings(in);std::stringstream saved;write_settings(saved,chosen);auto actual=read_settings(saved);
  require(actual.gun==gun&&int(actual.reload)==reload&&actual.p1_hand==1&&!actual.laser&&actual.gun_angle==-12,"Explicit gun/reload/hand/Laser/angle choices must survive save/load");
  require(actual.reticle_mode==ReticleMode::None&&actual.reticle_min_size==.77f&&actual.reticle_max_size==2.25f&&actual.reticle_shrink_distance==13.4f,"Explicit reticle tuning must survive save/load");
  require(actual.haptics==.35f&&actual.world_scale==.7f&&actual.panel_distance==3.2f&&actual.hud_size==1.5f&&actual.gun_scale==1.3f&&!actual.wider_view&&actual.view_fov==125&&!actual.game_zoom,"Other explicit choices persist while Game zoom remains session-only");
 }
 std::cout<<"Approved defaults, missing/invalid fallback and explicit alternate save/load passed\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
