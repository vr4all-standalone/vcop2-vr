// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <array>
#include <cmath>
struct ArcadeDraw { GLsizei count{};std::array<float,4> color{};glm::mat4 model{1}; };
inline std::vector<ArcadeDraw> arcade_draws;
inline PFNGLDRAWELEMENTSPROC original_draw_elements;
inline void GLAD_API_PTR record_arcade_draw(GLenum mode,GLsizei count,GLenum type,const void* indices){
    GLint program{};glGetIntegerv(GL_CURRENT_PROGRAM,&program);
    const GLint base=glGetUniformLocation(program,"base"),model=glGetUniformLocation(program,"model");
    if(base>=0&&model>=0&&(count==20688||count==29550||count==480||count==576)){
        ArcadeDraw draw;draw.count=count;glGetUniformfv(program,base,draw.color.data());glGetUniformfv(program,model,&draw.model[0][0]);arcade_draws.push_back(draw);
    }
    original_draw_elements(mode,count,type,indices);
}
inline void probe_arcade_contract(const glm::mat4& vp,bool reversed){
    Presentation guns;Settings options;options.gun=0;options.laser=false;SharedRuntime runtime;GameState game;game.valid=true;
    game.playing=true;for(auto& p:game.players){p.state=1;p.health=100;}
    runtime.state=game;PresentationInput hands;hands.focused=true;CabinetInput cabinet;cabinet.hand={0,1};cabinet.coin_mask=1;guns.feedback(cabinet,0);
    for(int h=0;h<2;h++){hands.hands[h].tracked=true;hands.poses[h]=glm::translate(glm::mat4(1),glm::vec3(h==0?-.16f:.16f,0,-.65f));}
    const bool material=std::getenv("GUN_CONTRACT_MATERIAL")!=nullptr;
    original_draw_elements=glad_glDrawElements;glad_glDrawElements=record_arcade_draw;
    auto render=[&](float angle){options.gun_angle=angle;guns.update(hands,options,runtime,1);arcade_draws.clear();glDepthMask(GL_TRUE);glClearColor(0,0,0,1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);guns.draw(vp,options,game,true,1,false);check(arcade_draws.size()==8,"both Arcade meshes reach the real GPU draw path");};
    if(!material){
        for(auto pair:{std::array<float,2>{-100,-40},{-30,-40},{0,-10},{7,-3},{30,20},{100,20}}){
            render(pair[0]);const float radians=pair[1]*3.14159265358979323846f/180;
            for(const auto& draw:arcade_draws){auto direction=glm::normalize(glm::vec3(draw.model*glm::vec4(0,0,-1,0)));check(glm::distance(direction,glm::vec3(0,std::sin(radians),-std::cos(radians)))<1e-5f,"GPU gun pose uses clamped displayed angle minus ten degrees");}
        }
    }else{
        for(auto map:{std::array<int,2>{0,1},{1,0},{-1,0},{1,-1}}){
            cabinet.hand=map;guns.feedback(cabinet,0);options.p1_hand=map[0]>=0?map[0]:1-map[1];
            // Both players must be visible for the paired permutations; the solo
            // permutations additionally prove that P1/P2 identity survives hand transfer.
            options.gun_angle=0;guns.update(hands,options,runtime,1);arcade_draws.clear();glDepthMask(GL_TRUE);glClearColor(0,0,0,1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);guns.draw(vp,options,game,true,1,false);
            const size_t players=(map[0]>=0)+(map[1]>=0);check(arcade_draws.size()==players*4,"only assigned logical players render Arcade guns");
            size_t draw_at=0;
            for(int player=0;player<2;player++)if(map[player]>=0){
                const bool blue=(player==1)!=reversed;
                const std::array<float,4> plastic=blue?std::array<float,4>{.04f,.25f,1.f,1.f}:std::array<float,4>{.90f,.04f,.04f,1.f};
                for(int part=0;part<4;part++){const auto& draw=arcade_draws[draw_at++];const auto want=part<2?plastic:part==2?std::array<float,4>{.15f,.15f,.17f,1}:std::array<float,4>{.12f,.22f,.3f,1};check(draw.color==want,"GPU material colors full body and grip by logical player while retaining metal and lens");}
            }
            std::vector<unsigned char> pixels(256*256*4);glReadPixels(0,0,256,256,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data());int colored=0;for(size_t i=0;i<pixels.size();i+=4)if((pixels[i]>50&&pixels[i]>pixels[i+2]*5)||(pixels[i+2]>50&&pixels[i+2]>pixels[i]*5))++colored;check(colored>100,"player plastic colors are visible in actual framebuffer pixels");
        }
    }
    glad_glDrawElements=original_draw_elements;
}
