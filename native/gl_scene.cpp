// SPDX-License-Identifier: GPL-2.0-or-later
#include "gl_scene.h"
#include "model2_scene.h"
#include "shaders/model2_shaders.h"
#include <glad/gl.h>
#include <glm/gtc/type_ptr.hpp>
#include <cstddef>
#include <algorithm>
#include <utility>
#include <stdexcept>
#include <string>

namespace vcop2 {
static GLuint compile(GLenum type,const char* source) {
    std::string code=source;
#ifdef __ANDROID__
    code.replace(0,code.find('\n'),"#version 300 es\nprecision highp float;\nprecision highp int;\nprecision highp usampler2D;\nprecision highp sampler2D;");
#endif
    auto shader=glCreateShader(type);const char* text=code.c_str();glShaderSource(shader,1,&text,nullptr);glCompileShader(shader);
    GLint ok{};glGetShaderiv(shader,GL_COMPILE_STATUS,&ok);
    if(!ok){char log[4096]{};glGetShaderInfoLog(shader,sizeof(log),nullptr,log);glDeleteShader(shader);throw std::runtime_error(log);}return shader;
}
static GLuint program(const char* vertex,const char* fragment) {
    auto vs=compile(GL_VERTEX_SHADER,vertex),fs=compile(GL_FRAGMENT_SHADER,fragment);auto p=glCreateProgram();
    glAttachShader(p,vs);glAttachShader(p,fs);glLinkProgram(p);glDeleteShader(vs);glDeleteShader(fs);
    GLint ok{};glGetProgramiv(p,GL_LINK_STATUS,&ok);if(!ok){char log[4096]{};glGetProgramInfoLog(p,sizeof(log),nullptr,log);glDeleteProgram(p);throw std::runtime_error(log);}return p;
}
struct GLScene::Impl {
    GLuint world{},panel{},sky_program{},sky_texture{},vao{},vbo{},panel_vao{},panel_vbo{};
    GLuint resources[6]{},rasters[3]{};
    std::vector<SceneTriangle> triangles;std::shared_ptr<const NativeFrame> frame;
    UploadStats uploads;
    std::array<bool,6> material_cached{};
    enum class Pixels {Background,Foreground,Native,Sky};
    struct RasterCache {bool valid{};Pixels source{};int width{},height{},storage_width{};};
    std::array<RasterCache,4> raster_cached{};
    // Every triangle owns its material attributes. Flat integer varyings retain
    // exact native bits without per-polygon uniforms or draw-call boundaries.
    struct Vertex {std::array<float,5> raster;std::array<int32_t,4> header;std::array<int32_t,3> properties;std::array<float,3> camera,native_homogeneous;};
    GLsizei vertex_count{},hud_vertex_count{};
    bool usable{},wider_view{},immersive{},prepared_immersive{},sky_ready{};
    int sky_width{},sky_height{};
    NativeLens presentation_override;
    struct WorldUniforms {GLint vp,lens,scale,distance,immersive,zoom,hud_size;} w{};
    struct PanelUniforms {GLint vp,lens,distance,size,hud_size;} p{};
    struct SkyUniforms {GLint rays,size,centre,repeat,focus,visible;} sky{};
    static const std::vector<uint32_t>& pixels(const NativeFrame& f,Pixels source) {
        switch(source){case Pixels::Background:return f.background;case Pixels::Foreground:return f.foreground;case Pixels::Native:return f.native;case Pixels::Sky:return f.sky;}
        return f.background;
    }
    void raster(GLuint texture,const std::vector<uint32_t>& pixels,bool transparent,int width=0,int height=0,int storage_width=0) {
        if(!width){width=frame->width;height=frame->height;}
        if(!storage_width)storage_width=width;
        std::vector<uint8_t> rgba(pixels.size()*4);
        for(size_t i=0;i<pixels.size();i++) {auto c=pixels[i];rgba[i*4]=c>>16;rgba[i*4+1]=c>>8;rgba[i*4+2]=c;rgba[i*4+3]=transparent&&c==0?0:255;}
        glBindTexture(GL_TEXTURE_2D,texture);glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,storage_width,height,0,GL_RGBA,GL_UNSIGNED_BYTE,storage_width==width?rgba.data():nullptr);
        if(storage_width!=width)glTexSubImage2D(GL_TEXTURE_2D,0,0,0,width,height,GL_RGBA,GL_UNSIGNED_BYTE,rgba.data());
    }
    void draw_sky(const glm::mat4& vp,const NativeLens& lens,float zoom) {
        if(!sky_ready)return;
        // Inverse projection yields rays in the same camera/world orientation
        // as geometry. Remove the eye position in double precision, so leaning
        // or the IPD cannot create parallax in an infinitely distant backdrop.
        const glm::dmat4 inverse=glm::inverse(glm::dmat4(vp));
        const glm::dvec4 eye_h=inverse*glm::dvec4(0,0,1,0);
        if(std::abs(eye_h.w)<1e-18)return;
        const glm::dvec3 eye=glm::dvec3(eye_h)/eye_h.w;
        glm::mat3 rays;
        for(int axis=0;axis<3;axis++) {
            const glm::dvec4 h=inverse*(axis==0?glm::dvec4(1,0,0,0):axis==1?glm::dvec4(0,1,0,0):glm::dvec4(0,0,0,1));
            rays[axis]=glm::vec3(glm::dvec3(h)-eye*h.w);
            rays[axis].x/=zoom;rays[axis].y/=zoom;
        }
        const float visible_width=float(std::min(frame->width,sky_width));
        const double mirror_period=2*std::max(1.f,visible_width-1);
        const float repeats=float(std::max(1.,std::round(6.283185307179586*lens.fx/mirror_period)));
        glUseProgram(sky_program);glUniformMatrix3fv(sky.rays,1,GL_FALSE,glm::value_ptr(rays));
        glUniform2f(sky.size,sky_width,sky_height);glUniform2f(sky.centre,lens.cx,lens.cy);glUniform1f(sky.repeat,repeats);glUniform1f(sky.focus,lens.fy);glUniform1f(sky.visible,visible_width);
        glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,sky_texture);glBindVertexArray(panel_vao);glDrawArrays(GL_TRIANGLES,0,3);
    }
    void draw_raster(unsigned index,const glm::mat4& vp,const NativeLens& lens,float distance,float hud_size=1) {
        const auto& pixels=index==0?frame->background:index==1?frame->foreground:frame->native;
        if(pixels.size()!=size_t(frame->width)*frame->height)return;
        glUseProgram(panel);glUniformMatrix4fv(p.vp,1,GL_FALSE,glm::value_ptr(vp));
        glUniform4f(p.lens,lens.fx,lens.fy,lens.cx,lens.cy);glUniform1f(p.distance,distance);glUniform2f(p.size,frame->width,frame->height);glUniform1f(p.hud_size,hud_size);
        glBindVertexArray(panel_vao);glActiveTexture(GL_TEXTURE0);glBindTexture(GL_TEXTURE_2D,rasters[index]);glDrawArrays(GL_TRIANGLES,0,6);
    }
};
GLScene::GLScene():impl_(std::make_unique<Impl>()) {
    auto& i=*impl_;i.world=program(Model2Vertex,Model2Fragment);i.panel=program(Model2PanelVertex,Model2PanelFragment);i.sky_program=program(Model2SkyVertex,Model2SkyFragment);
    i.w={glGetUniformLocation(i.world,"vp"),glGetUniformLocation(i.world,"lens"),glGetUniformLocation(i.world,"scale"),glGetUniformLocation(i.world,"distance"),glGetUniformLocation(i.world,"immersive"),glGetUniformLocation(i.world,"zoom"),glGetUniformLocation(i.world,"hud_size")};
    i.p={glGetUniformLocation(i.panel,"vp"),glGetUniformLocation(i.panel,"lens"),glGetUniformLocation(i.panel,"distance"),glGetUniformLocation(i.panel,"size"),glGetUniformLocation(i.panel,"hud_size")};
    i.sky={glGetUniformLocation(i.sky_program,"rays"),glGetUniformLocation(i.sky_program,"size"),glGetUniformLocation(i.sky_program,"centre"),glGetUniformLocation(i.sky_program,"repeats"),glGetUniformLocation(i.sky_program,"vertical_focus"),glGetUniformLocation(i.sky_program,"visible_width")};
    glUseProgram(i.sky_program);glUniform1i(glGetUniformLocation(i.sky_program,"image"),0);
    glGenTextures(1,&i.sky_texture);glBindTexture(GL_TEXTURE_2D,i.sky_texture);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_REPEAT);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);
    glUseProgram(i.world);constexpr const char* names[]={"texture0","texture1","palette","colorxlat","lumaram","gamma"};
    for(int n=0;n<6;n++)glUniform1i(glGetUniformLocation(i.world,names[n]),n);
    glUseProgram(i.panel);glUniform1i(glGetUniformLocation(i.panel,"image"),0);
    glGenTextures(6,i.resources);glGenTextures(3,i.rasters);
    for(auto texture:i.resources){glBindTexture(GL_TEXTURE_2D,texture);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);}
    for(auto texture:i.rasters){glBindTexture(GL_TEXTURE_2D,texture);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_NEAREST);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE);glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE);}
    glGenVertexArrays(1,&i.vao);glBindVertexArray(i.vao);glGenBuffers(1,&i.vbo);glBindBuffer(GL_ARRAY_BUFFER,i.vbo);
    glEnableVertexAttribArray(0);glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,sizeof(Impl::Vertex),nullptr);
    glEnableVertexAttribArray(1);glVertexAttribPointer(1,2,GL_FLOAT,GL_FALSE,sizeof(Impl::Vertex),reinterpret_cast<void*>(3*sizeof(float)));
    glEnableVertexAttribArray(2);glVertexAttribIPointer(2,4,GL_INT,sizeof(Impl::Vertex),reinterpret_cast<void*>(offsetof(Impl::Vertex,header)));
    glEnableVertexAttribArray(3);glVertexAttribIPointer(3,3,GL_INT,sizeof(Impl::Vertex),reinterpret_cast<void*>(offsetof(Impl::Vertex,properties)));
    glEnableVertexAttribArray(4);glVertexAttribPointer(4,3,GL_FLOAT,GL_FALSE,sizeof(Impl::Vertex),reinterpret_cast<void*>(offsetof(Impl::Vertex,camera)));
    glEnableVertexAttribArray(5);glVertexAttribPointer(5,3,GL_FLOAT,GL_FALSE,sizeof(Impl::Vertex),reinterpret_cast<void*>(offsetof(Impl::Vertex,native_homogeneous)));
    glGenVertexArrays(1,&i.panel_vao);glBindVertexArray(i.panel_vao);glGenBuffers(1,&i.panel_vbo);glBindBuffer(GL_ARRAY_BUFFER,i.panel_vbo);
    glEnableVertexAttribArray(0);glVertexAttribPointer(0,2,GL_FLOAT,GL_FALSE,2*sizeof(float),nullptr);
}
GLScene::~GLScene(){auto& i=*impl_;glDeleteProgram(i.world);glDeleteProgram(i.panel);glDeleteProgram(i.sky_program);glDeleteTextures(1,&i.sky_texture);glDeleteBuffers(1,&i.vbo);glDeleteBuffers(1,&i.panel_vbo);glDeleteVertexArrays(1,&i.vao);glDeleteVertexArrays(1,&i.panel_vao);glDeleteTextures(6,i.resources);glDeleteTextures(3,i.rasters);}
GLScene::UploadStats GLScene::upload_stats() const{return impl_->uploads;}
void GLScene::set_immersive(bool enabled){if(impl_->immersive!=enabled){impl_->immersive=enabled;prepare(impl_->frame,impl_->wider_view);}}
void GLScene::set_presentation_lens(const NativeLens& lens){impl_->presentation_override=lens;}
void GLScene::prepare(std::shared_ptr<const NativeFrame> frame,bool wider_view) {
    auto& i=*impl_;if(i.frame==frame&&i.wider_view==wider_view&&i.prepared_immersive==i.immersive)return;
    // Keep the immutable previous frame alive for exact comparison. No hashes,
    // extra multi-megabyte material copies or persistent old-frame chain.
    const auto previous=std::exchange(i.frame,std::move(frame));
    const auto previous_materials=std::exchange(i.material_cached,{});
    const auto previous_rasters=std::exchange(i.raster_cached,{});
    i.prepared_immersive=i.immersive;i.wider_view=wider_view;i.triangles.clear();i.vertex_count=0;i.hud_vertex_count=0;i.usable=false;i.sky_ready=false;if(!i.frame||!i.frame->complete)return;
    const auto& f=*i.frame;Model2Resources resource(f);i.triangles=scene_triangles(f,wider_view,i.immersive);i.usable=resource.valid()&&!i.triangles.empty();
    glActiveTexture(GL_TEXTURE0);glPixelStorei(GL_UNPACK_ALIGNMENT,1);
    constexpr const char* names[]={"texture0","texture1","palette","colorxlat","luma","gamma"};
    if(i.usable)for(int n=0;n<6;n++) {
        const auto current=std::find_if(f.blobs.rbegin(),f.blobs.rend(),[&](const Blob& blob){return blob.name==names[n];});
        if(current==f.blobs.rend())continue;
        const auto& b=*current;
        i.material_cached[n]=true;
        if(previous&&previous_materials[n]) {
            const auto old=std::find_if(previous->blobs.rbegin(),previous->blobs.rend(),[&](const Blob& blob){return blob.name==b.name;});
            if(old!=previous->blobs.rend()&&old->bytes==b.bytes)continue;
        }
        const int rows=int((b.bytes.size()+2047)/2048);std::vector<uint8_t> padded(size_t(rows)*2048);std::copy(b.bytes.begin(),b.bytes.end(),padded.begin());
        glBindTexture(GL_TEXTURE_2D,i.resources[n]);glTexImage2D(GL_TEXTURE_2D,0,GL_R8UI,2048,rows,0,GL_RED_INTEGER,GL_UNSIGNED_BYTE,padded.data());
        i.uploads.materials[n]++;i.uploads.bytes+=padded.size();
    }
    auto raster=[&](int slot,Impl::Pixels source,int width,int height,int storage_width=0) {
        const auto& pixels=Impl::pixels(f,source);
        if(width<=0||height<=0||pixels.size()!=size_t(width)*height)return;
        if(!storage_width)storage_width=width;
        i.raster_cached[slot]={true,source,width,height,storage_width};const auto& old=previous_rasters[slot];
        if(previous&&old.valid&&old.width==width&&old.height==height&&old.storage_width==storage_width&&Impl::pixels(*previous,old.source)==pixels)return;
        i.raster(slot==3?i.sky_texture:i.rasters[slot],pixels,slot==1,width,height,storage_width);
        if(slot==3)i.uploads.sky++;else i.uploads.rasters[slot]++;
        i.uploads.bytes+=pixels.size()*4;
    };
    if(i.usable) {
        if(!i.immersive)raster(0,Impl::Pixels::Background,f.width,f.height);
        raster(1,Impl::Pixels::Foreground,f.width,f.height);
    } else raster(2,Impl::Pixels::Native,f.width,f.height);
    if(i.immersive&&i.usable) {
        const bool full_sky=f.sky_width>0&&f.sky_height>0&&f.sky.size()==size_t(f.sky_width)*f.sky_height;
        const int source_width=full_sky?f.sky_width:f.width;
        // Native visible496 and full512 captures must use identical normalized
        // sampling precision. The hidden tail is never sampled by the mirror.
        i.sky_width=(source_width+31)&~31;i.sky_height=full_sky?f.sky_height:f.height;
        const auto& pixels=full_sky?f.sky:f.background;
        i.sky_ready=source_width>0&&i.sky_height>0&&pixels.size()==size_t(source_width)*i.sky_height;
        if(i.sky_ready)raster(3,full_sky?Impl::Pixels::Sky:Impl::Pixels::Background,source_width,i.sky_height,i.sky_width);
    }
    std::vector<Impl::Vertex> vertices;vertices.reserve(i.triangles.size()*3);
    auto append=[&](const SceneTriangle& triangle) {
        const auto& p=f.polygons[triangle.polygon];
        const std::array<int32_t,4> header={int32_t(p.header[0]),int32_t(p.header[1]),int32_t(p.header[2]),int32_t(p.header[3])};
        const std::array<int32_t,3> properties={int32_t(p.luma),int32_t(p.lod),int32_t(triangle.panel)};
        // Preserve the complete reverse native occupancy order in this one
        // triangle stream. Hardware primitive order still implements painter
        // composition, including window overrides, checker holes and alpha keys.
        for(const auto& vertex:triangle.vertices)vertices.push_back({{vertex.pixel_x,vertex.pixel_y,vertex.depth,vertex.u,vertex.v},header,properties,
            {float(vertex.camera.x),float(vertex.camera.y),float(vertex.camera.z)},
            {float(vertex.native_homogeneous.x),float(vertex.native_homogeneous.y),float(vertex.native_homogeneous.z)}});
    };
    for(auto it=i.triangles.rbegin();it!=i.triangles.rend();++it)if(!it->hud)append(*it);
    i.vertex_count=GLsizei(vertices.size());
    for(auto it=i.triangles.rbegin();it!=i.triangles.rend();++it)if(it->hud)append(*it);
    i.hud_vertex_count=GLsizei(vertices.size())-i.vertex_count;
    glBindBuffer(GL_ARRAY_BUFFER,i.vbo);glBufferData(GL_ARRAY_BUFFER,vertices.size()*sizeof(Impl::Vertex),vertices.data(),GL_STREAM_DRAW);
    const float w=float(f.width),h=float(f.height);float quad[]={0,0,w,0,w,h,0,0,w,h,0,h};
    glBindBuffer(GL_ARRAY_BUFFER,i.panel_vbo);glBufferData(GL_ARRAY_BUFFER,sizeof(quad),quad,GL_STREAM_DRAW);
}
void GLScene::draw(const glm::mat4& vp,float scale,float distance,float zoom,float hud_size) {
    auto& i=*impl_;if(!i.frame||!i.frame->complete||scale<=0||distance<=0)return;
    const auto lens=i.immersive?native_camera_lens(*i.frame):presentation_lens(*i.frame,&i.presentation_override);if(!lens.valid())return;
    hud_size=i.immersive&&i.wider_view&&i.usable&&!i.frame->raster_only&&std::isfinite(hud_size)&&hud_size>=.5f&&hud_size<=2.f?hud_size:1.f;
    if(!i.immersive||i.frame->raster_only||!std::isfinite(zoom)||zoom<=0)zoom=1;
    glEnable(GL_DEPTH_TEST);glDepthFunc(GL_ALWAYS);glDepthMask(GL_TRUE);glDisable(GL_BLEND);glDisable(GL_CULL_FACE);glDisable(GL_STENCIL_TEST);glColorMask(GL_TRUE,GL_TRUE,GL_TRUE,GL_TRUE);
    if(!i.usable){i.draw_raster(2,vp,lens,distance);glDepthFunc(GL_LESS);return;}
    // Native tilemap background is a backdrop, never an opaque depth sheet
    // between the player and actual camera-space world geometry.
    if(i.immersive)glDepthMask(GL_FALSE);
    if(i.immersive)i.draw_sky(vp,lens,zoom);else i.draw_raster(0,vp,lens,distance);
    if(i.immersive)glDepthMask(GL_TRUE);
    glUseProgram(i.world);glUniformMatrix4fv(i.w.vp,1,GL_FALSE,glm::value_ptr(vp));glUniform4f(i.w.lens,lens.fx,lens.fy,lens.cx,lens.cy);
    glUniform1f(i.w.scale,scale);glUniform1f(i.w.distance,distance);glUniform1i(i.w.immersive,i.immersive);glBindVertexArray(i.vao);
    glUniform1f(i.w.zoom,zoom);glUniform1f(i.w.hud_size,1);
    if(i.immersive)glDepthFunc(GL_LEQUAL);
    for(int n=0;n<6;n++){glActiveTexture(GL_TEXTURE0+n);glBindTexture(GL_TEXTURE_2D,i.resources[n]);}
    glDrawArrays(GL_TRIANGLES,0,i.vertex_count);
    glDepthFunc(GL_ALWAYS);if(i.immersive)glDepthMask(GL_FALSE);
    const auto hud_lens=i.immersive?panel_lens(*i.frame):lens;
    if(i.hud_vertex_count) {
        // Native cylinder meshes/digits keep their source pixels, materials and
        // painter order, at the same comfortable projection as native hearts.
        glUniform1f(i.w.hud_size,hud_size);
        glUniform4f(i.w.lens,hud_lens.fx,hud_lens.fy,hud_lens.cx,hud_lens.cy);
        glDrawArrays(GL_TRIANGLES,i.vertex_count,i.hud_vertex_count);
    }
    i.draw_raster(1,vp,hud_lens,distance,hud_size);glDepthMask(GL_TRUE);glActiveTexture(GL_TEXTURE0);glDepthFunc(GL_LESS);
}
}
