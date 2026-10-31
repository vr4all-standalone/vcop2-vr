// SPDX-License-Identifier: GPL-2.0-or-later
#include "gl_context.h"
#include "gl_scene.h"
#include "model2_scene.h"
#include <glad/gl.h>
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>
#define CHECK(x) do{if(!(x)){std::cerr<<"Failed " #x " line "<<__LINE__<<'\n';return 1;}}while(0)
using namespace vcop2;
int main(){
    NativeFrame f;f.width=496;f.height=384;f.yoffset=127;
    for(auto [name,size]:{std::pair{"texture0",2097152},{"texture1",2097152},{"palette",16384},{"colorxlat",49152},{"luma",32768},{"gamma",256}})f.blobs.push_back({name,std::vector<uint8_t>(size)});
    unsigned color=5|(10<<5)|(15<<10);f.blobs[2].bytes[8192]=color;f.blobs[2].bytes[8193]=color>>8;
    for(unsigned c=0;c<3;c++)for(unsigned component=0;component<32;component++)for(unsigned l=0;l<64;l++)f.blobs[3].bytes[2*(c*8192+component*256+l)]=component*4+l;
    for(unsigned i=0;i<256;i++)f.blobs[5].bytes[i]=255-i;
    NativePolygon p;p.count=4;p.focus={100,100};p.center={248,320};p.viewport={0,128,496,512};p.luma=252;
    p.vertices={NativeVertex{40,-10,4},NativeVertex{60,-10,4},NativeVertex{60,10,4},NativeVertex{40,10,4}};f.polygons={p};
    auto hit=trace_scene_aim({{0,0,0},{.5,0,-4}},f,1,2,true,nullptr,true,1);
    auto enlarged=trace_scene_aim({{0,0,0},{1,0,-4}},f,1,2,true,nullptr,true,2);
    CHECK(hit.target&&enlarged.target&&std::abs(enlarged.target->x-1)<1e-8);
    CHECK(std::abs(hit.native.u-enlarged.native.u)<1e-10&&std::abs(hit.native.v-enlarged.native.v)<1e-10);
    auto leaned=trace_scene_aim({{.25,0,0},{.75,0,-4}},f,1,2,true,nullptr,true,2);
    CHECK(leaned.target&&std::abs(leaned.native.u-hit.native.u)<1e-10);
    auto empty=f;empty.blobs.clear();auto fallback=trace_scene_aim({{0,0,0},{1,0,-4}},empty,1,2,true,nullptr,true,2);
    CHECK(fallback.target&&std::abs(fallback.native.u-hit.native.u)<1e-10);
    GLContext context;GLuint fbo,texture,depth;glGenFramebuffers(1,&fbo);glBindFramebuffer(GL_FRAMEBUFFER,fbo);
    glGenTextures(1,&texture);glBindTexture(GL_TEXTURE_2D,texture);glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,256,256,0,GL_RGBA,GL_UNSIGNED_BYTE,nullptr);glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,texture,0);
    glGenRenderbuffers(1,&depth);glBindRenderbuffer(GL_RENDERBUFFER,depth);glRenderbufferStorage(GL_RENDERBUFFER,GL_DEPTH_COMPONENT24,256,256);glFramebufferRenderbuffer(GL_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,GL_RENDERBUFFER,depth);CHECK(glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE);glViewport(0,0,256,256);
    GLScene scene;scene.set_immersive(true);scene.prepare(std::make_shared<NativeFrame>(f));const auto projection=glm::perspective(glm::radians(90.f),1.f,.02f,10000.f);
    auto draw=[&](float zoom,const glm::mat4& vp){glDepthMask(GL_TRUE);glClearColor(0,0,0,1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);scene.draw(vp,1,2,zoom);std::vector<uint8_t> pixels(256*256*4);glReadPixels(0,0,256,256,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data());return pixels;};
    auto pixel=[](const auto& im,int x,int y){size_t at=(y*256+x)*4;return unsigned(im[at])|unsigned(im[at+1])<<8|unsigned(im[at+2])<<16;};
    auto base=draw(1,projection);auto uploads=scene.upload_stats();auto zoomed=draw(2,projection);
    CHECK(pixel(base,144,128)!=0&&pixel(zoomed,144,128)==0);
    CHECK(pixel(base,160,128)==0&&pixel(zoomed,160,128)!=0);
    CHECK(draw(1,projection)==base&&scene.upload_stats()==uploads);
    auto turned=projection*glm::lookAt(glm::vec3(0),glm::vec3(1,0,-4),glm::vec3(0,1,0));CHECK(pixel(draw(2,turned),128,128)!=0);
    const float deep=float(native_zoom_factor(4.0667696f,34,true));CHECK(deep>8.6f);
    auto deep_view=projection*glm::lookAt(glm::vec3(0),glm::vec3(.5f*deep,0,-4),glm::vec3(0,1,0));
    CHECK(pixel(draw(deep,deep_view),128,128)!=0);
    auto deep_hit=trace_scene_aim({{0,0,0},{.5*deep,0,-4}},f,1,2,true,nullptr,true,deep);
    CHECK(deep_hit.target&&std::abs(deep_hit.native.u-hit.native.u)<1e-10&&std::abs(deep_hit.native.v-hit.native.v)<1e-10);
    // The native HUD occupies an unchanged panel projection while world zooms.
    auto hud=p;hud.window=4;hud.focus={1200,1200};hud.matrix[11]=1200;
    hud.vertices={NativeVertex{-225600,-172800,1200},NativeVertex{-201600,-172800,1200},NativeVertex{-201600,-196800,1200},NativeVertex{-225600,-196800,1200}};
    CHECK(native_hud_polygon(hud));f.polygons.push_back(hud);scene.prepare(std::make_shared<NativeFrame>(f));base=draw(1,projection);zoomed=draw(2,projection);size_t hud_pixels{};
    for(int y=0;y<110;y++)for(int x=0;x<128;x++){CHECK(pixel(base,x,y)==pixel(zoomed,x,y));hud_pixels+=pixel(base,x,y)!=0;}CHECK(hud_pixels>10);
    // An infinite backdrop changes magnification, but never acquires IPD/lean parallax.
    f.polygons={p};for(auto& v:f.polygons[0].vertices)v.x+=1000000;
    f.sky_width=512;f.sky_height=384;f.sky.resize(512*384);
    for(int y=0;y<384;y++)for(int x=0;x<512;x++)f.sky[y*512+x]=0xff000000u|(unsigned(x/2)<<16)|(unsigned(y/2)<<8)|40;
    scene.prepare(std::make_shared<NativeFrame>(f));auto sky=draw(2,projection),sky_base=draw(1,projection);
    CHECK(sky!=sky_base);
    CHECK(draw(2,projection*glm::translate(glm::mat4(1),glm::vec3(-.25,0,0)))==sky);
    CHECK(draw(2,projection*glm::translate(glm::mat4(1),glm::vec3(.25,0,0)))==sky);
    CHECK(glGetError()==GL_NO_ERROR);glDeleteTextures(1,&texture);glDeleteRenderbuffers(1,&depth);glDeleteFramebuffers(1,&fbo);
    std::cout<<"Optional zoom GPU magnification, native aim/fallback, head tracking, HUD, sky and upload invariants passed\n";
}
