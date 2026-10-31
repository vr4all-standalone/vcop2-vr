/*
    Controller callouts adapted from hotd2-vr xr_labels.cpp.
    Copyright 2026 mikermak. GNU GPL v2 or later.
    Button points are the Confidential Mission sidecars for the same bundled
    declan02530 controller GLBs (asset attribution: assets/models/CREDITS.txt).
*/
#pragma once
#include "shared/xr_panel.h"
#include <algorithm>
#include <array>

namespace vcop2::controller_help {
class Labels {
    // The reference's physical sizes: 16 mm plates, 21 mm pitch, 1.2 mm lines.
    static constexpr float Height=.016f,Gap=.025f,Pitch=.021f,StickRise=.026f;
    static constexpr float LineWidth=.0012f,Dot=.0024f;
    enum Button {Pump,Start,Menu,Trigger,Stick,Grip,Count};
    struct Picture {unsigned texture{};float hx{},hy{};};
    struct Placed {int button{};glm::vec3 centre{},from{},to{};};
    struct Layout {
        glm::mat4 model{0};glm::vec3 eye{};int key{-1};size_t count{};
        std::array<Picture,Count> pictures;std::array<Placed,Count> placed;
    };
    std::array<Layout,2> layouts_;
    inline static const glm::vec3 Points[2][Count]={
        {{-.0155f,.0067f,.0028f},{-.0049f,.0046f,-.0097f},{.0114f,.0021f,-.0109f},{-.0142f,-.0159f,.0254f},{.0063f,.0112f,.007f},{-.014245f,-.035311f,.031092f}},
        {{.0154f,.0067f,.0028f},{.0048f,.0046f,-.0097f},{0,0,0},{.0142f,-.0159f,.0254f},{-.0065f,.0111f,.007f},{.014245f,-.035311f,.031092f}}
    };
    // The squeeze-button surface centroid after loadGlb bakes the node's
    // +90-degree X rotation (x,y,z -> x,-z,y). Keep text layout guides above.
    inline static const glm::vec3 GripOrigin[2]={
        {-.013753f,-.028802f,-.026018f},{.013753f,-.028802f,-.026018f}
    };
    static glm::vec3 eyeOf(const glm::mat4& vp) {
        constexpr int rows[]={0,1,3};glm::mat3 m;glm::vec3 b;
        for(int i=0;i<3;i++){for(int c=0;c<3;c++)m[c][i]=vp[c][rows[i]];b[i]=-vp[3][rows[i]];}
        return glm::inverse(m)*b;
    }
    static void facing(const glm::vec3& at,const glm::vec3& eye,const glm::vec3& fallback,glm::vec3& across,glm::vec3& up,glm::vec3& toEye) {
        toEye=eye-at;toEye=glm::length(toEye)>1e-4f?glm::normalize(toEye):glm::vec3(0,0,1);
        across=glm::cross(glm::vec3(0,1,0),toEye);
        if(glm::length(across)<1e-3f)across=fallback-toEye*glm::dot(fallback,toEye);
        if(glm::length(across)<1e-3f)across=glm::cross(glm::vec3(0,0,1),toEye);
        across=glm::normalize(across);up=glm::cross(toEye,across);
    }
    static glm::mat4 billboard(const glm::vec3& at,const glm::vec3& eye) {
        glm::vec3 x,y,z;facing(at,eye,{1,0,0},x,y,z);
        return {glm::vec4(x,0),glm::vec4(y,0),glm::vec4(z,0),glm::vec4(at,1)};
    }
    static void pictures(Layout& l,int hand,int startPlayer,int gunPlayer) {
        const char* pump[2][2]={{"Y - P1 Reload","Y - P2 Reload"},{"B - P1 Reload","B - P2 Reload"}};
        const char* start[2][2]={{"X - P1 Start","X - P2 Start"},{"A - P1 Start","A - P2 Start"}};
        const char* shoot[]={"Trigger - P1 Shoot","Trigger - P2 Shoot"};
        const char* words[]={pump[hand][gunPlayer],start[hand][startPlayer],"Menu",shoot[gunPlayer],hand==0?"Stick - Recenter":"Stick - Laser","Grip - Toggle zoom"};
        static_assert(std::size(words)==Count);
        for(int b=0;b<Count;b++) {
            if(hand==1&&b==Menu)continue;
            int w{},h{};auto& p=l.pictures[b];p.texture=vr::xr::textTexture(words[b],40,glm::vec3(.96f),w,h);
            p.hy=Height*.5f;p.hx=h?p.hy*w/h:0;
        }
    }
    static void layOut(Layout& l,int hand,const glm::mat4& model,const glm::vec3& eye) {
        std::array<glm::vec3,Count> at;glm::vec3 middle(0);int points=0;
        for(int b=0;b<Count;b++)if(hand==0||b!=Menu){at[b]=glm::vec3(model*glm::vec4(Points[hand][b],1));middle+=at[b];++points;}
        middle/=float(points);glm::vec3 across,up,toEye;facing(middle,eye,glm::vec3(model[0]),across,up,toEye);
        auto x=[&](int b){return glm::dot(at[b]-middle,across);};
        auto y=[&](int b){return glm::dot(at[b]-middle,up);};
        auto room=[&](float px,float py){return middle+across*px+up*py;};
        const float side=hand==1?1.f:-1.f;float outer=0,inner=0;
        for(int b=0;b<Count;b++)if(hand==0||b!=Menu){outer=std::max(outer,side*x(b));inner=std::max(inner,-side*x(b));}
        const float column=side*(outer+Gap);std::array<float,3> ys{};float buttonsY=0,labelsY=0;
        const int rows=hand==0?3:2;
        for(int b=0;b<rows;b++){buttonsY+=y(b);ys[b]=b?std::min(y(b),ys[b-1]-Pitch):y(b);labelsY+=ys[b];}
        const float shift=(buttonsY-labelsY)/rows;for(int b=0;b<rows;b++)ys[b]+=shift;
        l.count=0;
        auto place=[&](int b,float px,float py,float align,const glm::vec3& to,bool toOuter=false){
            const auto& picture=l.pictures[b];if(!picture.texture)return;
            const float centreX=px+align*picture.hx;
            const auto from=b==Grip?glm::vec3(model*glm::vec4(GripOrigin[hand],1)):at[b];
            l.placed[l.count++]={b,room(centreX,py),from,toOuter?room(centreX+side*picture.hx,py):to};
        };
        for(int b=0;b<rows;b++)place(b,column,ys[b],side,room(column,ys[b]));
        const float gripY=std::min(y(Grip),ys[rows-1]-Pitch);
        const float gripColumn=-side*(inner+Gap);
        place(Grip,gripColumn,gripY,-side,room(gripColumn,gripY));
        const float stickX=x(Stick),stickY=std::max(y(Stick),ys[0])+StickRise;
        place(Stick,stickX,stickY,0,room(stickX,stickY-Height*.5f));
        place(Trigger,stickX,stickY+Pitch,0,glm::vec3(0),true);
    }
public:
    // model is exactly the model matrix used to draw this bundled controller.
    // Both eyes share the layout until the controller or viewpoint moves.
    void draw(const glm::mat4& vp,int hand,const glm::mat4& model,int startPlayer,int gunPlayer) {
        auto& l=layouts_[hand];const auto eye=eyeOf(vp);const int key=startPlayer*2+gunPlayer;
        const bool wordsChanged=l.key!=key;
        if(wordsChanged)pictures(l,hand,startPlayer,gunPlayer);
        if(wordsChanged||l.model!=model||l.count==0||glm::length(eye-l.eye)>.1f){l.model=model;l.eye=eye;l.key=key;layOut(l,hand,model,eye);}
        const glm::vec4 colour(.92f,.92f,.92f,.85f);
        for(size_t i=0;i<l.count;i++) {
            const auto& p=l.placed[i];const auto along=p.to-p.from;const float length=glm::length(along);
            if(length>1e-4f) {
                const auto middle=(p.from+p.to)*.5f,x=along/length;auto y=glm::cross(x,eye-middle);
                if(glm::length(y)>1e-6f){y=glm::normalize(y);glm::mat4 line(glm::vec4(x,0),glm::vec4(y,0),glm::vec4(glm::cross(x,y),0),glm::vec4(middle,1));vr::xr::drawTexture(vp,line,0,length*.5f,LineWidth*.5f,colour);}
            }
            vr::xr::drawTexture(vp,billboard(p.from,eye),0,Dot*.5f,Dot*.5f,colour);
        }
        for(size_t i=0;i<l.count;i++){const auto& p=l.placed[i];const auto& picture=l.pictures[p.button];vr::xr::drawTexture(vp,billboard(p.centre,eye),picture.texture,picture.hx,picture.hy);}
    }
};
}
