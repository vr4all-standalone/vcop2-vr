// SPDX-License-Identifier: GPL-2.0-or-later
#include "../native/presentation.h"
#include "../native/gl_context.h"
#include "../native/reticle_size.h"
#include "../native/settings_io.h"
#include <glad/gl.h>
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>
#include <stdexcept>
#include <vector>
#include <filesystem>
#include <chrono>
using namespace vcop2;
static void require(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
struct Marker {glm::mat4 model;glm::vec2 size;glm::vec3 color;};
static std::vector<Marker> markers;static unsigned dots{},beams{};static glm::mat4 inverse_vp{1};
static PFNGLDRAWARRAYSPROC previous;
static void GLAD_API_PTR observe(GLenum mode,GLint first,GLsizei count){
 GLint program{};glGetIntegerv(GL_CURRENT_PROGRAM,&program);auto color=glGetUniformLocation(program,"player_color"),model=glGetUniformLocation(program,"mvp"),size=glGetUniformLocation(program,"half_size");
 if(color>=0&&model>=0&&size>=0){require(count==72,"Custom shape must use the real Gunblade hollow X mesh");Marker m;glGetUniformfv(program,model,&m.model[0][0]);m.model=inverse_vp*m.model;glGetUniformfv(program,size,&m.size[0]);glGetUniformfv(program,color,&m.color[0]);markers.push_back(m);}
 else if(mode==GL_TRIANGLE_STRIP&&count==4&&glGetUniformLocation(program,"tint")>=0){float v[5]{};glGetBufferSubData(GL_ARRAY_BUFFER,0,sizeof(v),v);if(std::abs(std::abs(v[0])-.006f)<1e-6f&&std::abs(std::abs(v[1])-.006f)<1e-6f)dots++;if(std::abs(std::abs(v[0])-.0008f)<1e-6f)beams++;}
 previous(mode,first,count);
}
static bool close(glm::vec3 a,glm::vec3 b){return glm::length(a-b)<1e-4f;}
int main(int argc,char** argv)try {
 if(argc>1)_putenv_s("VCOP2_ASSETS",argv[1]);
 auto file=std::filesystem::temp_directory_path()/("vcop2-reticle-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())+".ini");
 struct Cleanup {std::filesystem::path path;~Cleanup(){std::error_code ec;std::filesystem::remove(path,ec);}} cleanup{file};_putenv_s("VCOP2_SETTINGS",file.string().c_str());
 GLContext context;
 GLuint fbo{},image{},depth{};glGenFramebuffers(1,&fbo);glBindFramebuffer(GL_FRAMEBUFFER,fbo);glGenTextures(1,&image);glBindTexture(GL_TEXTURE_2D,image);glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,512,512,0,GL_RGBA,GL_UNSIGNED_BYTE,nullptr);glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,image,0);glGenRenderbuffers(1,&depth);glBindRenderbuffer(GL_RENDERBUFFER,depth);glRenderbufferStorage(GL_RENDERBUFFER,GL_DEPTH_COMPONENT24,512,512);glFramebufferRenderbuffer(GL_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,GL_RENDERBUFFER,depth);require(glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE,"Real GPU reticle framebuffer incomplete");glViewport(0,0,512,512);glClearColor(0,0,0,1);
 previous=glad_glDrawArrays;glad_glDrawArrays=observe;
 Settings settings;settings.gun=0;settings.laser=false;SharedRuntime runtime;
 runtime.state.valid=runtime.state.playing=true;runtime.state.ready=true;
 for(auto& player:runtime.state.players){player.state=1;player.health=1;}
 Presentation ui;PresentationInput input;input.reticle_focal={400,450};
 input.head=glm::translate(glm::mat4(1),glm::vec3(.2,.3,.1))*glm::rotate(glm::mat4(1),.23f,glm::vec3(0,1,0))*glm::rotate(glm::mat4(1),.07f,glm::vec3(0,0,1));
 for(int hand=0;hand<2;hand++){input.hands[hand].tracked=true;input.poses[hand]=glm::translate(glm::mat4(1),glm::vec3(hand?.2f:-.2f,0,-.4f));input.aim_targets[hand]=Vec3{hand?.4:-.4,.1,-4};}
 ui.update(input,settings,runtime,1);CabinetInput cabinet;cabinet.hand={0,1};cabinet.start_mask=1;ui.feedback(cabinet,1);
 auto base=glm::perspective(glm::radians(90.f),1.f,.02f,100.f);
 auto draw=[&](glm::mat4 vp){inverse_vp=glm::inverse(vp);markers.clear();dots=beams=0;glDepthMask(GL_TRUE);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);ui.draw(vp,settings,runtime.state,true,2);return markers;};
 auto first=draw(base);require(first.size()==2,"Default Custom must draw both actual GPU reticles independently of Laser");require(!beams&&!dots,"Laser Off emitted an auxiliary beam/dot");
 std::vector<unsigned char> pixels(512*512*4);glReadPixels(0,0,512,512,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data());unsigned red{},blue{};
 for(size_t i=0;i<pixels.size();i+=4){if(pixels[i]>250&&std::abs(int(pixels[i+1])-32)<3&&std::abs(int(pixels[i+2])-32)<3)red++;if(std::abs(int(pixels[i])-32)<3&&std::abs(int(pixels[i+1])-102)<3&&pixels[i+2]>250)blue++;}
 require(red>12&&blue>12,"GPU custom draws produced no actual visible red/blue hollow X pixels");
 for(int x:{230,281}){const size_t center=(262*512+x)*4;require(pixels[center]<10&&pixels[center+1]<10&&pixels[center+2]<10,"Custom X must retain an empty aim center");}
 std::ofstream preview("reticle-gpu-preview.ppm",std::ios::binary);preview<<"P6\n512 512\n255\n";for(int y=511;y>=0;y--)for(int x=0;x<512;x++){size_t i=(y*512+x)*4;preview.write(reinterpret_cast<const char*>(pixels.data()+i),3);}preview.close();
 for(int player=0;player<2;player++){
  const auto target=*input.aim_targets[player];require(close(glm::vec3(first[player].model[3]),{target.x,target.y,target.z}),"GPU reticle moved away from real3D aim hit");
  for(int axis=0;axis<3;axis++)require(close(glm::vec3(first[player].model[axis]),glm::vec3(input.head[axis])),"GPU reticle did not use shared head orientation");
  auto delta=glm::vec3(target.x,target.y,target.z)-glm::vec3(input.head[3]);double reference=reticle_size_reference(glm::length(delta),settings.panel_distance,{settings.reticle_min_size,settings.reticle_shrink_distance,settings.reticle_max_size});
  require(std::abs(first[player].size.x-28*reference/400)<1e-5&&std::abs(first[player].size.y-28*reference/450)<1e-5,"GPU size differs from Gunblade policy/original per-game focal");
  require(close(first[player].color,player?glm::vec3(.125,.4,1):glm::vec3(1,.125,.125)),"GPU reticle logical-player color mismatch");
 }
 auto other_eye=draw(base*glm::translate(glm::mat4(1),glm::vec3(.064,0,0)));for(int p=0;p<2;p++){require(close(glm::vec3(other_eye[p].model[3]),glm::vec3(first[p].model[3]))&&glm::length(other_eye[p].size-first[p].size)<1e-6f,"Eyes changed target or perceived size");}
 settings.laser=true;draw(base);require(markers.size()==2&&beams==2&&!dots,"Laser On must draw separate beam while Custom replaces endpoint dots");
 settings.reticle_mode=ReticleMode::None;draw(base);require(markers.empty()&&beams==2&&!dots,"None did not remove every gameplay reticle/dot while preserving Laser");
 input.focused=false;ui.update(input,settings,runtime,3);draw(base);require(markers.empty()&&!dots,"Focus loss reintroduced a None gameplay endpoint dot");
 input.focused=true;settings.reticle_mode=ReticleMode::Custom;settings.laser=false;ui.update(input,settings,runtime,4);
 cabinet.hand={1,0};ui.feedback(cabinet,4);draw(base);require(markers.size()==2&&close(glm::vec3(markers[0].model[3]),{.4,.1,-4})&&close(markers[0].color,{1,.125,.125}),"Dual hand swap changed logical color/target ownership");
 runtime.state.players[0].state=0;runtime.state.players[0].health=0;
 for(int hand:{0,1}){cabinet.hand={-1,hand};ui.feedback(cabinet,4);draw(base);require(markers.size()==1&&close(markers[0].color,{.125,.4,1})&&close(glm::vec3(markers[0].model[3]),hand?glm::vec3(.4,.1,-4):glm::vec3(-.4,.1,-4)),"P2-only hand transfer lost player identity");}
 input.hands[1].tracked=false;ui.update(input,settings,runtime,5);draw(base);require(markers.empty(),"Tracking loss retained stale gameplay marker");input.hands[1].tracked=true;
 input.aim_targets[1]=Vec3{NAN,0,-4};ui.update(input,settings,runtime,6);draw(base);require(markers.empty(),"Nonfinite real hit emitted a custom marker");
 input.aim_targets[1].reset();ui.update(input,settings,runtime,7);draw(base);require(markers.empty(),"Missing real hit fabricated fallback custom marker");
 input.aim_targets[1]=Vec3{.4,.1,-4};input.head[0][0]=NAN;ui.update(input,settings,runtime,8);draw(base);require(markers.empty(),"Nonfinite head emitted a marker");input.head=glm::mat4(1);input.reticle_focal={NAN,450};ui.update(input,settings,runtime,9);draw(base);require(markers.empty(),"Nonfinite lens emitted a marker");
 // Exercise the actual menu, including every supported size step and clamps.
 Presentation menu;PresentationInput controls;Settings preferences;SharedRuntime isolated;double now=10;
 auto update=[&](){menu.update(controls,preferences,isolated,now);now+=.2;};auto move=[&](float x,float y){controls.stick={x,y};update();controls.stick={};update();};auto down=[&](int n){for(int i=0;i<n;i++)move(0,-1);};
 update();controls.toggle=true;update();controls.toggle=false;update();require(menu.open(),"Menu failed to open");
 down(7);move(1,0);require(preferences.reticle_mode==ReticleMode::None,"Reticle must immediately follow Laser");move(1,0);require(preferences.reticle_mode==ReticleMode::Custom,"Unavailable native choice appeared in forward cycle");move(-1,0);require(preferences.reticle_mode==ReticleMode::None,"Backward cycle is not Custom/None only");
 down(1);preferences.reticle_min_size=.05f;move(-1,0);require(preferences.reticle_min_size==.05f,"Minimum size below5%");for(int i=6;i<=100;i++){move(1,0);require(std::abs(preferences.reticle_min_size-i/100.f)<1e-6,"Minimum size skipped1percentage-point step");}move(1,0);require(preferences.reticle_min_size==1,"Minimum size exceeded100%");
 down(1);preferences.reticle_max_size=.25f;move(-1,0);require(preferences.reticle_max_size==.25f,"Maximum size below25%");for(int i=30;i<=300;i+=5){move(1,0);require(std::abs(preferences.reticle_max_size-i/100.f)<1e-5,"Maximum size skipped5% step");}move(1,0);require(preferences.reticle_max_size==3,"Maximum size exceeded300%");
 down(1);preferences.reticle_shrink_distance=.2f;move(-1,0);require(preferences.reticle_shrink_distance==.2f,"Shrink below0.2m");for(int i=3;i<=200;i++){move(1,0);require(std::abs(preferences.reticle_shrink_distance-i/10.f)<1e-5,"Shrink skipped0.1m step");}move(1,0);require(preferences.reticle_shrink_distance==20,"Shrink exceeded20m");
 std::ifstream saved(file);auto restored=read_settings(saved);require(restored.reticle_mode==ReticleMode::None&&restored.reticle_min_size==1&&restored.reticle_max_size==3&&restored.reticle_shrink_distance==20,"Actual menu changes did not persist");
 // Menu and native service pointers survive None, independently of gameplay.
 controls.reticle_focal={400,450};for(int h=0;h<2;h++){controls.hands[h].tracked=true;controls.poses[h]=input.poses[h];}update();markers.clear();dots=beams=0;inverse_vp=glm::inverse(base);menu.draw(base,preferences,isolated.state,true,now);require(markers.empty()&&dots==2,"None removed required menu pointer dots");
 for(auto& h:controls.hands)h.tracked=false;controls.toggle=true;update();controls.toggle=false;update();controls.toggle=true;update();controls.toggle=false;update();down(20);move(1,0);require(menu.service()&&!menu.open(),"Reticle rows displaced native Service");for(auto& h:controls.hands)h.tracked=true;update();markers.clear();dots=beams=0;menu.draw(base,preferences,isolated.state,true,now);require(markers.empty()&&dots==2,"None removed required service/calibration pointer dots");
 // The real Reset row restores the approved profile and writes it to disk.
{
 Presentation reset_menu;Settings chosen;SharedRuntime reset_runtime;PresentationInput reset_controls;double clock=300;
 chosen.gun=8;chosen.gun_angle=17;chosen.reload=ReloadMode::PointOutside;chosen.p1_hand=1;chosen.laser=false;
 chosen.reticle_mode=ReticleMode::None;chosen.reticle_min_size=.55f;chosen.reticle_max_size=2.f;chosen.reticle_shrink_distance=19.f;
 chosen.haptics=0;chosen.world_scale=.4f;chosen.panel_distance=4;chosen.hud_size=2;chosen.gun_scale=1.4f;chosen.wider_view=false;chosen.view_fov=130;chosen.game_zoom=true;
 auto tick=[&]{clock+=.2;reset_menu.update(reset_controls,chosen,reset_runtime,clock);};
 auto move=[&](float x,float y){reset_controls.stick={x,y};tick();reset_controls.stick={};tick();};
 auto approved=[&](const Settings& s){
  require(s.gun==0&&s.gun_angle==0&&s.reload==ReloadMode::ShootOutside&&s.p1_hand==0&&s.laser,"Reset gun/angle/reload/hand/Laser differs from approved defaults");
  require(s.reticle_mode==ReticleMode::Custom&&std::abs(s.reticle_min_size-.20f)<1e-6&&std::abs(s.reticle_max_size-.35f)<1e-6&&s.reticle_shrink_distance==7,"Reset reticle differs from captured20/35percent,7m");
  require(s.haptics==.65f&&s.world_scale==1&&s.panel_distance==1.8f&&s.hud_size==1&&s.gun_scale==1&&s.wider_view&&s.view_fov==110&&!s.game_zoom,"Reset remaining visible profile mismatch");
 };
 tick();reset_controls.toggle=true;tick();reset_controls.toggle=false;tick();for(int row=0;row<21;row++)move(0,-1);
 move(1,0);require(chosen.gun==8&&chosen.reticle_shrink_distance==19,"Stick movement must not activate Reset");
 reset_controls.hands[0].start=true;tick();reset_controls.hands[0].start=false;tick();approved(chosen);
 std::ifstream reset_saved(file);approved(read_settings(reset_saved));
}
// Production HUD menu actions and persistence, independent from reticle settings.
{
 Presentation hud_menu;Settings hud_settings;SharedRuntime hud_runtime;PresentationInput hud_controls;hud_controls.focused=true;double hud_clock=100;
 auto tick_hud=[&]{hud_clock+=.2;hud_menu.update(hud_controls,hud_settings,hud_runtime,hud_clock);};
 auto move_hud=[&](float x,float y){hud_controls.stick={x,y};tick_hud();hud_controls.stick={};tick_hud();};
 auto down_hud=[&](int n){while(n--)move_hud(0,-1);};
 tick_hud();hud_controls.toggle=true;tick_hud();hud_controls.toggle=false;tick_hud();down_hud(14);
 for(int i=95;i>=50;i-=5){move_hud(-1,0);require(std::abs(hud_settings.hud_size-i/100.f)<1e-6,"HUD decrease must be5percent after Panel distance");}
 move_hud(-1,0);require(hud_settings.hud_size==.5f,"HUD minimum clamp");
 for(int i=55;i<=200;i+=5){move_hud(1,0);require(std::abs(hud_settings.hud_size-i/100.f)<1e-6,"HUD increase must be5percent");}
 move_hud(1,0);require(hud_settings.hud_size==2,"HUD maximum clamp");
 std::ifstream hud_file(std::getenv("VCOP2_SETTINGS"));auto loaded_hud=read_settings(hud_file);require(loaded_hud.hud_size==2,"HUD production menu saves choice");hud_file.close();
 require(hud_settings.panel_distance==1.8f&&hud_settings.reticle_max_size==.35f,"HUD edits leave panel and reticle unchanged");
 down_hud(7);hud_controls.hands[0].start=true;tick_hud();hud_controls.hands[0].start=false;tick_hud();require(hud_settings.hud_size==1,"Explicit Reset restores HUD default");
 std::ifstream reset_file(std::getenv("VCOP2_SETTINGS"));require(read_settings(reset_file).hud_size==1,"HUD Reset persists");
}
 require(glGetError()==GL_NO_ERROR,"Reticle GL state error");glad_glDrawArrays=previous;glDeleteRenderbuffers(1,&depth);glDeleteTextures(1,&image);glDeleteFramebuffers(1,&fbo);
 std::cout<<"Actual GPU mesh/target/focal/head/stereo/colors/Laser/None/focus/tracking plus production menu ranges/save-load/service passed\n";return 0;
}catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
