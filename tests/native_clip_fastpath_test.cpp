// SPDX-License-Identifier: GPL-2.0-or-later
#include "../native/native_clip.h"
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <random>
#include <stdexcept>

struct poly_vertex {float x{},y{},p[4]{};};
struct model2_state {struct plane {poly_vertex normal;float distance{};};};
#define pz p[0]
#define pu p[1]
#define pv p[2]
// The driver extracts these functions from the actual pinned, patched MAME
// source. This test does not maintain another implementation of its clipper.
#include VCOP2_CLIP_REFERENCE_HEADER
using Planes=std::array<model2_state::plane,4>;
static uint64_t cases{},accepted{},fallback{};
static void require(bool pass,const char* message) {if(!pass)throw std::runtime_error(message);}
static poly_vertex vertex(float x,float y,float z,float u=0,float v=0) {return {x,y,{z,u,v,123.25f}};}
static Planes planes(float cx=248,float cy=320,float left=0,float bottom=128,float right=496,float top=512) {
    Planes result{};
    const std::array<std::array<float,3>,4> raw{{{1,0,cx-left},{-1,0,right-cx},{0,-1,top-cy},{0,1,cy-bottom}}};
    for(unsigned i=0;i<4;++i) {
        const auto& n=raw[i];const float length=std::hypot(n[0]?n[0]:n[1],n[2]);
        result[i].normal=vertex(n[0]/length,n[1]/length,n[2]/length);
    }
    return result;
}
template<unsigned N> static void compare(const std::array<poly_vertex,4>& input,const Planes& limits,int expected=-1) {
    std::array<poly_vertex,32> work{},reference{},actual{};
    std::copy_n(input.begin(),N,work.begin());int count=N;
    for(const auto& plane:limits) {
        count=clip_polygon(work.data(),count,reference.data(),plane);
        require(count>=0&&count<=8,"reference native polygon exceeds its original eight-vertex storage");
        std::copy_n(reference.begin(),count,work.begin());
    }
    for(bool enabled:{true,false}) {
        unsigned clips{};
        auto original=[&](poly_vertex* in,int size,poly_vertex* out,model2_state::plane plane){++clips;return clip_polygon(in,size,out,plane);};
        const int got=vcop2::clip_polygon_frustum<N>(input.data(),actual.data(),limits.data(),dot_product,original,enabled);
        require(got==count,"fast path changed original vertex count");
        require(clips==0||clips==4,"mixed case did not run the full original four-plane path");
        if(!enabled)require(clips==4,"disabled fast path did not run the complete original clipper");
        else if(expected>=0)require(clips==unsigned(expected),"expected accept/fallback branch was not exercised");
        for(int i=0;i<count;++i) {
            const auto& a=actual[i];const auto& b=reference[i];
            const std::array<float,5> av{a.x,a.y,a.pz,a.pu,a.pv},bv{b.x,b.y,b.pz,b.pu,b.pv};
            for(unsigned field=0;field<5;++field)require(std::bit_cast<uint32_t>(av[field])==std::bit_cast<uint32_t>(bv[field]),"fast path changed an original vertex field bit pattern");
        }
        if(enabled){if(clips)++fallback;else ++accepted;}
    }
    ++cases;
}
static void compare(unsigned n,const std::array<poly_vertex,4>& input,const Planes& limits,int expected=-1) {
    require(n==3||n==4,"fixture must contain native triangles or quads");
    if(n==3)compare<3>(input,limits,expected);else compare<4>(input,limits,expected);
}
int main() try {
    auto limits=planes();
    const std::array<poly_vertex,4> inside{vertex(-1,-1,2,-0.f,1),vertex(1,-1,2,2,-0.f),vertex(1,1,2,3,4),vertex(-1,1,2,5,6)};
    compare<3>(inside,limits,0);compare<4>(inside,limits,0);
    auto outside=inside;for(auto& p:outside)p.x=1000;
    compare<3>(outside,limits,4);compare<4>(outside,limits,4);
    // Equality, adjacent floats, denormals and signed zero must retain >= and
    // the original arithmetic, rather than an epsilon or inverse comparison.
    const std::array<float,11> boundary{0.f,-0.f,std::nextafter(0.f,1.f),std::nextafter(0.f,-1.f),1.f,
        std::nextafter(1.f,0.f),std::nextafter(1.f,2.f),-1.f,std::nextafter(-1.f,0.f),std::nextafter(-1.f,-2.f),std::numeric_limits<float>::min()};
    for(float value:boundary)for(unsigned axis=0;axis<3;++axis) {
        auto p=inside;for(auto& v:p){if(axis==0)v.x=value;else if(axis==1)v.y=value;else v.pz=value;}
        compare<3>(p,limits);compare<4>(p,limits);
    }
    Planes box{};box[0].normal.x=1;box[1].normal.x=-1;box[2].normal.y=1;box[3].normal.y=-1;
    for(auto& p:box)p.distance=-1;
    for(float edge:boundary){auto p=inside;p[0].x=edge;compare<3>(p,box);compare<4>(p,box);}
    // Exercise cancellation at the actual normalized native frustum edges,
    // not just simple axis-aligned planes, across many exponent ranges.
    for(int exponent=-20;exponent<=20;++exponent)for(unsigned side=0;side<4;++side)for(int step=-2;step<=2;++step) {
        const float z=std::ldexp(1.f,exponent);
        float edge=(side<2?248.f:192.f)*z;
        if(side==0||side==3)edge=-edge;
        for(int i=0;i<std::abs(step);++i)edge=std::nextafter(edge,step<0?-std::numeric_limits<float>::infinity():std::numeric_limits<float>::infinity());
        auto p=inside;for(auto& v:p){v.pz=z;if(side<2)v.x=edge;else v.y=edge;}
        compare<3>(p,limits);compare<4>(p,limits);
    }
    for(float exceptional:{std::numeric_limits<float>::quiet_NaN(),std::bit_cast<float>(0xffc12345u),
                           std::numeric_limits<float>::infinity(),-std::numeric_limits<float>::infinity()}) {
        for(unsigned axis=0;axis<3;++axis) {auto p=inside;if(axis==0)p[0].x=exceptional;else if(axis==1)p[0].y=exceptional;else p[0].pz=exceptional;compare<3>(p,limits,std::isnan(exceptional)?4:-1);compare<4>(p,limits,std::isnan(exceptional)?4:-1);}
        auto p=limits;p[0].distance=exceptional;compare<3>(inside,p);compare<4>(inside,p);
        p=limits;p[0].normal.x=exceptional;compare<3>(inside,p,4);compare<4>(inside,p,4);
    }
    auto degenerate=inside;for(auto& p:degenerate)p=vertex(-0.f,+0.f,1,-0.f,+0.f);
    compare<3>(degenerate,limits,0);compare<4>(degenerate,limits,0);
    // Planar, convex input covers full accept/reject, every mixed edge and
    // repeated clipping without violating the real rasterizer's capacity.
    std::mt19937 random(0x564332);std::uniform_real_distribution<float> unit(-1,1);
    for(unsigned i=0;i<80000;++i) {
        auto p=inside;const float z=unit(random)*80,x=unit(random)*12000,y=unit(random)*12000;
        const float sx=unit(random)*4000,sy=unit(random)*4000,dx=unit(random)*20,dy=unit(random)*20;
        constexpr int corners[4][2]={{-1,-1},{1,-1},{1,1},{-1,1}};
        for(unsigned v=0;v<4;++v){const auto* c=corners[v];p[v]=vertex(x+c[0]*sx,y+c[1]*sy,z+c[0]*dx+c[1]*dy,unit(random)*2048,unit(random)*2048);}
        compare(i&1?3:4,p,limits);
    }
    // Owned captures arrive over stdin; the driver never writes their vertices
    // into source fixtures or software delivery files.
    unsigned captured{};std::cin>>captured;
    for(unsigned i=0;i<captured;++i) {
        unsigned n;float cx,cy,left,bottom,right,top;std::cin>>n>>cx>>cy>>left>>bottom>>right>>top;
        std::array<poly_vertex,4> p{};
        for(unsigned v=0;v<n;++v)std::cin>>p[v].x>>p[v].y>>p[v].pz>>p[v].pu>>p[v].pv;
        require(bool(std::cin),"incomplete private capture fixture");compare(n,p,planes(cx,cy,left,bottom,right,top));
    }
    std::cout<<"Bitwise native clipping passed "<<cases<<" cases with fast path both enabled and disabled ("<<accepted<<" direct accepts, "<<fallback<<" original fallbacks, "<<captured<<" owned capture polygons)\n";
    return 0;
} catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
