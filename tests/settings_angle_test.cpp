// SPDX-License-Identifier: GPL-2.0-or-later
#include "../native/settings_io.h"
#include <iostream>
#include <sstream>
#include <filesystem>
#include <fstream>
#include <chrono>
#define CHECK(x) do {if(!(x)){std::cerr<<"Failed " #x " line "<<__LINE__<<'\n';return 1;}}while(0)
int main(){using namespace vcop2;
 for(int angle:{-30,-12,0,18,30}){
  std::istringstream old("version=6\ngun_angle="+std::to_string(angle)+"\npanel_distance=2.7\np1_hand=1\nhaptics=0.35\n");auto migrated=read_settings(old);CHECK(migrated.gun_angle==0);CHECK(migrated.panel_distance==2.7f&&migrated.p1_hand==1&&migrated.haptics==.35f);
  migrated.gun_angle=17;std::ostringstream saved;write_settings(saved,migrated);CHECK(saved.str().find("gun_angle_zero_version=1\n")!=std::string::npos);std::istringstream next(saved.str());auto restored=read_settings(next);CHECK(restored.gun_angle==17&&restored.panel_distance==2.7f&&restored.p1_hand==1);
 }
 for(const char* invalid:{"-1","0","nan","1junk","1.5"}){std::istringstream old(std::string("version=6\ngun_angle=12\ngun_angle_zero_version=")+invalid+"\n");CHECK(read_settings(old).gun_angle==0);}
 for(const char* invalid:{"-31","31","nan","inf","12junk"}){std::istringstream bad(std::string("version=6\ngun_angle_zero_version=1\ngun_angle=")+invalid+"\n");CHECK(read_settings(bad).gun_angle==0);}

 const auto path=std::filesystem::temp_directory_path()/("vr4all-angle-vcop2-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count())+".ini");
 {std::ofstream out(path);out<<"version=6\ngun_angle=23\npanel_distance=2.7\ncalibration_native=442,169\nunknown_future_setting=keep me\n";}
 CHECK(load_settings(path.string().c_str()).gun_angle==0);
 std::ifstream persisted(path);std::string text{std::istreambuf_iterator<char>(persisted),{}};persisted.close();
 CHECK(text.find("gun_angle=0\n")!=std::string::npos&&text.find("gun_angle_zero_version=1\n")!=std::string::npos);
 CHECK(text.find("calibration_native=442,169\n")!=std::string::npos&&text.find("unknown_future_setting=keep me\n")!=std::string::npos);
 CHECK(load_settings(path.string().c_str()).panel_distance==2.7f);
 {auto at=text.find("gun_angle=0\n");text.replace(at,12,"gun_angle=-19\n");std::ofstream out(path);out<<text;}
 CHECK(load_settings(path.string().c_str()).gun_angle==-19);
 std::ifstream unchanged(path);std::string after{std::istreambuf_iterator<char>(unchanged),{}};unchanged.close();CHECK(after==text);
 std::filesystem::remove(path);CHECK(load_settings(path.string().c_str()).gun_angle==0&&!std::filesystem::exists(path));
 std::cout<<"Legacy angles reset once and later preferences survive save/load\n";
}
