// SPDX-License-Identifier: GPL-2.0-or-later
#ifdef NDEBUG
#undef NDEBUG
#endif
#include "gl_scene.h"
#include "gl_context.h"
#include <glad/gl.h>
#include <glm/gtc/matrix_transform.hpp>
#include <cassert>
#include <iostream>
using namespace vcop2;
int main(){
 GLContext context;constexpr int width=128,height=128;GLuint fbo,color,depth;glGenFramebuffers(1,&fbo);glBindFramebuffer(GL_FRAMEBUFFER,fbo);
 glGenTextures(1,&color);glBindTexture(GL_TEXTURE_2D,color);glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,width,height,0,GL_RGBA,GL_UNSIGNED_BYTE,nullptr);glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,color,0);
 glGenRenderbuffers(1,&depth);glBindRenderbuffer(GL_RENDERBUFFER,depth);glRenderbufferStorage(GL_RENDERBUFFER,GL_DEPTH_COMPONENT24,width,height);glFramebufferRenderbuffer(GL_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,GL_RENDERBUFFER,depth);assert(glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE);glViewport(0,0,width,height);
 auto frame=std::make_shared<NativeFrame>();frame->width=64;frame->height=32;frame->yoffset=0;
 NativePolygon polygon;polygon.count=4;polygon.focus={20,20};polygon.center={32,368};polygon.viewport={0,352,64,384};polygon.header={0x6000,0,0,0};polygon.luma=252;polygon.lod=10000;
 polygon.vertices={NativeVertex{-20,20,10,0,0},NativeVertex{20,20,10,8,0},NativeVertex{20,-20,10,8,8},NativeVertex{-20,-20,10,0,8}};frame->polygons={polygon};
 for(const auto& named:{std::pair<const char*,size_t>{"texture0",2097152},{"texture1",2097152},{"palette",16384},{"colorxlat",49152},{"luma",32768},{"gamma",256}})frame->blobs.push_back({named.first,std::vector<uint8_t>(named.second)});
 std::fill(frame->blobs[0].bytes.begin(),frame->blobs[0].bytes.end(),255);std::fill(frame->blobs[1].bytes.begin(),frame->blobs[1].bytes.end(),255);
 for(unsigned y=0;y<32;y++)for(unsigned x=0;x<64;x++)frame->background.push_back(0xff000000u|((20+(x*3)%200)<<16)|((20+(y*5)%150)<<8)|100);
 frame->native=frame->background;
 GLScene scene;scene.set_immersive(true);scene.prepare(frame);
 auto projection=glm::perspective(glm::radians(100.f),1.f,.02f,1000.f);
 auto render=[&](float x,float yaw,float pitch=0.f){glDepthMask(GL_TRUE);glClearColor(0,0,0,1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);auto eye=glm::vec3(x,0,0);auto direction=glm::vec3(std::sin(glm::radians(yaw))*std::cos(glm::radians(pitch)),std::sin(glm::radians(pitch)),-std::cos(glm::radians(yaw))*std::cos(glm::radians(pitch)));scene.draw(projection*glm::lookAt(eye,eye+direction,glm::vec3(0,1,0)),1,2);std::vector<uint8_t> result(width*height*4);glReadPixels(0,0,width,height,GL_RGBA,GL_UNSIGNED_BYTE,result.data());return result;};
 auto centre=render(0,0),left=render(-.25f,0),right=render(.25f,0);
 assert(centre==left&&centre==right); // An infinite sky cannot move under leaning/IPD.
 auto turned=render(0,80);assert(turned!=centre);
 for(float yaw:{0.f,80.f,150.f,180.f,-150.f,-80.f}){auto image=render(0,yaw);for(size_t i=0;i<image.size();i+=4)assert(image[i]||image[i+1]||image[i+2]);}
 // The sky mirrors only the original visible window, excluding the page break.
 frame=std::make_shared<NativeFrame>(*frame);frame->sequence++;frame->sky_width=128;frame->sky_height=32;
 for(unsigned y=0;y<32;y++)for(unsigned x=0;x<128;x++){auto pixel=frame->background[y*64+(x%64)];if(x>=64)pixel=(pixel&0xffffff00u)|200u;frame->sky.push_back(pixel);}
 scene.prepare(frame);auto period_front=render(0,0),period_back=render(0,180);const size_t middle=(height/2*width+width/2)*4;
 assert(period_front[middle+2]==100&&period_back[middle+2]==100);
 for(float yaw:{0.f,80.f,150.f,180.f,-150.f,-80.f}){auto crop=render(0,yaw);for(size_t i=2;i<crop.size();i+=4)assert(crop[i]==100);}
 // Both reflected image edges are continuous despite a non-seamless source.
 const float span=63.f,mirror_period=2*span;
 const float bias=glm::degrees(std::atan(std::tan(glm::radians(50.f))/width));
 for(float edge:{0.f,span}) {
  float yaw=glm::degrees((edge-32)*6.283185307179586f/mirror_period)-bias;
  auto before=render(0,yaw-.01f),after=render(0,yaw+.01f);
  for(int c=0;c<3;c++)assert(std::abs(int(before[middle+c])-int(after[middle+c]))<=3);
 }
 // The native cloud band ends at its original camera angle, not the zenith.
 auto high=render(0,0,60);assert(high[middle+1]==20); // Original top source row.
 // OpenXR uses an infinite far plane; it must cancel translations identically.
 projection=glm::infinitePerspective(glm::radians(100.f),1.f,.02f);
 auto infinity=render(0,0);assert(infinity==render(-.25f,0)&&infinity==render(.25f,0));
 auto rear_left=render(0,179.999f),rear_right=render(0,-179.999f);assert(rear_left[middle+2]==rear_right[middle+2]);
 // Opaque world geometry still wins over the infinite backdrop.
 frame=std::make_shared<NativeFrame>(*frame);frame->sequence++;frame->polygons[0].header[0]=0;
 unsigned base=5|(10<<5)|(15<<10);frame->blobs[2].bytes[8192]=base;frame->blobs[2].bytes[8193]=base>>8;
 for(unsigned c=0;c<3;c++)for(unsigned component=0;component<32;component++)for(unsigned luma=0;luma<64;luma++)frame->blobs[3].bytes[2*(c*8192+component*256+luma)]=component*4+luma;
 for(unsigned i=0;i<256;i++)frame->blobs[5].bytes[i]=255-i;
 scene.prepare(frame);auto world=render(0,0);const size_t at=(height/2*width+width/2)*4;assert(world[at]==0xac&&world[at+1]==0x98&&world[at+2]==0x84);
 // Actual native strip dimensions and camera centre survive capture widening.
 frame=std::make_shared<NativeFrame>(*frame);frame->width=496;frame->height=384;frame->yoffset=127;
 frame->polygons[0].focus={173.651f,173.651f};frame->polygons[0].center={248,320};frame->polygons[0].viewport={0,128,496,512};frame->polygons[0].header[0]=0x6000;
 frame->background.assign(496*384,0xff283264);frame->native=frame->background;frame->sky_width=512;frame->sky_height=384;frame->sky.clear();
 for(unsigned y=0;y<384;y++)for(unsigned x=0;x<512;x++)frame->sky.push_back(0xff283200u|((x>=240&&x<=256)?210u:100u));
 scene.prepare(frame);auto actual_centre=render(0,0);assert(actual_centre[middle+2]==210);
 auto actual_left=render(-.25f,0),actual_right=render(.25f,0);assert(actual_centre==actual_left&&actual_centre==actual_right);
 auto actual_rear_left=render(0,179.999f),actual_rear_right=render(0,-179.999f);assert(actual_rear_left[middle+2]==actual_rear_right[middle+2]);
 assert(glGetError()==GL_NO_ERROR);glDeleteFramebuffers(1,&fbo);glDeleteTextures(1,&color);glDeleteRenderbuffers(1,&depth);
 std::cout<<"Infinite native sky is translation invariant, rotates with head view, covers360 degrees and stays behind world geometry\n";
}
