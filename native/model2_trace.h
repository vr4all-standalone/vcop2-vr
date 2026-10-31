// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "model2_capture.h"
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <locale>
#include <sstream>

namespace vcop2 {
inline SceneCapture& trace_capture(){static SceneCapture capture;return capture;}
inline bool& trace_active(){static bool active{};return active;}
inline bool& trace_geometry_pending(){static bool pending{};return pending;}
inline uint32_t& trace_object(){static uint32_t object{};return object;}
inline std::filesystem::path& trace_pending_raster(){static std::filesystem::path path;return path;}
inline std::array<bool,3>& trace_rasters_written(){static std::array<bool,3> written{};return written;}

// Opt-in bounded diagnostic capture; normal native output is left untouched.
inline void trace_begin(uint64_t sequence,double time) {
    static unsigned captures{};
    static double next=5;
    trace_active()=std::getenv("VCOP2_SCENE_DIR")&&captures<12&&time>=next&&!trace_geometry_pending();
    trace_object()=0xffffffff;
    if(trace_active()){trace_capture().begin(sequence,time);++captures;next=time+5;}
}

template<class Range> void json_array(std::ostream& out,const Range& values) {
    out<<'[';bool first=true;for(auto value:values){if(!first)out<<',';first=false;out<<value;}out<<']';
}

inline void write_trace(const NativeFrame& frame,const std::filesystem::path& root) {
    trace_pending_raster().clear();trace_rasters_written()={};
    std::ostringstream name;name<<"frame-"<<std::setw(8)<<std::setfill('0')<<frame.sequence;
    const auto destination=root/name.str();
    std::filesystem::create_directories(root);
    if(!std::filesystem::create_directory(destination))throw std::runtime_error("Scene capture already exists");
    // Consumers accept only metadata.json; an interrupted .partial is incomplete.
    const auto temporary=destination/"metadata.partial";
    std::ofstream out(temporary,std::ios::binary);out.exceptions(std::ios::failbit|std::ios::badbit);
    out.imbue(std::locale::classic());out<<std::setprecision(9);
    out<<"{\"schema\":1,\"sequence\":"<<frame.sequence<<",\"time\":"<<frame.time
       <<",\"width\":"<<frame.width<<",\"height\":"<<frame.height
       <<",\"xoffset\":"<<frame.xoffset<<",\"yoffset\":"<<frame.yoffset
       <<",\"native_fov\":"<<frame.native_fov<<",\"native_reference_fov\":"<<frame.native_reference_fov
       <<",\"rejected\":"<<frame.rejected<<",\"blobs\":[";
    bool first=true;
    for(const auto& blob:frame.blobs) {
        std::ofstream bytes(destination/(blob.name+".bin"),std::ios::binary);
        bytes.exceptions(std::ios::failbit|std::ios::badbit);
        bytes.write(reinterpret_cast<const char*>(blob.bytes.data()),blob.bytes.size());bytes.close();
        if(!first)out<<',';first=false;
        out<<"{\"name\":\""<<blob.name<<"\",\"size\":"<<blob.bytes.size()<<'}';
    }
    out<<"],\"polygons\":[";first=true;
    for(const auto& p:frame.polygons) {
        if(!first)out<<',';first=false;
        out<<"{\"attr\":"<<p.attr<<",\"object\":"<<p.object<<",\"window\":"<<p.window
           <<",\"sort\":"<<p.sort<<",\"luma\":"<<p.luma<<",\"lod\":"<<p.lod
           <<",\"culled\":"<<(p.culled?"true":"false")<<",\"cull_reasons\":"<<p.cull_reasons<<",\"header\":";
        json_array(out,p.header);out<<",\"focus\":";json_array(out,p.focus);
        out<<",\"center\":";json_array(out,p.center);out<<",\"viewport\":";json_array(out,p.viewport);
        out<<",\"matrix\":";json_array(out,p.matrix);out<<",\"vertices\":[";
        for(unsigned i=0;i<p.count;i++) {
            if(i)out<<',';const auto& v=p.vertices[i];
            out<<'['<<v.x<<','<<v.y<<','<<v.z<<','<<v.u<<','<<v.v<<']';
        }
        out<<"]}";
    }
    out<<"]}\n";out.close();
    trace_pending_raster()=destination;
}

inline void commit_trace_rasters() {
    const auto& path=trace_pending_raster();
    if(path.empty())throw std::runtime_error("No pending native raster capture");
    for(bool written:trace_rasters_written())if(!written)throw std::runtime_error("Native raster capture is incomplete");
    std::filesystem::rename(path/"metadata.partial",path/"metadata.json");
}

template<class ReadPixel> inline void trace_raster(const char* name,int width,int height,ReadPixel read) {
    if(trace_pending_raster().empty())return;
    if(!name)throw std::invalid_argument("Missing native raster name");
    const std::string key=name;
    const int slot=key=="background"?0:key=="foreground"?1:key=="native"?2:-1;
    if(slot<0)throw std::invalid_argument("Unknown native raster name");
    if(width<1||height<1||width>2048||height>2048)throw std::invalid_argument("Invalid raster snapshot");
    // Canonical little-endian ARGB32 bytes, independent of host integer layout.
    std::vector<uint8_t> bytes(size_t(width)*height*4);
    for(int y=0;y<height;y++)for(int x=0;x<width;x++) {
        const uint32_t pixel=read(x,y);const size_t at=(size_t(y)*width+x)*4;
        for(int c=0;c<4;c++)bytes[at+c]=uint8_t(pixel>>(c*8));
    }
    std::ofstream out(trace_pending_raster()/(std::string(name)+"-argb32.bin"),std::ios::binary);
    out.exceptions(std::ios::failbit|std::ios::badbit);
    out.write(reinterpret_cast<const char*>(bytes.data()),bytes.size());out.close();
    trace_rasters_written()[slot]=true;
}
}
