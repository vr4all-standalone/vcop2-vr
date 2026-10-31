// SPDX-License-Identifier: GPL-2.0-or-later
#ifdef NDEBUG
#undef NDEBUG // These executable regression checks must also run in Release.
#endif
#include "model2_scene.h"
#include <cassert>
#include <iostream>
#ifdef VCOP2_RENDERER_GL_TEST
#include "gl_scene.h"
#include "gl_context.h"
#include <glad/gl.h>
#include <glm/gtc/matrix_transform.hpp>
#endif

using namespace vcop2;
static bool close(double a,double b){return std::abs(a-b)<1e-5;}
static void resources(NativeFrame& f) {
    f.blobs.clear();
    f.blobs.push_back({"texture0",std::vector<uint8_t>(2097152)});
    f.blobs.push_back({"texture1",std::vector<uint8_t>(2097152)});
    f.blobs.push_back({"palette",std::vector<uint8_t>(16384)});
    f.blobs.push_back({"colorxlat",std::vector<uint8_t>(49152)});
    f.blobs.push_back({"luma",std::vector<uint8_t>(32768)});
    f.blobs.push_back({"gamma",std::vector<uint8_t>(256)});
    unsigned base=5|(10<<5)|(15<<10);f.blobs[2].bytes[8192]=base;f.blobs[2].bytes[8193]=base>>8;
    for(unsigned c=0;c<3;c++)for(unsigned component=0;component<32;component++)for(unsigned luma=0;luma<64;luma++)
        f.blobs[3].bytes[2*(c*8192+component*256+luma)]=component*4+luma;
    for(unsigned i=0;i<32768;i++)f.blobs[4].bytes[i]=(i&127)/2;
    for(unsigned i=0;i<256;i++)f.blobs[5].bytes[i]=255-i;
}
static NativeFrame frame() {
    NativeFrame f;f.width=496;f.height=384;f.yoffset=127;
    f.lens={496,384,1827.30969,1827.30969,248,191};f.complete=true;resources(f);return f;
}
static NativePolygon quad(float depth=100) {
    NativePolygon p;p.count=4;p.focus={1200,1200};p.center={248,320};p.viewport={0,128,496,512};
    p.header={0,0,0,0};p.luma=252;
    p.vertices={NativeVertex{-10000,10000,depth,0,0},NativeVertex{10000,10000,depth,8,0},
        NativeVertex{10000,-10000,depth,8,8},NativeVertex{-10000,-10000,depth,0,8}};return p;
}
int main() {
    auto f=frame();auto p=quad();
    // A focus/window change must preserve its native pixels under one common VR lens.
    auto vertices=world_positions(p,f,.01f,2.f);
    auto a=presentation_lens(f).project_point(vertices[0]);
    assert(close(a.u,148./495)&&close(a.v,91./383));
    assert(close(vertices[0].z,-1));
    f.polygons={p};
    auto hit=trace_scene_aim({{0,0,0},{0,0,-1}},f,.01f,2.f);
    assert(hit.target&&close(hit.target->z,-1));
    assert(hit.native.valid&&hit.native.inside&&close(hit.native.u,248./495));
    // Shared triangle clipping cannot show or aim through a native viewport border.
    f.polygons[0].viewport={248,128,496,512};
    auto clipped=scene_triangles(f,false);assert(!clipped.empty());
    for(const auto& t:clipped)for(const auto& v:t.vertices)assert(v.pixel_x>=248-1e-5);
    for(const auto& t:scene_triangles(f,true))for(const auto& v:t.vertices)assert(v.pixel_x>=248-1e-5);
    f.polygons[0].culled=true;assert(scene_triangles(f,true).empty());
    // Offscreen native geometry becomes visible only when the requested wider view is enabled.
    f.polygons={p};f.polygons[0].vertices[0].x=-100000;
    auto normal=scene_triangles(f,false),wide=scene_triangles(f,true);
    bool outside=false;for(const auto& t:wide)for(const auto& v:t.vertices)outside|=v.pixel_x<0;
    assert(outside);for(const auto& t:normal)for(const auto& v:t.vertices)assert(v.pixel_x>=0);
    // Native override order, rather than a conventional physical Z buffer, selects the visible surface.
    auto near=quad(50),far=quad(150);near.sort=10;far.sort=20;far.window=2;
    f.polygons={near,far};hit=trace_scene_aim({{0,0,0},{0,0,-1}},f,.01f,2.f);
    assert(hit.target&&close(hit.target->z,-1.5));
    // Screen-space direct polygons remain at the selected panel distance.
    p.focus={1,1};f.polygons={p};vertices=world_positions(p,f,.01f,3.f);
    for(auto v:vertices)assert(close(v.z,-3));
    // No geometry, including boot/service screens, still has a usable native aiming panel.
    f.polygons.clear();hit=trace_scene_aim({{0,0,0},{0,0,-1}},f,.01f,2.f);
    assert(hit.target&&close(hit.target->z,-2)&&hit.native.valid);
    // Model 2 packs four nibble texels per little-endian word in 2x2 blocks.
    f.blobs[0].bytes[0]=0x34;f.blobs[0].bytes[1]=0x12;Model2Resources material(f);
    assert(material.texel(0,0,0,0,0)==1);assert(material.texel(0,0,0,1,0)==2);
    assert(material.texel(0,0,0,0,1)==3);assert(material.texel(0,0,0,1,1)==4);
    // Transparent solid polygons do not hide world geometry or consume an aim hit.
    p=quad();p.header[0]=0x2000;f.polygons={p};
    assert(scene_triangles(f,false).empty());
    // These literals independently exercise palette -> color translator -> gamma.
    p.header[0]=0;assert(material.sample(p,0,0,100,248,191)==0xffac9884u);
    {
        // A retained material object follows its own wrapper after COW detach,
        // while a separately held frame must keep its original resource bytes.
        auto mutable_frame=frame(),old_snapshot=mutable_frame;
        Model2Resources retained(mutable_frame),old_material(old_snapshot);auto solid=quad();
        auto original=retained.sample(solid,0,0,100,248,191);assert(original);
        for(auto& component:mutable_frame.blobs[5].bytes)component=255-component;
        assert(retained.sample(solid,0,0,100,248,191)!=original);
        assert(old_material.sample(solid,0,0,100,248,191)==original);
    }
    std::fill(f.blobs[0].bytes.begin(),f.blobs[0].bytes.end(),0x22);
    std::fill(f.blobs[1].bytes.begin(),f.blobs[1].bytes.end(),0x22);
    p.header[0]=0x4000;p.lod=10000;
    assert(material.sample(p,1,1,100,248,191)==0xffe4d0bcu);
    p.header[0]=0xc000;assert(!material.sample(p,1,1,100,249,191));
    // Alpha key 15 must expose the panel, not hide it with a colored polygon.
    std::fill(f.blobs[0].bytes.begin(),f.blobs[0].bytes.end(),0xff);
    std::fill(f.blobs[1].bytes.begin(),f.blobs[1].bytes.end(),0xff);
    p.header[0]=0x6000;f.polygons={p};assert(!material.sample(p,1,1,100,248,191));
    hit=trace_scene_aim({{0,0,0},{0,0,-1}},f,.01f,2.f);assert(hit.target&&close(hit.target->z,-2));
    // A comfortable display FOV changes presentation, while its aim still maps
    // to the original native pixel. Native camera focus is not relabeled.
    f=frame();f.polygons={quad()};auto display_lens=f.lens;display_lens.fx=display_lens.fy=390;
    hit=trace_scene_aim({{.032,0,0},{0,0,-1}},f,.01f,2.f,false,&display_lens);
    assert(hit.target&&close(hit.target->z,-1)&&close(hit.native.u,(248+390*.032)/495));
    // Checker coverage uses floor, including negative native coordinates exposed
    // by wider view. Truncating -.5 to zero would select the opposite pixel.
    p=quad();p.header[0]=0x8000;for(unsigned i=0;i<4;i++)p.vertices[i].x*=10;
    f.polygons={p};hit=trace_scene_aim({{0,0,0},{(-.5-248)/f.lens.fx,0,-1}},f,.01f,2.f,true);
    assert(hit.target&&close(hit.target->z,-2));
    // Incomplete frames are never shown and must not produce an actionable aim.
    f.complete=false;assert(!trace_scene_aim({{0,0,0},{0,0,-1}},f,.01f,2.f).target);
    // A missing resource causes the renderer's native panel fallback; aim follows it.
    f.complete=true;f.blobs.clear();f.polygons={quad()};
    hit=trace_scene_aim({{0,0,0},{0,0,-1}},f,.01f,2.f);assert(hit.target&&close(hit.target->z,-2));
#ifdef VCOP2_RENDERER_GL_TEST
    // Real GPU color checks: no raster or polygon material is mocked here.
    GLContext context;f=frame();f.polygons={quad()};f.background.assign(size_t(f.width)*f.height,0xff000000);
    GLuint framebuffer,color,depth;glGenFramebuffers(1,&framebuffer);glBindFramebuffer(GL_FRAMEBUFFER,framebuffer);
    glGenTextures(1,&color);glBindTexture(GL_TEXTURE_2D,color);glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,f.width,f.height,0,GL_RGBA,GL_UNSIGNED_BYTE,nullptr);
    glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,color,0);
    glGenRenderbuffers(1,&depth);glBindRenderbuffer(GL_RENDERBUFFER,depth);glRenderbufferStorage(GL_RENDERBUFFER,GL_DEPTH_COMPONENT24,f.width,f.height);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,GL_RENDERBUFFER,depth);assert(glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE);
    glViewport(0,0,f.width,f.height);auto l=f.lens;const float n=.001f;
    auto projection=glm::frustum(float(-l.cx/l.fx*n),float((f.width-l.cx)/l.fx*n),float(-(f.height-l.cy)/l.fy*n),float(l.cy/l.fy*n),n,100.f);
    auto pixel=[&](int x,int y){uint8_t p[4];glReadPixels(x,f.height-1-y,1,1,GL_RGBA,GL_UNSIGNED_BYTE,p);return 0xff000000u|(unsigned(p[0])<<16)|(unsigned(p[1])<<8)|p[2];};
    {GLScene scene;auto image=std::make_shared<NativeFrame>(f);scene.prepare(image);scene.draw(projection,.01f,2.f);assert(pixel(248,191)==0xffac9884u);
        for(auto& blob:f.blobs)if(blob.name=="texture0"||blob.name=="texture1")std::fill(blob.bytes.begin(),blob.bytes.end(),0x22);
        f.polygons[0].header[0]=0x4000;f.polygons[0].lod=10000;image=std::make_shared<NativeFrame>(f);scene.prepare(image);scene.draw(projection,.01f,2.f);assert(pixel(248,191)==0xffe4d0bcu);
        f.polygons[0].header[0]=0xc000;image=std::make_shared<NativeFrame>(f);scene.prepare(image);scene.draw(projection,.01f,2.f);
        assert(pixel(249,191)==0xff000000u);assert(pixel(248,191)==0xffe4d0bcu);
        // Moving the eye shifts the same real surface and its ray hit together.
        f.polygons[0].header[0]=0x4000;image=std::make_shared<NativeFrame>(f);scene.prepare(image);
        glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);scene.draw(projection*glm::translate(glm::mat4(1),glm::vec3(-.032f,0,0)),.01f,2.f);
        assert(pixel(88,191)==0xff000000u);assert(pixel(90,191)==0xffe4d0bcu);assert(pixel(291,191)==0xff000000u);
        auto aim=trace_scene_aim({{.032,0,0},{0,0,-1}},f,.01f,2.f);assert(aim.target&&close(aim.target->z,-1));
        assert(close(aim.native.u,(248+1827.30969*.032)/495));
        scene.set_presentation_lens(display_lens);
        auto display_projection=glm::frustum(float(-display_lens.cx/display_lens.fx*n),float((f.width-display_lens.cx)/display_lens.fx*n),float(-(f.height-display_lens.cy)/display_lens.fy*n),float(display_lens.cy/display_lens.fy*n),n,100.f);
        scene.draw(display_projection,.01f,2.f);assert(pixel(248,191)==0xffe4d0bcu);
        glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);scene.draw(display_projection*glm::translate(glm::mat4(1),glm::vec3(-.032f,0,0)),.01f,2.f);
        // fx390 at1m gives12.48pixels ofparallax, not58px orpanel6.24px.
        assert(pixel(134,191)==0xff000000u);assert(pixel(137,191)==0xffe4d0bcu);assert(pixel(337,191)==0xff000000u);
        scene.set_presentation_lens({});
        // Adjacent triangles carry independent flat integer material data. A
        // single ordered batch must not bleed headers, luma, LOD or panel depth
        // between primitives, even when native priority overrides physical Z.
        auto rectangle=[](float x0,float x1,float z){
            auto polygon=quad(z);
            polygon.vertices={NativeVertex{(x0-248)*z,7100,z,8,8},NativeVertex{(x1-248)*z,7100,z,8,8},
                NativeVertex{(x1-248)*z,-2900,z,8,8},NativeVertex{(x0-248)*z,-2900,z,8,8}};
            return polygon;
        };
        auto solid=rectangle(128,188,100),textured=rectangle(188,248,100),mipped=rectangle(248,308,100),screen=rectangle(308,368,100);
        textured.header[0]=mipped.header[0]=0x4000;textured.lod=10000;mipped.lod=1572;
        screen.focus={1,1};screen.luma=128;
        std::fill(f.blobs[1].bytes.begin(),f.blobs[1].bytes.end(),0x88);
        f.polygons={solid,textured,mipped,screen};image=std::make_shared<NativeFrame>(f);scene.prepare(image);
        glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);scene.draw(projection,.01f,2.f);
        assert(pixel(158,191)==0xffac9884u);assert(pixel(218,191)==0xffe4d0bcu);
        assert(pixel(278,191)==0xffccb8a4u);assert(pixel(338,191)==0xffcbb7a3u);
        float world_depth{},panel_depth{};glReadPixels(158,f.height-1-191,1,1,GL_DEPTH_COMPONENT,GL_FLOAT,&world_depth);
        glReadPixels(338,f.height-1-191,1,1,GL_DEPTH_COMPONENT,GL_FLOAT,&panel_depth);assert(panel_depth-world_depth>.0004f);
        auto override=rectangle(198,238,150);override.window=8;override.luma=128;override.sort=60000;
        f.polygons.push_back(override);image=std::make_shared<NativeFrame>(f);scene.prepare(image);scene.draw(projection,.01f,2.f);
        assert(pixel(218,191)==0xffcbb7a3u);assert(pixel(278,191)==0xffccb8a4u);
        // Missing material resources use the exact native diagnostic raster.
        f.blobs.clear();f.native.assign(size_t(f.width)*f.height,0xff123456);image=std::make_shared<NativeFrame>(f);scene.prepare(image);scene.draw(projection,.01f,2.f);assert(pixel(248,191)==0xff123456);
    }
    assert(glGetError()==GL_NO_ERROR);glDeleteFramebuffers(1,&framebuffer);glDeleteTextures(1,&color);glDeleteRenderbuffers(1,&depth);
    std::cout<<"Model 2 production OpenGL material/stereo/fallback tests passed\n";
#endif
    std::cout<<"Model 2 renderer geometry/aim/texture tests passed\n";
}
