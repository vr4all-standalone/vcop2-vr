// SPDX-License-Identifier: GPL-2.0-or-later
#ifdef NDEBUG
#undef NDEBUG
#endif
#include "gl_scene.h"
#include "gl_context.h"
#include <glad/gl.h>
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cassert>
#include <fstream>
#include <iostream>
#include <numeric>
using namespace vcop2;

static std::shared_ptr<NativeFrame> fixture() {
    auto f=std::make_shared<NativeFrame>();f->width=64;f->height=32;
    NativePolygon p;p.count=4;p.focus={20,20};p.center={32,368};p.viewport={0,352,64,384};
    p.header={0x4000,0,0,0};p.luma=252;p.lod=10000;
    p.vertices={NativeVertex{-2000,2000,100,0,0},NativeVertex{2000,2000,100,8,0},NativeVertex{2000,-2000,100,8,8},NativeVertex{-2000,-2000,100,0,8}};f->polygons={p};
    for(const auto& named:{std::pair<const char*,size_t>{"texture0",2097152},{"texture1",2097152},{"palette",16384},{"colorxlat",49152},{"luma",32768},{"gamma",256}})f->blobs.push_back({named.first,std::vector<uint8_t>(named.second)});
    unsigned base=5|(10<<5)|(15<<10);f->blobs[2].bytes[8192]=base;f->blobs[2].bytes[8193]=base>>8;
    for(unsigned c=0;c<3;c++)for(unsigned component=0;component<32;component++)for(unsigned luma=0;luma<64;luma++)f->blobs[3].bytes[2*(c*8192+component*256+luma)]=component*4+luma;
    for(unsigned n=0;n<32768;n++)f->blobs[4].bytes[n]=(n&127)/2;
    for(unsigned n=0;n<256;n++)f->blobs[5].bytes[n]=255-n;
    f->background.assign(64*32,0xff123456);f->foreground.assign(64*32,0);f->native.assign(64*32,0xff654321);
    return f;
}
int main(int argc,char** argv) {
    GLContext context;constexpr int width=128,height=128;GLuint fbo,color,depth;
    glGenFramebuffers(1,&fbo);glBindFramebuffer(GL_FRAMEBUFFER,fbo);glGenTextures(1,&color);glBindTexture(GL_TEXTURE_2D,color);
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,width,height,0,GL_RGBA,GL_UNSIGNED_BYTE,nullptr);glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,color,0);
    glGenRenderbuffers(1,&depth);glBindRenderbuffer(GL_RENDERBUFFER,depth);glRenderbufferStorage(GL_RENDERBUFFER,GL_DEPTH_COMPONENT24,width,height);glFramebufferRenderbuffer(GL_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,GL_RENDERBUFFER,depth);
    assert(glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE);glViewport(0,0,width,height);
    auto projection=glm::infinitePerspective(glm::radians(100.f),1.f,.02f);
    auto render=[&](GLScene& scene,float yaw=0.f,float pitch=0.f){
        glDepthMask(GL_TRUE);glClearColor(0,0,0,1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
        auto direction=glm::vec3(std::sin(glm::radians(yaw))*std::cos(glm::radians(pitch)),std::sin(glm::radians(pitch)),-std::cos(glm::radians(yaw))*std::cos(glm::radians(pitch)));
        scene.draw(projection*glm::lookAt(glm::vec3(0),direction,glm::vec3(0,1,0)),1,2);
        std::vector<uint8_t> out(width*height*4);glReadPixels(0,0,width,height,GL_RGBA,GL_UNSIGNED_BYTE,out.data());return out;
    };
    GLScene scene;scene.set_immersive(true);auto f=fixture();scene.prepare(f);auto initial=render(scene);auto stats=scene.upload_stats();
    assert(std::accumulate(stats.materials.begin(),stats.materials.end(),uint64_t{})==6);
    assert(stats.rasters[0]==0&&stats.rasters[1]==1&&stats.rasters[2]==0&&stats.sky==1);
    auto verify_fresh=[&]{GLScene fresh;fresh.set_immersive(true);fresh.prepare(f);assert(render(scene)==render(fresh));};
    // Native frames own distinct vectors even when their contents did not change.
    f=std::make_shared<NativeFrame>(*f);f->sequence++;scene.prepare(f);assert(scene.upload_stats()==stats);assert(render(scene)==initial);
    // Every shader resource is independently invalidated by actual byte changes.
    for(int slot:{2,0,1,3,4,5}) {
        auto before=render(scene);f=std::make_shared<NativeFrame>(*f);f->sequence++;
        auto& bytes=f->blobs[slot].bytes;
        if(slot==2)bytes[8192]^=31;
        else if(slot<2)std::fill(bytes.begin(),bytes.end(),slot==0?0x22:0x88);
        else if(slot==3||slot==4)for(auto& b:bytes)b^=3;
        else for(auto& b:bytes)b=255-b;
        scene.prepare(f);auto changed=scene.upload_stats();auto expected=stats;expected.materials[slot]++;
        assert(changed.materials==expected.materials&&changed.rasters==stats.rasters&&changed.sky==stats.sky);
        if(slot!=1)assert(render(scene)!=before); // Sheet1 is not sampled at level0.
        verify_fresh();stats=changed;
    }
    // The other texture sheet remains current when the game selects it later.
    auto sheet0=render(scene);f=std::make_shared<NativeFrame>(*f);f->polygons[0].header[2]^=0x1000;
    scene.prepare(f);assert(scene.upload_stats()==stats);assert(render(scene)!=sheet0);verify_fresh();
    f=std::make_shared<NativeFrame>(*f);std::fill(f->foreground.begin(),f->foreground.end(),0xff284669);scene.prepare(f);
    auto foreground=scene.upload_stats();assert(foreground.materials==stats.materials&&foreground.sky==stats.sky&&foreground.rasters[1]==stats.rasters[1]+1);verify_fresh();
    f=std::make_shared<NativeFrame>(*f);std::fill(f->foreground.begin(),f->foreground.end(),0);scene.prepare(f);stats=scene.upload_stats();
    // Raster-only service/boot screens upload the native image, not world assets.
    auto game=f;f=std::make_shared<NativeFrame>(*f);f->raster_only=true;scene.prepare(f);auto fallback=scene.upload_stats();
    assert(fallback.materials==stats.materials&&fallback.sky==stats.sky&&fallback.rasters[2]==1);
    assert(render(scene)!=initial);f=std::make_shared<NativeFrame>(*f);scene.prepare(f);assert(scene.upload_stats()==fallback);
    f=std::make_shared<NativeFrame>(*game);scene.prepare(f);verify_fresh();
    // Switching between reference and immersive modes repopulates only used layers.
    auto before_mode=scene.upload_stats();scene.set_immersive(false);auto reference=scene.upload_stats();
    assert(reference.materials==before_mode.materials&&reference.rasters[0]==1&&reference.rasters[2]==before_mode.rasters[2]);
    scene.set_immersive(true);verify_fresh();
    // Full512 capture and the original496 visible crop must render identically.
    f=std::make_shared<NativeFrame>(*f);f->width=496;f->height=384;f->yoffset=127;
    f->polygons[0].focus={173.651f,173.651f};f->polygons[0].center={248,320};f->polygons[0].viewport={0,128,496,512};f->polygons[0].header[0]=0x6000;
    std::fill(f->blobs[0].bytes.begin(),f->blobs[0].bytes.end(),255);std::fill(f->blobs[1].bytes.begin(),f->blobs[1].bytes.end(),255);
    f->sky_width=512;f->sky_height=384;f->sky.resize(512*384);
    if(argc>1) {
        std::ifstream in(argv[1],std::ios::binary);std::string format;int w{},h{},max{};in>>format>>w>>h>>max;in.get();assert(format=="P6"&&w==512&&h==384&&max==255);
        for(auto& pixel:f->sky){unsigned char rgb[3];in.read(reinterpret_cast<char*>(rgb),3);assert(in);pixel=0xff000000u|(unsigned(rgb[0])<<16)|(unsigned(rgb[1])<<8)|rgb[2];}
    } else for(int y=0;y<384;y++)for(int x=0;x<512;x++)f->sky[y*512+x]=0xff000000u|((x*13%256)<<16)|((y*7%256)<<8)|((x+y)%256);
    f->background.resize(496*384);for(int y=0;y<384;y++)std::copy_n(f->sky.begin()+y*512,496,f->background.begin()+y*496);
    f->foreground.clear();f->native=f->background;scene.prepare(f);
    std::vector<std::vector<uint8_t>> full;
    for(float yaw:{0.f,45.f,90.f,135.f,180.f,-135.f,-90.f,-45.f})for(float pitch:{-80.f,-45.f,0.f,45.f,80.f})full.push_back(render(scene,yaw,pitch));
    f=std::make_shared<NativeFrame>(*f);f->sky.clear();scene.prepare(f);size_t n{};
    for(float yaw:{0.f,45.f,90.f,135.f,180.f,-135.f,-90.f,-45.f})for(float pitch:{-80.f,-45.f,0.f,45.f,80.f}) {
        auto cropped=render(scene,yaw,pitch);const auto& original=full[n++];
        if(cropped!=original){size_t different{};for(size_t p=0;p<cropped.size();p+=4)if(!std::equal(cropped.begin()+p,cropped.begin()+p+4,original.begin()+p))different++;std::cerr<<"Sky width mismatch yaw="<<yaw<<" pitch="<<pitch<<" pixels="<<different<<"\n";}
        assert(cropped==original);
    }
    assert(glGetError()==GL_NO_ERROR);glDeleteFramebuffers(1,&fbo);glDeleteTextures(1,&color);glDeleteRenderbuffers(1,&depth);
    std::cout<<"Exact upload caching, all6 material invalidations, fallback/mode transitions and40 full/cropped sky views passed\n";
}
