// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
namespace vcop2 {
// A wholly accepted polygon cannot gain intersections in any original plane
// pass: preserve its vertices directly. Every other case uses the original
// clipper, in the original plane order and with its original arithmetic.
template<unsigned NumVerts,class Vertex,class Plane,class Dot,class Clip>
int clip_polygon_frustum(const Vertex* input,Vertex* output,const Plane* planes,Dot dot,Clip clip,bool fast=true) {
    static_assert(NumVerts==3||NumVerts==4);
    bool inside=fast;
    for(unsigned plane=0;plane<4&&inside;++plane)
        for(unsigned vertex=0;vertex<NumVerts;++vertex)
            // Keep the exact original >= comparison. NaNs must fall back.
            if(!(dot(input[vertex],planes[plane].normal)>=planes[plane].distance)){inside=false;break;}
    if(inside) {
        for(unsigned vertex=0;vertex<NumVerts;++vertex)output[vertex]=input[vertex];
        return NumVerts;
    }
    Vertex work[8];
    for(unsigned vertex=0;vertex<NumVerts;++vertex)work[vertex]=input[vertex];
    int count=NumVerts;
    for(unsigned plane=0;plane<4;++plane) {
        count=clip(work,count,output,planes[plane]);
        for(int vertex=0;vertex<count;++vertex)work[vertex]=output[vertex];
    }
    return count;
}
}
