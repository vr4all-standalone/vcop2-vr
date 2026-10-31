// SPDX-License-Identifier: GPL-2.0-or-later
#include "../native/gl_context.h"
#include "../native/presentation.h"
#include <glad/gl.h>
#include <glm/gtc/matrix_transform.hpp>
#include <cstdlib>
#include <iostream>
#include <vector>
#define CHECK(x) do{if(!(x)){std::cerr<<"Failed " #x " line "<<__LINE__<<'\n';return 1;}}while(0)
using namespace vcop2;
int main(int argc,char**argv){
    if(argc!=2)return 2;_putenv_s("VCOP2_ASSETS",argv[1]);GLContext context;
    GLuint fbo,color,depth;glGenFramebuffers(1,&fbo);glBindFramebuffer(GL_FRAMEBUFFER,fbo);
    glGenTextures(1,&color);glBindTexture(GL_TEXTURE_2D,color);glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,256,256,0,GL_RGBA,GL_UNSIGNED_BYTE,nullptr);glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,color,0);
    glGenRenderbuffers(1,&depth);glBindRenderbuffer(GL_RENDERBUFFER,depth);glRenderbufferStorage(GL_RENDERBUFFER,GL_DEPTH_COMPONENT24,256,256);glFramebufferRenderbuffer(GL_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,GL_RENDERBUFFER,depth);
    CHECK(glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE);glViewport(0,0,256,256);
    Presentation ui;PresentationInput input;Settings s;s.laser=false;SharedRuntime runtime;
    runtime.state.valid=runtime.state.ready=runtime.state.playing=true;runtime.state.mode=8;runtime.state.submode=8;
    CabinetInput joined;joined.coin_mask=1;ui.feedback(joined,0);ui.update(input,s,runtime,0);
    const auto projection=glm::perspective(glm::radians(70.f),1.f,.05f,100.f);
    auto draw=[&](double now,const glm::mat4& vp){glDepthMask(GL_TRUE);glClearColor(0,0,0,1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);ui.draw(vp,s,runtime.state,true,now);std::vector<uint8_t> pixels(256*256*4);glReadPixels(0,0,256,256,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data());return pixels;};
    const auto blank=draw(0,projection);CHECK(blank==draw(4,projection));
    s.game_zoom=true;ui.update(input,s,runtime,5);const auto enabled=draw(5,projection);CHECK(enabled!=blank);
    CHECK(draw(5.999,projection)==enabled&&draw(6,projection)==blank);
    size_t letters{};for(int y=0;y<256;y++)for(int x=0;x<256;x++){const size_t i=(y*256+x)*4;if(enabled[i]||enabled[i+1]||enabled[i+2]){letters++;CHECK(x>70&&x<186&&y>60&&y<100);}}
    CHECK(letters>50);
    s.game_zoom=false;ui.update(input,s,runtime,7);const auto disabled=draw(7,projection);CHECK(disabled!=blank&&disabled!=enabled);
    s.game_zoom=true;ui.update(input,s,runtime,7.5);CHECK(draw(8,projection)==enabled&&draw(8.5,projection)==blank);
    s.game_zoom=false;ui.update(input,s,runtime,10);input.focused=false;ui.update(input,s,runtime,10.1);CHECK(draw(10.2,projection)==blank);
    input.focused=true;ui.update(input,s,runtime,10.3);CHECK(draw(10.3,projection)==blank);
    s.game_zoom=true;ui.update(input,s,runtime,12);input.head=glm::translate(glm::mat4(1),glm::vec3(.3,0,0))*glm::rotate(glm::mat4(1),.4f,glm::vec3(0,1,0));ui.update(input,s,runtime,12.1);
    const auto followed=draw(12.1,projection*glm::inverse(input.head));size_t mismatch{};
    for(size_t i=0;i<enabled.size();i+=4)if(!std::equal(enabled.begin()+i,enabled.begin()+i+3,followed.begin()+i))mismatch++;
    CHECK(mismatch<30); // Float raster rounding under the cancelled head transform.
    runtime.state.service=true;ui.update(input,s,runtime,12.2);runtime.state.service=false;ui.update(input,s,runtime,12.3);CHECK(draw(12.3,projection)==blank);
    input.toggle=true;ui.update(input,s,runtime,20);input.toggle=false;ui.update(input,s,runtime,21);
    for(int row=0;row<18;row++){input.stick={0,-1};ui.update(input,s,runtime,22+row*2);input.stick={};ui.update(input,s,runtime,23+row*2);}
    input.stick={1,0};ui.update(input,s,runtime,60);CHECK(!s.game_zoom);
    const auto menu_changed=draw(60,projection*glm::inverse(input.head));
    CHECK(menu_changed!=draw(61,projection*glm::inverse(input.head)));
    CHECK(glGetError()==GL_NO_ERROR);
    std::cout<<"Zoom enabled/disabled GPU notices: one-second expiry, replacement, lower HUD, head-follow, no startup or stale-focus feedback passed\n";
}
