// SPDX-License-Identifier: GPL-2.0-or-later
// Adapted from mikermak's VR4ALL xr_gun.cpp (Copyright 2026 mikermak).
#pragma once
#include "shared/ui_compat.h"
#include <glm/glm.hpp>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <vector>

namespace vcop2 {
class GunEffects {
    struct Vertex {glm::vec3 position;glm::vec2 corner;glm::vec4 color;};
    struct Puff {glm::vec3 position,velocity;double born;float size;};
    GLuint program_{},vao_{},buffer_{};
    std::vector<Puff> smoke_;
    std::array<double,2> emitted_{-100,-100};
    std::array<unsigned,2> sequence_{};
    static float random(unsigned n,unsigned k) {
        unsigned h=n*747796405u+k*2891336453u+0x9e3779b9u;
        h^=h>>16;h*=0x7feb352du;h^=h>>15;h*=0x846ca68bu;h^=h>>16;
        return (h&0xffffff)/float(0x1000000);
    }
    static glm::vec3 side(const glm::vec3& forward,const glm::vec3& to_eye) {
        auto v=glm::cross(forward,to_eye);
        if(glm::length(v)<1e-6f)v=glm::cross(forward,glm::vec3(0,1,0));
        if(glm::length(v)<1e-6f)v=glm::cross(forward,glm::vec3(1,0,0));
        return glm::normalize(v);
    }
    static void quad(std::vector<Vertex>& out,const Vertex& a,const Vertex& b,const Vertex& c,const Vertex& d) {
        for(const auto& v:{a,b,c,a,c,d})out.push_back(v);
    }
    static void billboard(std::vector<Vertex>& out,glm::vec3 at,glm::vec3 eye,float size,glm::vec4 color) {
        auto to_eye=eye-at;if(glm::length(to_eye)<1e-6f)to_eye={0,0,1};else to_eye=glm::normalize(to_eye);
        auto right=side(glm::vec3(0,1,0),to_eye)*size,up=glm::cross(to_eye,right);
        quad(out,{at-right-up,{-1,-1},color},{at+right-up,{1,-1},color},{at+right+up,{1,1},color},{at-right+up,{-1,1},color});
    }
    static void flame(std::vector<Vertex>& out,glm::vec3 from,glm::vec3 forward,glm::vec3 eye,float length,float width,glm::vec4 color) {
        auto r=side(forward,eye-from)*width,to=from+forward*length;
        quad(out,{from-r,{0,-1},color},{from+r,{0,1},color},{to+r,{1,1},color},{to-r,{1,-1},color});
    }
    bool init() {
        if(program_)return true;
        OpenGlSource vs,fs;
        vs.addSource(R"(
in vec3 in_pos;in vec2 in_uv;in vec4 in_color;uniform mat4 mvp;
out vec2 corner;out vec4 color;
void main(){gl_Position=mvp*vec4(in_pos,1);corner=in_uv;color=in_color;})");
        fs.addSource(R"(
in vec2 corner;in vec4 color;uniform int shape;uniform float angle;out vec4 result;
void main(){float a=color.a;float r=length(corner);
if(shape==0)a*=1.-smoothstep(0.,1.,r);
else if(shape==1)a*=pow(1.-clamp(corner.x,0.,1.),1.6)*(1.-corner.y*corner.y);
else {float rays=pow(abs(cos((atan(corner.y,corner.x)+angle)*3.)),10.);
a*=clamp((1.-r)*(.25+1.6*rays)+(1.-smoothstep(0.,.4,r)),0.,1.);}
result=vec4(color.rgb,a);})");
        program_=gl_CompileAndLink(vs.generate().c_str(),fs.generate().c_str());if(!program_)return false;
        glGenVertexArrays(1,&vao_);glBindVertexArray(vao_);glGenBuffers(1,&buffer_);glBindBuffer(GL_ARRAY_BUFFER,buffer_);
        glEnableVertexAttribArray(0);glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,sizeof(Vertex),reinterpret_cast<void*>(offsetof(Vertex,position)));
        glEnableVertexAttribArray(1);glVertexAttribPointer(1,2,GL_FLOAT,GL_FALSE,sizeof(Vertex),reinterpret_cast<void*>(offsetof(Vertex,corner)));
        glEnableVertexAttribArray(3);glVertexAttribPointer(3,4,GL_FLOAT,GL_FALSE,sizeof(Vertex),reinterpret_cast<void*>(offsetof(Vertex,color)));
        return true;
    }
    void draw(const glm::mat4& vp,const std::vector<Vertex>& vertices,int shape,bool smoke,float angle=0) {
        if(vertices.empty()||!init())return;
        glUseProgram(program_);glUniformMatrix4fv(glGetUniformLocation(program_,"mvp"),1,GL_FALSE,&vp[0][0]);
        glUniform1i(glGetUniformLocation(program_,"shape"),shape);glUniform1f(glGetUniformLocation(program_,"angle"),angle);
        if(smoke){glEnable(GL_DEPTH_TEST);glDepthFunc(GL_LESS);}else glDisable(GL_DEPTH_TEST);
        glDepthMask(GL_FALSE);glDisable(GL_CULL_FACE);glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA,smoke?GL_ONE_MINUS_SRC_ALPHA:GL_ONE);
        glBindVertexArray(vao_);glBindBuffer(GL_ARRAY_BUFFER,buffer_);
        glBufferData(GL_ARRAY_BUFFER,vertices.size()*sizeof(Vertex),vertices.data(),GL_STREAM_DRAW);
        glDrawArrays(GL_TRIANGLES,0,GLsizei(vertices.size()));glDepthMask(GL_TRUE);glDisable(GL_BLEND);
    }
