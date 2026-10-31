// SPDX-License-Identifier: GPL-2.0-or-later
// Raster math follows MAME's BSD-3-Clause model2rd.ipp (see model2_scene.h).
#pragma once
namespace vcop2 {
inline constexpr const char* Model2Vertex=R"(#version 330
layout(location=0) in vec3 raster;
layout(location=1) in vec2 texcoord;
layout(location=2) in ivec4 material_header;
layout(location=3) in ivec3 material_properties;
layout(location=4) in vec3 camera_position;layout(location=5) in vec3 original_homogeneous;
uniform mat4 vp;uniform vec4 lens;uniform float scale;uniform float distance;uniform float zoom;uniform float hud_size;uniform bool immersive;
out vec3 native_position;out vec3 texture_position;
flat out ivec4 header;flat out int polygon_luma;flat out int polygon_lod;flat out int world_geometry;
void main(){
    header=material_header;polygon_luma=material_properties.x;polygon_lod=material_properties.y;
    world_geometry=immersive&&material_properties.z==0?1:0;
    if(world_geometry!=0){
        gl_Position=vp*vec4(camera_position*vec3(zoom,zoom,1.0)*scale,1.0);
        native_position=original_homogeneous*scale;texture_position=vec3(texcoord,1.0)*scale;return;
    }
    float z=material_properties.z!=0?distance:raster.z*scale;
    vec3 world=vec3((raster.x-lens.z)*z/lens.x,(lens.w-raster.y)*z/lens.y,-z);
    world.xy*=hud_size;
    gl_Position=vp*vec4(world,1.0);
    native_position=vec3(raster.xy,1.0)*z;
    texture_position=vec3(texcoord,1.0)*(z/raster.z);
})";
inline constexpr const char* Model2Fragment=R"(#version 330
in vec3 native_position;in vec3 texture_position;out vec4 color;
uniform usampler2D texture0;uniform usampler2D texture1;uniform usampler2D palette;
uniform usampler2D colorxlat;uniform usampler2D lumaram;uniform usampler2D gamma;
flat in ivec4 header;flat in int polygon_luma;flat in int polygon_lod;flat in int world_geometry;
uint read_byte(usampler2D image,int offset){return texelFetch(image,ivec2(offset&2047,offset>>11),0).r;}
uint read_word(usampler2D image,int offset){return read_byte(image,offset*2)|(read_byte(image,offset*2+1)<<8);}
uint texel(int sheet,int bx,int by,int x,int y){
    int xx=bx+x,yy=by+y;if(xx>=1024){xx-=1024;yy^=1024;}
    if(xx<0||xx>=1024||yy<0||yy>=2048)return 15u;
    int offset=(yy/2)*512+xx/2;
    uint value=sheet==0?read_word(texture0,offset):read_word(texture1,offset);
    if((y&1)==0)value>>=8;if((x&1)==0)value>>=4;return value&15u;
}
uint mix_fixed(uint x,uint y,uint a){return (x+(((y-x)*a)>>8))&0x00ff00ffu;}
uint filtered(int level,int u,int v){
    int w,h,x,y,sheet;int mx=(header.x>>8)&1,my=(header.x>>9)&1;
    if(level==-1){w=128;h=128;x=((header.z>>13)&1)*128;y=((header.z>>14)&3)*128;
        sheet=1;u<<=1<<((header.x>>10)&3);v<<=1<<((header.x>>10)&3);}
    else{w=(32<<(header.x&7))>>level;h=(32<<((header.x>>3)&7))>>level;
        x=((32*(header.z&63)-2048)>>level)&2047;y=((32*((header.z>>6)&31)-1024)>>level)&1023;
        sheet=level&1;u>>=level;v>>=level;}
    sheet^=(header.z>>12)&1;
    if(mx!=0&&(u&(w<<8))!=0)u=~u;if(my!=0&&(v&(h<<8))!=0)v=~v;
    u-=128;v-=128;uint uf=uint(u&255),vf=uint(v&255);
    int u0=(u>>8)&(w-1),u1=(u0+1)&(w-1),v0=(v>>8)&(h-1),v1=(v0+1)&(h-1);
    if((((header.x>>6)&1)==0||mx!=0)&&u1==0){if(uf>=128u){u0=u1;u1++;uf=0u;}else{u1=u0;u0--;uf=256u;}}
    if((((header.x>>7)&1)==0||my!=0)&&v1==0){if(vf>=128u){v0=0;v1++;vf=0u;}else{v1=v0;v0--;vf=256u;}}
    uint a=texel(sheet,x,y,u0,v0)<<4,b=texel(sheet,x,y,u1,v0)<<4;
    uint c=texel(sheet,x,y,u0,v1)<<4,d=texel(sheet,x,y,u1,v1)<<4;
    if((header.x&8192)!=0){
        if(a!=240u)a|=0x00800000u;if(b!=240u)b|=0x00800000u;
        if(c!=240u)c|=0x00800000u;if(d!=240u)d|=0x00800000u;
        if(a==240u)a=b&255u;if(b==240u)b=a&255u;if(c==240u)c=d&255u;if(d==240u)d=c&255u;
    }
    a=mix_fixed(a,b,uf);c=mix_fixed(c,d,uf);
    if((header.x&8192)!=0){if(a==240u)a=c&255u;if(c==240u)c=a&255u;}
    return mix_fixed(a,c,vf);
}
void main(){
    float native_z=native_position.z;
    if(world_geometry!=0&&abs(native_z)<0.000001)native_z=native_z<0.0?-0.000001:0.000001;
    vec2 source_pixel=native_position.xy/native_z;
    if(world_geometry!=0)source_pixel=clamp(source_pixel,vec2(-100000000.0),vec2(100000000.0));
    ivec2 pixel=ivec2(floor(source_pixel));
    int renderer=(header.x>>13)&3;
    if(renderer==1||((header.x&32768)!=0&&((pixel.x^pixel.y)&1)==0))discard;
    uint luma=uint(polygon_luma>>2);
    if((renderer&2)!=0){
        float depth=native_position.z/texture_position.z;
        if(world_geometry!=0)depth=max(abs(depth),0.0001);
        uint bits=floatBitsToUint(depth)>>16;
        int logarithm=((int(bits>>7)-127)*256)|int(log2(1.0+float(bits&127u)/128.0)*256.0);
        int mml=logarithm-polygon_lod;
        int max_level=0;int dimension=min(32<<(header.x&7),32<<((header.x>>3)&7));
        for(int j=0;j<11;j++){if(dimension>2){dimension>>=1;max_level++;}}
        int level=clamp(mml>>7,0,max_level);
        ivec2 uv=ivec2(texture_position.xy/texture_position.z*256.0);
        uint t=filtered(level,uv.x,uv.y);
        if(mml>0&&level<max_level)t=mix_fixed(t,filtered(level+1,uv.x,uv.y),uint((mml&127)<<1));
        else if((header.x&4096)!=0&&mml<0)t=mix_fixed(t,filtered(-1,uv.x,uv.y),uint(min((-mml)>>((header.x>>10)&3),127)));
        if(renderer==3){if(t<0x00400000u)discard;t&=255u;}
        luma=min(63u,(read_byte(lumaram,((header.y&255)<<7)+int(t>>1))*uint(polygon_luma)/256u)&255u);
    }
    uint base=read_word(palette,4096+((header.w>>6)&1023));
    vec3 rgb;
    for(int c=0;c<3;c++){
        uint raw=read_word(colorxlat,c*8192+int(((base>>(c*5))&31u)<<8)+int(luma))&255u;
        rgb[c]=float(read_byte(gamma,int(raw)))/255.0;
    }
    color=vec4(rgb,1.0);
})";
inline constexpr const char* Model2PanelVertex=R"(#version 330
layout(location=0) in vec2 pixel;uniform mat4 vp;uniform vec4 lens;uniform float distance;
uniform float hud_size;uniform vec2 size;out vec2 uv;
void main(){vec3 p=vec3((pixel.x-lens.z)/lens.x,(lens.w-pixel.y)/lens.y,-1.0)*distance;
    p.xy*=hud_size;gl_Position=vp*vec4(p,1.0);uv=pixel/size;})";
