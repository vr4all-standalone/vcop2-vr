// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "projection.h"
#include <array>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
namespace vcop2 {
struct NativeVertex {float x{},y{},z{},u{},v{};};
enum NativeCull : uint32_t {
    CullBackface=1, CullNonRendering=2, CullDepthLimit=4, CullBehind=8,
    CullKnown=0x80000000u
};
struct NativePolygon {
    unsigned count{},attr{},window{},sort{},luma{};int lod{};
    std::array<unsigned,4> header{};
    std::array<float,2> focus{},center{};
    std::array<int,4> viewport{};
    std::array<float,12> matrix{};
    std::array<NativeVertex,4> vertices;
    uint32_t object{},cull_reasons{};bool culled{};
};
// Copies share read-only payloads. Mutable access detaches before returning a
// writable reference/view; callers must not retain one across copying or
// publication. SceneCapture never exposes a mutable resource view.
class SharedBytes {
    std::shared_ptr<std::vector<uint8_t>> storage_;
    std::vector<uint8_t>& writable() {
        if(!storage_)storage_=std::make_shared<std::vector<uint8_t>>();
        else if(storage_.use_count()!=1)storage_=std::make_shared<std::vector<uint8_t>>(*storage_);
        return *storage_;
    }
public:
    SharedBytes()=default;
    SharedBytes(std::vector<uint8_t> bytes):storage_(std::make_shared<std::vector<uint8_t>>(std::move(bytes))){}
    SharedBytes(std::initializer_list<uint8_t> bytes):storage_(std::make_shared<std::vector<uint8_t>>(bytes)){}
    template<class It> SharedBytes(It first,It last):storage_(std::make_shared<std::vector<uint8_t>>(first,last)){}
    const std::vector<uint8_t>& vector() const {static const std::vector<uint8_t> empty;return storage_?*storage_:empty;}
    size_t size() const{return vector().size();}
    bool empty() const{return vector().empty();}
    const uint8_t* data() const{return vector().data();}
    uint8_t* data(){return writable().data();}
    auto begin() const{return vector().begin();}auto end() const{return vector().end();}
    auto cbegin() const{return vector().cbegin();}auto cend() const{return vector().cend();}
    auto begin(){return writable().begin();}auto end(){return writable().end();}
    const uint8_t& operator[](size_t n) const{return vector()[n];}uint8_t& operator[](size_t n){return writable()[n];}
    const uint8_t& front() const{return vector().front();}uint8_t& front(){return writable().front();}
    const uint8_t& back() const{return vector().back();}uint8_t& back(){return writable().back();}
    template<class It> void assign(It first,It last) {
        // Replacement never clones the old shared bytes before overwriting them.
        if(storage_&&storage_.use_count()==1)storage_->assign(first,last);
        else storage_=std::make_shared<std::vector<uint8_t>>(first,last);
    }
    friend bool operator==(const SharedBytes& a,const SharedBytes& b){return a.storage_==b.storage_||a.vector()==b.vector();}
    friend bool operator==(const SharedBytes& a,const std::vector<uint8_t>& b){return a.vector()==b;}
    friend bool operator==(const std::vector<uint8_t>& a,const SharedBytes& b){return a==b.vector();}
};
struct Blob {std::string name;SharedBytes bytes;};
inline bool resource_cache_enabled(){static const bool enabled=std::getenv("VCOP2_NO_RESOURCE_CACHE")==nullptr;return enabled;}
struct NativeFrame {
    uint64_t sequence{};double time{};
    int xoffset{},yoffset{},width{},height{};
    bool complete{true},raster_only{};
    NativeLens lens;
    float native_fov{},native_reference_fov{};
    std::vector<uint32_t> background,foreground,native;
    int sky_width{},sky_height{};
    std::vector<uint32_t> sky;
    unsigned rejected{};
    std::vector<NativePolygon> polygons;
    std::vector<Blob> blobs;
};
// Published snapshots have a fresh shared_ptr control block. Only its final
// strong-owner deleter may retire the payload; weak readers therefore expire
// normally even when a later snapshot reuses the same backing allocations.
class NativeFramePool {
public:
    static constexpr size_t MaxCached=3;
private:
    struct State {
        std::mutex mutex;
        std::array<std::unique_ptr<NativeFrame>,MaxCached> retired;
        size_t count{};
    };
    std::shared_ptr<State> state_=std::make_shared<State>();
public:
    NativeFrame acquire() {
        std::unique_ptr<NativeFrame> frame;
        {std::lock_guard lock(state_->mutex);if(state_->count)frame=std::move(state_->retired[--state_->count]);}
        return frame?std::move(*frame):NativeFrame{};
    }
    std::shared_ptr<const NativeFrame> publish(std::unique_ptr<NativeFrame> frame) {
        if(!frame)return {};
        return {frame.release(),[state=state_](NativeFrame* raw) noexcept {
            std::unique_ptr<NativeFrame> retired(raw);
            {std::lock_guard lock(state->mutex);if(state->count<MaxCached)state->retired[state->count++]=std::move(retired);}
            // If full, release the large payload after dropping the pool lock.
        }};
    }
    size_t cached_count() const {std::lock_guard lock(state_->mutex);return state_->count;}
};
// This collector owns every byte. The producer keeps no mutable reference to
// the published frame, so an eye pass can safely retain its previous snapshot.
class SceneCapture {
    NativeFrame frame_;
    std::vector<Blob> previous_resources_;
    size_t total_bytes_{},used_blobs_{};
public:
    static constexpr unsigned MaxPolygons=32768,MaxBlobBytes=4*1024*1024;
    static constexpr unsigned MaxFrameBytes=16*1024*1024,MaxBlobs=16;
    void begin(uint64_t sequence,double time,NativeFrame recycled={}) {
        if(!std::isfinite(time)||time<0)throw std::invalid_argument("Invalid native time");
        frame_=std::move(recycled);frame_.sequence=sequence;frame_.time=time;
        frame_.xoffset=frame_.yoffset=frame_.width=frame_.height=0;
        frame_.complete=true;frame_.raster_only=false;frame_.lens={};
        frame_.native_fov=frame_.native_reference_fov=0;
        frame_.background.clear();frame_.foreground.clear();frame_.native.clear();
        frame_.sky_width=frame_.sky_height=0;frame_.sky.clear();
        frame_.rejected=0;frame_.polygons.clear();total_bytes_=used_blobs_=0;
        // Keep old blob allocations private until overwritten below. take()
        // discards the unused suffix so no stale resource can be published.
    }
    void display(int xoffset,int yoffset,int width,int height) {
        if(width<1||height<1||width>2048||height>2048)throw std::invalid_argument("Invalid native display");
        frame_.xoffset=xoffset;frame_.yoffset=yoffset;frame_.width=width;frame_.height=height;
    }
    bool append(const NativePolygon& polygon) {
        bool valid=polygon.count==3||polygon.count==4;
        for(unsigned i=0;valid&&i<polygon.count;i++) {
            const auto& v=polygon.vertices[i];
            valid=std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z)&&std::isfinite(v.u)&&std::isfinite(v.v);
        }
        for(float value:polygon.focus)valid=valid&&std::isfinite(value);
        for(float value:polygon.center)valid=valid&&std::isfinite(value);
        for(float value:polygon.matrix)valid=valid&&std::isfinite(value);
        if(!valid||frame_.polygons.size()>=MaxPolygons){++frame_.rejected;return false;}
        frame_.polygons.push_back(polygon);return true;
    }
    void bytes(const char* name,const void* data,size_t length) {
        if(!name||!data||!length||length>MaxBlobBytes||used_blobs_>=MaxBlobs||length>MaxFrameBytes-total_bytes_)
            throw std::invalid_argument("Invalid native snapshot size");
        std::string key=name;
        if(key.empty()||key.size()>64||key.find_first_not_of("abcdefghijklmnopqrstuvwxyz0123456789_-")!=std::string::npos)
            throw std::invalid_argument("Invalid native snapshot name");
        for(size_t i=0;i<used_blobs_;++i)if(frame_.blobs[i].name==key)throw std::invalid_argument("Duplicate native snapshot name");
        const auto* first=static_cast<const uint8_t*>(data);
        if(used_blobs_==frame_.blobs.size())frame_.blobs.emplace_back();
        else for(size_t i=used_blobs_;i<frame_.blobs.size();++i)if(frame_.blobs[i].name==key){if(i!=used_blobs_)std::swap(frame_.blobs[used_blobs_],frame_.blobs[i]);break;}
        auto& blob=frame_.blobs[used_blobs_];blob.name=std::move(key);
        const SharedBytes* unchanged=nullptr;
        if(resource_cache_enabled())for(const auto& previous:previous_resources_)
            if(previous.name==blob.name&&previous.bytes.size()==length&&std::memcmp(previous.bytes.data(),first,length)==0){unchanged=&previous.bytes;break;}
        if(unchanged)blob.bytes=*unchanged;else blob.bytes.assign(first,first+length);
        ++used_blobs_;total_bytes_+=length;
    }
    void camera_zoom(float fov,float reference){frame_.native_fov=fov;frame_.native_reference_fov=reference;}
    NativeFrame take() {
        frame_.blobs.resize(used_blobs_);
        // Own only this capture's resource set: no frame references, retained
        // generations or omitted/stale resources in a later publication.
        if(resource_cache_enabled())previous_resources_=frame_.blobs;else previous_resources_.clear();
        auto frame=std::move(frame_);frame_={};total_bytes_=used_blobs_=0;return frame;
    }
    std::shared_ptr<const NativeFrame> finish() {return std::make_shared<const NativeFrame>(take());}
};
}
