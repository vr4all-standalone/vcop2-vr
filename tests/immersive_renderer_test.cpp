// SPDX-License-Identifier: GPL-2.0-or-later
#ifdef NDEBUG
#undef NDEBUG
#endif
#include "model2_scene.h"
#include <cassert>
#include <iostream>
#ifdef VCOP2_IMMERSIVE_GL_TEST
#include "gl_scene.h"
#include "gl_context.h"
#include <glad/gl.h>
#include <glm/gtc/matrix_transform.hpp>
#endif
using namespace vcop2;
static bool close(double a,double b){return std::abs(a-b)<1e-5;}
static NativeFrame fixture(){NativeFrame f;f.width=496;f.height=384;f.yoffset=127;f.lens={496,384,1827,1827,248,191};return f;}
static NativePolygon surface(){NativePolygon p;p.count=4;p.focus={200,300};p.center={248,320};p.viewport={0,128,496,512};p.luma=252;return p;}
int main(){
 auto f=fixture();auto p=surface();
 p.vertices={NativeVertex{400,900,4,0,0},NativeVertex{600,900,4,8,0},NativeVertex{600,1200,4,8,8},NativeVertex{400,1200,4,0,8}};
 auto actual=world_positions(p,f,.5f,2,true);assert(close(actual[0].x,1)&&close(actual[0].y,1.5)&&close(actual[0].z,-2));
 // Focus is perspective information: undoing it restores equal physical dimensions.
 assert(close(actual[1].x-actual[0].x,actual[3].y-actual[0].y));
 f.polygons={p};auto lens=native_camera_lens(f);assert(close(lens.fx,200)&&close(lens.fy,300));
 // A world surface outside the original arcade screen survives for a head turn.
 p.focus={100,100};p.vertices={NativeVertex{500,-40,1,0,0},NativeVertex{500,40,1,8,0},NativeVertex{500,40,2,8,8},NativeVertex{500,-40,2,0,8}};
 f.polygons={p};auto immersive=scene_triangles(f,false,true);assert(immersive.size()==2);assert(scene_triangles(f,false).empty());
 for(const auto& t:immersive)for(const auto& v:t.vertices)assert(finite(v.camera));
 // Geometry behind/crossing the native camera is clipped only after the HMD pose.
 p.vertices={NativeVertex{100,-100,1,0,0},NativeVertex{100,100,1,8,0},NativeVertex{100,100,-1,8,8},NativeVertex{100,-100,-1,0,8}};f.polygons={p};
 immersive=scene_triangles(f,false,true);assert(immersive.size()==2);bool behind=false;
 for(const auto& t:immersive)for(const auto& v:t.vertices){assert(finite(v.camera));behind|=v.camera.z>0;}
 assert(behind);
 // The original native camera maps an actual world position back to cabinet pixels.
 auto aim=native_camera_lens(f).project_point({1,0,-1});assert(aim.valid&&close(aim.u,348./495));
 // Native behind-camera rejection is not a geometric rejection in VR.
 p.culled=true;p.cull_reasons=CullKnown|CullBehind;f.polygons={p};assert(scene_triangles(f,false,true).size()==2);
 p.cull_reasons|=CullBackface;f.polygons={p};assert(scene_triangles(f,false,true).empty());
 f.raster_only=true;assert(scene_triangles(f,false,true).empty());auto flat=native_camera_lens(f);assert(flat.fx<400&&flat.fx>380);f.raster_only=false;
#ifdef VCOP2_IMMERSIVE_GL_TEST
 GLContext context;GLuint fbo,color,depth;glGenFramebuffers(1,&fbo);glBindFramebuffer(GL_FRAMEBUFFER,fbo);
 glGenTextures(1,&color);glBindTexture(GL_TEXTURE_2D,color);glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,256,256,0,GL_RGBA,GL_UNSIGNED_BYTE,nullptr);glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,color,0);
 glGenRenderbuffers(1,&depth);glBindRenderbuffer(GL_RENDERBUFFER,depth);glRenderbufferStorage(GL_RENDERBUFFER,GL_DEPTH_COMPONENT24,256,256);glFramebufferRenderbuffer(GL_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,GL_RENDERBUFFER,depth);
 assert(glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE);glViewport(0,0,256,256);
 for(const auto& named: {std::pair<const char*,size_t>{"texture0",2097152},{"texture1",2097152},{"palette",16384},{"colorxlat",49152},{"luma",32768},{"gamma",256}})f.blobs.push_back({named.first,std::vector<uint8_t>(named.second)});
 unsigned base=5|(10<<5)|(15<<10);f.blobs[2].bytes[8192]=base;f.blobs[2].bytes[8193]=base>>8;
 for(unsigned c=0;c<3;c++)for(unsigned component=0;component<32;component++)for(unsigned luma=0;luma<64;luma++)f.blobs[3].bytes[2*(c*8192+component*256+luma)]=component*4+luma;
 for(unsigned i=0;i<256;i++)f.blobs[5].bytes[i]=255-i;
 f.background.assign(496*384,0xff000000);f.native=f.background;
 auto view=glm::perspective(glm::radians(90.f),1.f,.02f,1000.f);
 auto clear=[&](){glDepthMask(GL_TRUE);glClearColor(0,0,0,1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);};
 auto pixel_at=[&](int x,int y){uint8_t px[4]{};glReadPixels(x,y,1,1,GL_RGBA,GL_UNSIGNED_BYTE,px);return 0xff000000u|(unsigned(px[0])<<16)|(unsigned(px[1])<<8)|px[2];};
 auto pixel=[&](){return pixel_at(128,128);};
 GLScene scene;
 // A source-camera offscreen wall really appears when the headset turns right.
 p=surface();p.focus={100,100};p.vertices={NativeVertex{1500,-100,4,0,0},NativeVertex{1500,100,4,8,0},NativeVertex{1500,100,6,8,8},NativeVertex{1500,-100,6,0,8}};f.polygons={p};
 auto image=std::make_shared<NativeFrame>(f);scene.prepare(image);clear();auto turned=view*glm::lookAt(glm::vec3(0),glm::vec3(15,0,-5),glm::vec3(0,1,0));scene.draw(turned,1,2);assert(pixel()==0xff000000u);
 scene.set_immersive(true);clear();scene.draw(turned,1,2);assert(pixel()==0xffac9884u);
 auto offscreen_aim=trace_scene_aim({{0,0,0},{15,0,-5}},f,1,2,false,nullptr,true);assert(offscreen_aim.target&&close(offscreen_aim.target->x,15));
 // Looking sideways also retains the portion behind the original camera.
 p.vertices={NativeVertex{100,-100,-2,0,0},NativeVertex{100,100,-2,8,0},NativeVertex{100,100,-4,8,8},NativeVertex{100,-100,-4,0,8}};p.culled=true;p.cull_reasons=CullKnown|CullBehind;f.polygons={p};image=std::make_shared<NativeFrame>(f);scene.prepare(image);clear();scene.draw(view*glm::lookAt(glm::vec3(0),glm::vec3(1,0,3),glm::vec3(0,1,0)),1,2);assert(pixel()==0xffac9884u);
 // Background pixels at 2m cannot depth-occlude a real wall ten metres away.
 p=surface();p.focus={100,100};p.vertices={NativeVertex{-100,100,10,0,0},NativeVertex{100,100,10,8,0},NativeVertex{100,-100,10,8,8},NativeVertex{-100,-100,10,0,8}};f.polygons={p};image=std::make_shared<NativeFrame>(f);scene.prepare(image);clear();scene.draw(view,1,2);assert(pixel()==0xffac9884u);assert(pixel_at(111,128)==0xff000000u&&pixel_at(137,128)==0xffac9884u);
 // A lean moves that surface by its real ten-metre depth, not the HUD distance.
 clear();scene.draw(view*glm::translate(glm::mat4(1),glm::vec3(-.5f,0,0)),1,2);assert(pixel_at(111,128)==0xffac9884u&&pixel_at(137,128)==0xff000000u);auto leaned=trace_scene_aim({{.5,0,0},{0,0,-1}},f,1,2,false,nullptr,true);assert(leaned.target&&close(leaned.target->z,-10)&&close(leaned.native.u,(248+5.)/495));
 assert(glGetError()==GL_NO_ERROR);glDeleteFramebuffers(1,&fbo);glDeleteTextures(1,&color);glDeleteRenderbuffers(1,&depth);
 std::cout<<"Immersive GPU head-turn, behind-camera, background-depth and lean checks passed\n";
#endif
 std::cout<<"Immersive camera shape, offscreen head-turn and signed-depth checks passed\n";
}