public:
    ~GunEffects(){if(buffer_)glDeleteBuffers(1,&buffer_);if(vao_)glDeleteVertexArrays(1,&vao_);if(program_)glDeleteProgram(program_);}
    void clear(){smoke_.clear();emitted_={-100,-100};}
    void emit(int player,double born,double now,glm::vec3 muzzle,glm::vec3 forward,float scale) {
        if(emitted_[player]==born)return;emitted_[player]=born;
        if(now<born||now-born>=.1)return;
        auto shot=++sequence_[player];
        for(unsigned k=0;k<3;k++) {
            float spread=random(shot,k)-.5f;
            smoke_.push_back({muzzle+forward*((.01f+.02f*k)*scale),
                forward*(.18f+.1f*k)+glm::vec3(spread*.04f,.05f+.04f*spread,0),born,(.018f+.006f*k)*scale});
        }
        if(smoke_.size()>36)smoke_.erase(smoke_.begin(),smoke_.end()-36);
    }
    void smoke(const glm::mat4& vp,glm::vec3 eye,double now) {
        std::erase_if(smoke_,[&](const auto& p){return now<p.born||now-p.born>=.9;});
        std::vector<Vertex> vertices;vertices.reserve(smoke_.size()*6);
        for(const auto& p:smoke_) {
            float age=float(now-p.born),t=age/.9f;
            auto at=p.position+p.velocity*(.35f*(1-std::exp(-age/.35f)))+glm::vec3(0,.03f*age,0);
            float alpha=.22f*std::min(1.f,age/.06f)*(1-t)*(1-t);
            billboard(vertices,at,eye,p.size*(1+3.5f*t),{.62f,.6f,.58f,alpha});
        }
        draw(vp,vertices,0,true);
    }
    void flash(const glm::mat4& vp,glm::vec3 eye,int player,double age,glm::vec3 muzzle,glm::vec3 forward,float scale) {
        float strength=age<0?0:age<.02?1:std::max(0.f,1-float(age-.02)/.05f);if(strength<=0)return;
        std::vector<Vertex> flames,stars;flames.reserve(12);stars.reserve(12);
        auto shot=sequence_[player];float reach=(.07f+.06f*random(shot,7))*scale;
        auto tip=muzzle+forward*(.006f*scale);
        flame(flames,tip,forward,eye,reach*(.6f+.4f*strength),.016f*scale,{1,.55f,.15f,strength});
        flame(flames,tip,forward,eye,reach*.55f,.007f*scale,{1,.95f,.75f,strength});
        auto at=muzzle+forward*(.012f*scale);
        billboard(stars,at,eye,(.035f+.02f*strength)*scale,{1,.7f,.25f,strength});
        billboard(stars,at,eye,.016f*scale,{1,1,.9f,strength});
        // The short glow at the nose remains visible from behind the gun, as in the peer.
        draw(vp,flames,1,false);draw(vp,stars,2,false,6.2831853f*random(shot,3));
    }
};
}
