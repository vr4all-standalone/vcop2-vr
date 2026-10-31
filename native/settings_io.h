// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "input_policy.h"
#include "weapon_choice.h"
#include <istream>
#include <fstream>
#include <iterator>
#include <ostream>
#include <sstream>
#include <string>
#include <vector>
namespace vcop2 {
inline Settings read_settings(std::istream& input,bool* angle_migrated=nullptr) {
    bool angle_zero_version=false;
    Settings out;std::string line;std::vector<std::string> lines;bool immersive_version=false,original_gun_version=false,three_gun_version=false;
    while(std::getline(input,line)){if(line=="version=2"||line=="version=3"||line=="version=4"||line=="version=5"||line=="version=6")immersive_version=true;if(line=="version=3"||line=="version=4"||line=="version=5"||line=="version=6")original_gun_version=true;if(line=="version=4"||line=="version=5"||line=="version=6")three_gun_version=true;lines.push_back(line);}
    for(const auto& line:lines) {
        auto equals=line.find('=');if(equals==std::string::npos)continue;
        auto key=line.substr(0,equals);std::istringstream value(line.substr(equals+1));double number{};char extra{};
        if(!(value>>number)||(value>>extra)||!std::isfinite(number))continue;
        auto integer=[&](int lo,int hi){return number==std::floor(number)&&number>=lo&&number<=hi;};
        if(key=="gun_angle_zero_version"&&integer(1,2147483647))angle_zero_version=true;
        else if(key=="p1_hand"&&integer(0,1))out.p1_hand=int(number);
        else if(key=="reload"&&integer(0,2))out.reload=ReloadMode(int(number));
        else if(key=="gun"&&integer(0,8)) {
            const int choice=int(number);
            if(three_gun_version) {if(selectable_gun_choice(choice))out.gun=choice;}
            else out.gun=choice==1?1:choice==0&&original_gun_version?0:AutoOriginalGun;
        }
        else if(key=="laser"&&integer(0,1))out.laser=number!=0;
        else if(key=="reticle_mode"&&integer(1,2))out.reticle_mode=reticle_mode_value(int(number));
        else if(key=="reticle_min_size"&&number>=.05&&number<=1)out.reticle_min_size=float(number);
        else if(key=="reticle_max_size"&&number>=.25&&number<=3)out.reticle_max_size=float(number);
        else if(key=="reticle_shrink_distance"&&number>=.2&&number<=20)out.reticle_shrink_distance=float(number);
        // Game zoom is session-only: ignore legacy saved On values so every
        // new VR launch starts in the accepted unzoomed presentation.
        else if(key=="wider_view"&&immersive_version&&integer(0,1))out.wider_view=number!=0;
        else if(key=="background"&&integer(0,1))out.background=BackgroundMode(int(number));
        else if(key=="view_fov"&&immersive_version&&number>=80&&number<=140)out.view_fov=float(number);
        else if(key=="world_scale"&&immersive_version&&number>=.01&&number<=2)out.world_scale=float(number);
        else if(key=="hud_size"&&number>=.5&&number<=2)out.hud_size=float(number);
        else if(key=="panel_distance"&&number>=.5&&number<=10)out.panel_distance=float(number);
        else if(key=="gun_angle"&&number>=-30&&number<=30)out.gun_angle=float(number);
        else if(key=="gun_scale"&&number>=.4&&number<=1.6)out.gun_scale=float(number);
        else if(key=="haptics"&&number>=0&&number<=1)out.haptics=HapticLevels[haptic_level(float(number))];
    }
    // Missing or invalid values retain Settings{}; valid saved tuning is preserved.
    if(!angle_zero_version)out.gun_angle=0;
    if(angle_migrated)*angle_migrated=!angle_zero_version;
    return out;
}
inline void write_settings(std::ostream& out,const Settings& s) {
    out<<"version=6\ngun_angle_zero_version=1\np1_hand="<<s.p1_hand<<"\nreload="<<int(s.reload)<<"\ngun="<<(selectable_gun_choice(s.gun)?s.gun:AutoOriginalGun)<<"\nlaser="<<s.laser
       <<"\nview_fov="<<s.view_fov<<"\nwider_view="<<s.wider_view<<"\nworld_scale="<<s.world_scale<<"\npanel_distance="<<s.panel_distance<<"\nhud_size="<<s.hud_size<<"\ngun_angle="<<s.gun_angle<<"\ngun_scale="<<s.gun_scale<<"\nreticle_mode="<<int(reticle_mode_value(int(s.reticle_mode)))<<"\nreticle_min_size="<<s.reticle_min_size<<"\nreticle_max_size="<<s.reticle_max_size<<"\nreticle_shrink_distance="<<s.reticle_shrink_distance<<"\nhaptics="<<s.haptics<<"\nbackground="<<int(s.background)<<'\n';
}
// Persist migration at the first real settings load, without a Settings ABI
// field or rewriting unrelated/unknown preferences and calibration entries.
inline Settings load_settings(const char* path) {
    std::ifstream file(path);if(!file)return Settings{};
    std::string text{std::istreambuf_iterator<char>(file),std::istreambuf_iterator<char>()};file.close();
    std::istringstream input(text);bool migrated=false;auto settings=read_settings(input,&migrated);
    if(migrated) {
        std::istringstream old(text);std::ostringstream updated;std::string line;bool angle=false,marker=false;
        while(std::getline(old,line)){
            auto key=line.substr(0,line.find('='));
            if(key=="gun_angle"){updated<<"gun_angle=0\n";angle=true;}
            else if(key=="gun_angle_zero_version"){updated<<"gun_angle_zero_version=1\n";marker=true;}
            else updated<<line<<'\n';
        }
        if(!angle)updated<<"gun_angle=0\n";if(!marker)updated<<"gun_angle_zero_version=1\n";
        std::ofstream output(path);if(output)output<<updated.str();
    }
    return settings;
}

}
