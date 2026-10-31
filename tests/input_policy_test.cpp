#include "../native/controller_controls.h"
#include "../native/shared_runtime.h"
#include "../native/settings_io.h"
#include "../native/realtime_pacer.h"
#include <iostream>
#include <sstream>
#define CHECK(x) do {if(!(x)){std::cerr<<"Failed " #x " line "<<__LINE__<<'\n';return 1;}}while(0)
int main(){using namespace vcop2;
 CHECK(Settings{}.world_scale==1.f&&Settings{}.view_fov==110.f&&Settings{}.wider_view);
 CHECK(Settings{}.gun==0);
 std::stringstream prior_default("version=2\ngun=0\nworld_scale=0.5\nview_fov=100\n");
 auto updated_default=read_settings(prior_default);CHECK(updated_default.gun==8&&updated_default.world_scale==.5f&&updated_default.view_fov==100);
 for(int choice=1;choice<=8;choice++){std::stringstream choice_file("version=2\ngun="+std::to_string(choice)+"\n");CHECK(read_settings(choice_file).gun==(choice==1?1:8));}
 std::stringstream chosen_arcade("version=3\ngun=0\n");CHECK(read_settings(chosen_arcade).gun==0);
 for(int version=1;version<=4;version++)for(int choice=0;choice<=8;choice++) {
   std::stringstream previous("version="+std::to_string(version)+"\ngun="+std::to_string(choice)+"\n");
   CHECK(read_settings(previous).gun==(version>=4?(choice==0||choice==1||choice==8?choice:0):(choice==1?1:choice==0&&version>=3?0:8)));
 }
 for(int choice=0;choice<=8;choice++){std::stringstream no_version("gun="+std::to_string(choice)+"\n");CHECK(read_settings(no_version).gun==(choice==1?1:8));}
 std::stringstream immersive_v4("version=4\ngun_angle_zero_version=1\np1_hand=1\nreload=2\ngun=1\nlaser=0\nview_fov=125\nwider_view=0\nworld_scale=0.65\npanel_distance=2.7\ngun_angle=-12\ngun_scale=1.25\nhaptics=0.35\nbackground=1\n");
 auto preserved=read_settings(immersive_v4);
 CHECK(preserved.gun==1&&preserved.p1_hand==1&&preserved.reload==ReloadMode::PointOutside&&!preserved.laser);
 CHECK(preserved.view_fov==125&&!preserved.wider_view&&preserved.world_scale==.65f&&preserved.panel_distance==2.7f);
 CHECK(preserved.gun_angle==-12&&preserved.gun_scale==1.25f&&preserved.haptics==.35f&&preserved.background==BackgroundMode::Depth3D);
 std::stringstream roundtrip;write_settings(roundtrip,preserved);auto restored=read_settings(roundtrip);
 CHECK(restored.gun==preserved.gun&&restored.p1_hand==preserved.p1_hand&&restored.reload==preserved.reload&&restored.laser==preserved.laser);
 CHECK(restored.view_fov==preserved.view_fov&&restored.wider_view==preserved.wider_view&&restored.world_scale==preserved.world_scale&&restored.panel_distance==preserved.panel_distance);
 CHECK(restored.gun_angle==preserved.gun_angle&&restored.gun_scale==preserved.gun_scale&&restored.haptics==preserved.haptics&&restored.background==preserved.background);
 for(int choice=2;choice<=7;choice++){Settings invalid;invalid.gun=choice;std::stringstream sanitized;write_settings(sanitized,invalid);CHECK(read_settings(sanitized).gun==8&&sanitized.str().find("\ngun=8\n")!=std::string::npos);}
 std::stringstream legacy("version=1\nworld_scale=0.1\nview_fov=65\nwider_view=0\np1_hand=1\nreload=2\n");
 auto migrated=read_settings(legacy);CHECK(migrated.world_scale==1.f&&migrated.view_fov==110.f&&migrated.wider_view);CHECK(migrated.p1_hand==1&&migrated.reload==ReloadMode::PointOutside);
 Settings settings;GameState state;state.valid=true;state.ready=true;state.mode=5;
 Hands hands;for(auto& h:hands){h.tracked=true;h.aim={1,0,true,true};}
 InputPolicy policy;policy.update(0,hands,state,settings,true,false);
 hands[1].start=true;auto input=policy.update(.1,hands,state,settings,true,false);CHECK(input.coin_mask==1);CHECK(!input.start_mask);
 input=policy.update(.25,hands,state,settings,true,false);CHECK(input.start_mask==1);
 hands[1].start=false;hands[1].trigger=true;input=policy.update(.4,hands,state,settings,true,false);CHECK(input.guns[0].x==65535);CHECK(input.guns[0].y==0);CHECK(input.guns[0].trigger);
 hands[1].trigger=false;policy.update(.5,hands,state,settings,true,false);
 hands[1].aim={1.1,.5,true,false};hands[1].trigger=true;input=policy.update(.6,hands,state,settings,true,false);CHECK(input.guns[0].pump&&!input.guns[0].trigger);
 hands[1].trigger=false;input=policy.update(.65,hands,state,settings,true,false);CHECK(input.guns[0].pump);
 input=policy.update(.8,hands,state,settings,true,false);CHECK(!input.guns[0].pump);
 state.ready=false;hands[1].start=true;input=policy.update(.9,hands,state,settings,true,false);CHECK(!input.coin_mask&&!input.start_mask);
 state.ready=true;input=policy.update(1,hands,state,settings,true,false);CHECK(!input.coin_mask&&!input.start_mask);
 CabinetLatch latch;CabinetInput firing;firing.guns[0].trigger=true;latch.update(firing);firing.guns[0].trigger=false;firing.guns[0].pump=true;CHECK(!latch.update(firing).guns[0].pump);CHECK(latch.update(firing).guns[0].pump);
 OperatorLatch operators;CHECK(operators.update(8)==8);CHECK(operators.update(0)&8);CHECK(!(operators.update(0x800)&0x800));
 settings.wider_view=true;std::stringstream saved;write_settings(saved,settings);CHECK(read_settings(saved).wider_view);
 CHECK(saved.str().find("version=6\n")==0&&read_settings(saved).gun==0);
 NativeSpeedTracker speed;CHECK(!speed.update(0,100,false).ready);CHECK(speed.update(5,105,false).percent==100);
 CHECK(!speed.update(5.2,106,true).ready);CHECK(!speed.update(5.2,110,false).ready);
 auto resumed=speed.update(10.2,115,false);CHECK(resumed.ready&&std::abs(resumed.percent-100)<.001);
 CHECK(!speed.update(0,116,false).ready);CHECK(speed.update(5,121,false).percent==100);
 std::cout<<"Model 2 native readiness, join pulses, full-range aim and reload delivery passed\n";
}
