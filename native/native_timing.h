// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <array>
#include <atomic>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#if __has_include("native_timing_config.h")
#include "native_timing_config.h"
#endif
#ifndef VCOP2_NATIVE_TIMING_AVAILABLE
#define VCOP2_NATIVE_TIMING_AVAILABLE 0
#endif
namespace vcop2 {
inline constexpr bool NativeTimingAvailable=VCOP2_NATIVE_TIMING_AVAILABLE!=0;
enum class NativeSection {I960,GeometryDSP,SoundCPU,SCSP,GeometryParse,ScreenUpdate,PaletteUpdate,BackgroundTiles,CaptureBlobs,RasterCopies,ForegroundTiles,Count};
inline constexpr std::array<const char*,size_t(NativeSection::Count)> NativeSectionNames{
    "i960","geometry-dsp","sound-cpu","scsp","geometry-parse","screen-update","palette-update","background-tiles","capture-blobs","raster-copies","foreground-tiles"};
inline bool native_timing_enabled() {
    if constexpr(NativeTimingAvailable) {
        static const bool enabled=std::getenv("VCOP2_NATIVE_TIMING")!=nullptr;return enabled;
    } else return false;
}
constexpr uint64_t native_timing_stride(NativeSection section) {
    return section<=NativeSection::SCSP?256u:1u;
}
// ns is the inclusive estimated time (sampled elapsed time * sample stride).
// calls counts every enabled invocation; samples records actual clock pairs.
struct NativeTimingCounter {std::atomic<uint64_t> ns{},calls{},samples{};};
inline auto& native_timing_counters(){static std::array<NativeTimingCounter,size_t(NativeSection::Count)> counters;return counters;}
class NativeTimingScope {
    using Clock=std::chrono::steady_clock;
    NativeSection section_;bool active_{};Clock::time_point begin_{};
public:
    explicit NativeTimingScope(NativeSection section):section_(section) {
        if constexpr(NativeTimingAvailable)if(native_timing_enabled()) {
            auto& counter=native_timing_counters()[size_t(section_)];
            const auto call=counter.calls.fetch_add(1,std::memory_order_relaxed)+1;
            active_=call%native_timing_stride(section_)==0;
            if(active_)begin_=Clock::now();
        }
    }
    ~NativeTimingScope() {
        if constexpr(NativeTimingAvailable)if(active_) {
            const auto elapsed=std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now()-begin_).count();
            auto& counter=native_timing_counters()[size_t(section_)];
            counter.ns.fetch_add(uint64_t(elapsed)*native_timing_stride(section_),std::memory_order_relaxed);
            counter.samples.fetch_add(1,std::memory_order_relaxed);
        }
    }
    NativeTimingScope(const NativeTimingScope&)=delete;
    NativeTimingScope& operator=(const NativeTimingScope&)=delete;
};
}
