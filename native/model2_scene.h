// SPDX-License-Identifier: GPL-2.0-or-later
// Model 2 raster rules derived from MAME model2_v.cpp/model2rd.ipp,
// BSD-3-Clause, R. Belmont, Olivier Galibert, ElSemi, Angelo Salese,
// Matthew Daniels. Keep the accompanying attribution in source distributions.
#pragma once
#include "model2_capture.h"
#include "projection.h"
#include "native_zoom.h"
#include <algorithm>
#include <cstring>
#include <numeric>
#include <limits>

namespace vcop2 {
inline float frame_zoom(const NativeFrame& frame,bool enabled) {
    return frame.raster_only?1.f:float(native_zoom_factor(frame.native_fov,frame.native_reference_fov,enabled));
}
// The dominant focus in the verified tutorial capture (2239/2261 polygons).
// Alternate native focuses are reprojected through this reference, rather than
// changing the user's eye projection when the game draws another window.
inline NativeLens presentation_lens(const NativeFrame& frame,const NativeLens* override_lens=nullptr) {
    if(override_lens&&override_lens->valid())return *override_lens;
    if(frame.lens.valid())return frame.lens;
    return {frame.width,frame.height,1827.30969,1827.30969,248.,191.};
}
inline bool panel_polygon(const NativePolygon& p) {
    return std::abs(p.focus[0])<=1.001&&std::abs(p.focus[1])<=1.001;
}
inline bool native_hud_polygon(const NativePolygon& p) {
    // Original HUD dispatcher0x1a000 uses this fixed camera for every weapon;
    // its subroutines0x1a45c..1ac00 translate the HUD toZ1200. Rail-camera aim
    // markers must remain world geometry. Known cylinder IDs also identify old
    // diagnostic captures that omitted the native transform matrix.
    constexpr uint32_t cylinders[]={0x80587b,0x80594e,0x805a21,0x805af4,0x805bc7,0x805c86,0x805d59};
    if(p.window!=4||p.focus!=std::array<float,2>{1200,1200}||p.center!=std::array<float,2>{248,320}
       ||p.viewport!=std::array<int,4>{0,128,496,512}||p.count<3||p.count>4)return false;
    const bool cylinder=std::find(std::begin(cylinders),std::end(cylinders),p.object)!=std::end(cylinders);
    const bool hud_transform=p.matrix[11]==1200&&std::isfinite(p.matrix[9])&&std::isfinite(p.matrix[10])
        &&std::abs(p.matrix[9])<=300&&p.matrix[10]<=0&&p.matrix[10]>=-192;
    if(!hud_transform&&!cylinder)return false;
    for(unsigned n=0;n<p.count;n++) {
        const auto& v=p.vertices[n];
        if(!std::isfinite(v.x)||!std::isfinite(v.y)||!std::isfinite(v.z)||v.z<900||v.z>1500)return false;
        const float x=248+v.x/v.z,y=191-v.y/v.z;
        if(x<-32||x>528||y<190||y>416)return false;
        if(!hud_transform&&(v.z<1168.74f||v.z>1231.26f))return false;
    }
    return true;
}
inline NativeLens panel_lens(const NativeFrame& frame) {
    const double focus=frame.width/(2*std::tan(65.0*3.14159265358979323846/360));
    return {frame.width,frame.height,focus,focus,frame.width*.5,frame.height*.5};
}
inline NativeLens native_camera_lens(const NativeFrame& frame) {
    if(frame.raster_only)return panel_lens(frame);
    struct Camera {std::array<float,2> focus,center;unsigned count;};
    std::vector<Camera> cameras;
    for(const auto& p:frame.polygons) {
        if(panel_polygon(p)||native_hud_polygon(p)||!std::isfinite(p.focus[0])||!std::isfinite(p.focus[1])||p.focus[0]<=0||p.focus[1]<=0)continue;
        auto found=std::find_if(cameras.begin(),cameras.end(),[&](const Camera& c){return c.focus==p.focus&&c.center==p.center;});
        if(found==cameras.end())cameras.push_back({p.focus,p.center,p.count});else found->count+=p.count;
    }
    if(cameras.empty())return panel_lens(frame);
    const auto& camera=*std::max_element(cameras.begin(),cameras.end(),[](const Camera& a,const Camera& b){return a.count<b.count;});
    return {frame.width,frame.height,camera.focus[0],camera.focus[1],frame.xoffset+camera.center[0],384-camera.center[1]+frame.yoffset};
}
inline Vec3 camera_position(const NativePolygon& p,const NativeVertex& v) {
    if(panel_polygon(p)||std::abs(p.focus[0])<1e-6||std::abs(p.focus[1])<1e-6)return {NAN,NAN,NAN};
    return {v.x/p.focus[0],v.y/p.focus[1],-v.z};
}
inline std::array<Vec3,4> world_positions(const NativePolygon& p,const NativeFrame& frame,float scale,float distance,bool immersive=false) {
    std::array<Vec3,4> result{};auto lens=immersive?native_camera_lens(frame):presentation_lens(frame);
    const bool hud=immersive&&native_hud_polygon(p);if(hud)lens=panel_lens(frame);
    for(unsigned i=0;i<p.count&&i<4;i++) {
        const auto& v=p.vertices[i];
        if(immersive&&!panel_polygon(p)&&!hud){result[i]=camera_position(p,v)*scale;continue;}
        if(v.z<=0){result[i]={NAN,NAN,NAN};continue;}
        const double x=frame.xoffset+p.center[0]+v.x/v.z;
        const double y=384-p.center[1]+frame.yoffset-v.y/v.z;
        result[i]=lens.unproject(x,y,panel_polygon(p)||hud?distance:v.z*scale);
    }
    return result;
}
struct SceneVertex {float pixel_x{},pixel_y{},depth{},u{},v{};Vec3 camera;Vec3 native_homogeneous;};
struct SceneTriangle {std::array<SceneVertex,3> vertices;size_t polygon{};bool panel{},hud{};};

// The list is in the native front-to-back occupancy order. Reversing it gives
// the identical compositing result without requiring an OpenXR stencil buffer.
inline std::vector<size_t> native_order(const NativeFrame& frame) {
    std::vector<size_t> result(frame.polygons.size());std::iota(result.begin(),result.end(),0);
    std::sort(result.begin(),result.end(),[&](size_t a,size_t b){
        const auto& x=frame.polygons[a];const auto& y=frame.polygons[b];
        if(x.window!=y.window)return x.window>y.window;
        if(x.sort!=y.sort)return x.sort<y.sort;
        return a>b;
    });return result;
}
inline std::vector<SceneTriangle> scene_triangles(const NativeFrame& frame,bool wider_view=false,bool immersive=false) {
    std::vector<SceneTriangle> triangles;
    if(frame.raster_only)return triangles;
    for(size_t index:native_order(frame)) {
        const auto& p=frame.polygons[index];
        if(p.count<3||p.count>4||((p.header[0]>>13)&3)==1)continue;
        const bool hud=immersive&&native_hud_polygon(p);
        const bool world=immersive&&!panel_polygon(p)&&!hud;
        if(p.culled&&(!world||!(p.cull_reasons&CullKnown)||(p.cull_reasons&~uint32_t(CullKnown|CullBehind))))continue;
        if(world) {
            // Keep camera-space geometry signed and unprojected. The actual eye
            // pose determines clipping, including surfaces behind the native view.
            auto convert=[&](const NativeVertex& v){
                const float cx=frame.xoffset+p.center[0],cy=384-p.center[1]+frame.yoffset;
                SceneVertex result;result.depth=v.z;result.u=v.u/8;result.v=v.v/8;
                result.camera=camera_position(p,v);result.native_homogeneous={cx*v.z+v.x,cy*v.z-v.y,v.z};
                if(std::abs(v.z)>1e-8){result.pixel_x=cx+v.x/v.z;result.pixel_y=cy-v.y/v.z;}
                return result;
            };
            for(unsigned i=1;i+1<p.count;i++)triangles.push_back({{convert(p.vertices[0]),convert(p.vertices[i]),convert(p.vertices[i+1])},index,false});
            continue;
        }
        std::vector<NativeVertex> vertices(p.vertices.begin(),p.vertices.begin()+p.count);
        const float cx=frame.xoffset+p.center[0],cy=384-p.center[1]+frame.yoffset;
        // Clip BEFORE projection, retaining intersections and texture coordinates.
        // Near-plane intersections are required for polygons crossing the camera.
        auto clip=[&](auto plane){
            std::vector<NativeVertex> output;
            if(vertices.empty())return;
            auto previous=vertices.back();float pd=plane(previous);
            for(const auto& current:vertices) {
                const float cd=plane(current);
                if((pd>=0)!=(cd>=0)) {
                    const float t=pd/(pd-cd);
                    output.push_back({previous.x+(current.x-previous.x)*t,previous.y+(current.y-previous.y)*t,
                        previous.z+(current.z-previous.z)*t,previous.u+(current.u-previous.u)*t,previous.v+(current.v-previous.v)*t});
                }
                if(cd>=0)output.push_back(current);
                previous=current;pd=cd;
            }
            vertices=std::move(output);
        };
        clip([](const NativeVertex& v){return v.z-.0001f;});
        const bool full_viewport=p.viewport[0]+frame.xoffset<=0&&p.viewport[2]+frame.xoffset>=frame.width-1
            &&384-p.viewport[3]+frame.yoffset<=0&&384-p.viewport[1]+frame.yoffset>=frame.height-1;
        // Wider view reveals only world geometry originally clipped by the full
        // display frustum. Subwindows and screen-space overlays retain their crop.
        if(!wider_view||panel_polygon(p)||hud||!full_viewport) {
            const float left=std::max(0,p.viewport[0]+frame.xoffset),right=std::min(frame.width,p.viewport[2]+frame.xoffset);
            const float top=std::max(0,384-p.viewport[3]+frame.yoffset),bottom=std::min(frame.height,384-p.viewport[1]+frame.yoffset);
            if(left>right||top>bottom)continue;
            clip([&](const NativeVertex& v){return v.x+(cx-left)*v.z;});
            clip([&](const NativeVertex& v){return (right-cx)*v.z-v.x;});
            clip([&](const NativeVertex& v){return (cy-top)*v.z-v.y;});
            clip([&](const NativeVertex& v){return v.y+(bottom-cy)*v.z;});
        }
        auto convert=[&](const NativeVertex& v){
            SceneVertex result;result.pixel_x=cx+v.x/v.z;result.pixel_y=cy-v.y/v.z;result.depth=v.z;result.u=v.u/8;result.v=v.v/8;
            result.native_homogeneous={cx*v.z+v.x,cy*v.z-v.y,v.z};return result;
        };
        for(size_t i=1;i+1<vertices.size();i++)triangles.push_back({{convert(vertices[0]),convert(vertices[i]),convert(vertices[i+1])},index,panel_polygon(p)||hud,hud});
    }return triangles;
}

class Model2Resources {
    const SharedBytes* data_[6]{};
    static uint32_t lerp(uint32_t x,uint32_t y,unsigned a){return (x+(((y-x)*a)>>8))&0x00ff00ff;}
public:
    explicit Model2Resources(const NativeFrame& f) {
        constexpr const char* names[]={"texture0","texture1","palette","colorxlat","luma","gamma"};
        for(const auto& b:f.blobs)for(int i=0;i<6;i++)if(b.name==names[i])data_[i]=&b.bytes;
    }
    bool valid()const {
        constexpr size_t sizes[]={2097152,2097152,16384,49152,32768,256};
        for(int i=0;i<6;i++)if(!data_[i]||data_[i]->size()<sizes[i])return false;
        return true;
    }
    uint8_t byte(int resource,size_t offset)const{return data_[resource]&&offset<data_[resource]->size()?(*data_[resource])[offset]:0;}
    uint16_t word(int resource,size_t offset)const{return byte(resource,offset*2)|(unsigned(byte(resource,offset*2+1))<<8);}
    unsigned texel(unsigned sheet,int base_x,int base_y,int x,int y)const {
        int x2=base_x+x,y2=base_y+y;if(x2>=1024){x2-=1024;y2^=1024;}
        if(x2<0||x2>=1024||y2<0||y2>=2048)return 15;
        unsigned value=word(sheet,(y2/2)*512+x2/2);
        if(!(y&1))value>>=8;
        if(!(x&1))value>>=4;
        return value&15;
    }
    uint32_t filtered(const NativePolygon& p,int level,int u,int v)const {
        const auto& h=p.header;int w,hg,x,y,sheet;
        const bool micro=level<0;const int mirrorx=(h[0]>>8)&1,mirrory=(h[0]>>9)&1;
        if(micro){w=hg=128;x=((h[2]>>13)&1)*128;y=((h[2]>>14)&3)*128;sheet=1;u<<=1<<((h[0]>>10)&3);v<<=1<<((h[0]>>10)&3);}
        else {w=(32<<((h[0]>>0)&7))>>level;hg=(32<<((h[0]>>3)&7))>>level;
            x=((32*int(h[2]&63)-2048)>>level)&2047;y=((32*int((h[2]>>6)&31)-1024)>>level)&1023;sheet=level&1;
            u>>=level;v>>=level;}
        sheet^=(h[2]>>12)&1;
        if(mirrorx&&(u&(w<<8)))u=~u;
        if(mirrory&&(v&(hg<<8)))v=~v;
        u-=128;v-=128;unsigned uf=u&255,vf=v&255;
        int u0=(u>>8)&(w-1),u1=(u0+1)&(w-1),v0=(v>>8)&(hg-1),v1=(v0+1)&(hg-1);
        if((!((h[0]>>6)&1)||mirrorx)&&u1==0){if(uf>=128){u0=u1;u1++;uf=0;}else{u1=u0;u0--;uf=256;}}
        if((!((h[0]>>7)&1)||mirrory)&&v1==0){if(vf>=128){v0=0;v1++;vf=0;}else{v1=v0;v0--;vf=256;}}
        uint32_t a=texel(sheet,x,y,u0,v0)<<4,b=texel(sheet,x,y,u1,v0)<<4,c=texel(sheet,x,y,u0,v1)<<4,d=texel(sheet,x,y,u1,v1)<<4;
        const bool transparent=(h[0]&0x2000)!=0;
        if(transparent){for(auto ptr:{&a,&b,&c,&d})if(*ptr!=240)*ptr|=0x00800000;
            if(a==240)a=b&255;
            if(b==240)b=a&255;
            if(c==240)c=d&255;
            if(d==240)d=c&255;}
        a=lerp(a,b,uf);c=lerp(c,d,uf);
        if(transparent){if(a==240)a=c&255;if(c==240)c=a&255;}
        return lerp(a,c,vf);
    }
    std::optional<uint32_t> sample(const NativePolygon& p,float u,float v,float depth,int px,int py)const {
        const auto& h=p.header;const unsigned renderer=(h[0]>>13)&3;
        if(renderer==1||((h[0]&0x8000)&&!((px^py)&1)))return {};
        unsigned luma=p.luma>>2;
        if(renderer&2) {
            if(depth<=0||!std::isfinite(depth))return {};
            uint32_t bits;std::memcpy(&bits,&depth,4);bits>>=16;
            const int log=((int(bits>>7)-127)*256)|int(std::log2(1.f+float(bits&127)/128)*256);
            int max_level=0;for(unsigned n=std::min(32u<<(h[0]&7),32u<<((h[0]>>3)&7));n>2;n>>=1)max_level++;
            const int mml=log-p.lod,level=std::clamp(mml>>7,0,max_level);
            const int iu=int(u*256),iv=int(v*256);
            uint32_t t=filtered(p,level,iu,iv);
            if(mml>0&&level<max_level)t=lerp(t,filtered(p,level+1,iu,iv),(mml&127)<<1);
            else if((h[0]&0x1000)&&mml<0)t=lerp(t,filtered(p,-1,iu,iv),std::min((-mml)>>((h[0]>>10)&3),127));
            if(renderer==3){if(t<0x00400000)return {};t&=255;}
            luma=std::min(63u,(unsigned(byte(4,((h[1]&255)<<7)+(t>>1)))*p.luma/256)&255);
        }
        unsigned color=word(2,0x1000+((h[3]>>6)&1023));uint32_t rgb=0xff000000;
        for(int c=0;c<3;c++){unsigned raw=word(3,c*8192+(((color>>(c*5))&31)<<8)+luma)&255;rgb|=unsigned(byte(5,raw))<<(16-c*8);}
        return rgb;
    }
};
struct SceneAim {std::optional<Vec3> target;AimPoint native;};
inline SceneAim trace_scene_aim(Ray ray,const NativeFrame& frame,float scale,float distance,bool wider_view=false,const NativeLens* override_lens=nullptr,bool immersive=false,double zoom=1) {
    const auto lens=immersive?native_camera_lens(frame):presentation_lens(frame,override_lens);
    if(!frame.complete||!lens.valid()||scale<=0||distance<=0)return {};
    if(!immersive||frame.raster_only||!std::isfinite(zoom)||zoom<=0)zoom=1;
    Model2Resources resources(frame);
    const auto triangles=resources.valid()?scene_triangles(frame,wider_view,immersive):std::vector<SceneTriangle>{};
    SceneAim nearest;double nearest_distance=std::numeric_limits<double>::infinity();
    for(const auto& triangle:triangles) {
        if(immersive&&triangle.hud)continue;
        std::array<Vec3,3> positions;
        for(int i=0;i<3;i++){const auto& v=triangle.vertices[i];positions[i]=immersive&&!triangle.panel?zoomed_world_position(v.camera,zoom)*scale:lens.unproject(v.pixel_x,v.pixel_y,triangle.panel?distance:v.depth*scale);}
        auto point=intersect_triangle(ray,positions);if(!point)continue;
        const auto e0=positions[1]-positions[0],e1=positions[2]-positions[0],q=*point-positions[0];
        const double d00=dot(e0,e0),d01=dot(e0,e1),d11=dot(e1,e1),d20=dot(q,e0),d21=dot(q,e1),den=d00*d11-d01*d01;
        if(std::abs(den)<1e-20)continue;
        double b=(d11*d20-d01*d21)/den,c=(d00*d21-d01*d20)/den,a=1-b-c;
        auto native=lens.project_point(immersive&&!triangle.panel?unzoomed_world_position(*point,zoom):*point);
        float u=0,v=0,ooz=0;Vec3 original{};const double weights[]={a,b,c};
        for(int i=0;i<3;i++) {
            const auto& vertex=triangle.vertices[i];double w=weights[i]*(triangle.panel?1./vertex.depth:1.);
            u+=w*vertex.u;v+=w*vertex.v;ooz+=w;original=original+vertex.native_homogeneous*w;
        }
        if(std::abs(ooz)<1e-20)continue;
        u/=ooz;v/=ooz;
        float depth=triangle.panel?1/ooz:float(std::max(0.0001,std::abs(point->z/scale)));
        int px=0,py=0;
        if(immersive&&std::abs(original.z)>1e-10){px=int(std::floor(original.x/original.z));py=int(std::floor(original.y/original.z));}
        else if(native.valid){px=int(std::floor(native.u*(frame.width-1)));py=int(std::floor(native.v*(frame.height-1)));}
        if(!resources.sample(frame.polygons[triangle.polygon],u,v,depth,px,py))continue;
        if(!immersive)return {point,native};
        const double d=dot(*point-ray.origin,*point-ray.origin);
        if(d<nearest_distance){nearest_distance=d;nearest={point,native};}
    }
    if(nearest.target)return nearest;
    if(!finite(ray.origin)||!finite(ray.direction)||std::abs(ray.direction.z)<1e-12)return {};
    const double t=(-distance-ray.origin.z)/ray.direction.z;if(t<=0)return {};
    Vec3 point=ray.origin+ray.direction*t;return {point,lens.project_point(unzoomed_world_position(point,zoom))};
}
}