inline constexpr const char* Model2PanelFragment=R"(#version 330
in vec2 uv;uniform sampler2D image;out vec4 color;
void main(){color=texture(image,uv);if(color.a==0.0)discard;})";
// Native Sys24 sky art is an infinitely distant rotational background. Its
// mirrored visible-window extension closes the panorama without using the
// native art page seam outside the original arcade viewport.
inline constexpr const char* Model2SkyVertex=R"(#version 330
uniform mat3 rays;out vec3 direction;
void main(){
    vec2 point=gl_VertexID==0?vec2(-1.0,-1.0):gl_VertexID==1?vec2(3.0,-1.0):vec2(-1.0,3.0);
    gl_Position=vec4(point,1.0,1.0);direction=rays*vec3(point,1.0);
})";
inline constexpr const char* Model2SkyFragment=R"(#version 330
in vec3 direction;uniform sampler2D image;uniform vec2 size;uniform vec2 centre;uniform float repeats;uniform float vertical_focus;uniform float visible_width;out vec4 color;
void main(){
    const float pi=3.14159265358979323846;
    float horizontal=length(direction.xz);
    float longitude=horizontal>0.000001?atan(direction.x,-direction.z):0.0;
    // This is the original camera's vertical cylindrical projection. Unlike
    // stretching visible image rows over a hemisphere, it retains the native
    // cloud band height. The original uniform top sky row supplies the zenith.
    float y=centre.y-vertical_focus*direction.y/max(horizontal,0.000001);
    float span=max(1.0,visible_width-1.0);
    float period=2.0*span;
    float phase=mod(centre.x+period*repeats*longitude/(2.0*pi),period);
    float x=span-abs(phase-span);
    vec2 uv=vec2((x+0.5)/size.x,clamp((y+0.5)/size.y,0.5/size.y,1.0-0.5/size.y));
    color=vec4(texture(image,uv).rgb,1.0);
})";

}
