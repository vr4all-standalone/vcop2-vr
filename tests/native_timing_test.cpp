#include "../native/native_timing.h"
#include <iostream>
#include <string_view>
#ifndef EXPECT_TIMING_AVAILABLE
#define EXPECT_TIMING_AVAILABLE 0
#endif
#define CHECK(x) do {if(!(x)){std::cerr<<"Failed " #x " line "<<__LINE__<<'\n';return 1;}}while(0)
static_assert(vcop2::NativeTimingAvailable==bool(EXPECT_TIMING_AVAILABLE));
int main(int argc,char** argv) {using namespace vcop2;
    const bool enabled=NativeTimingAvailable && argc>1 && std::string_view(argv[1])=="on";
    CHECK(native_timing_enabled()==enabled);
    for(auto& counter:native_timing_counters()){counter.ns=0;counter.calls=0;counter.samples=0;}
    std::atomic<unsigned> work{};
    for(size_t section=0;section<size_t(NativeSection::Count);++section) {
        const auto kind=NativeSection(section);
        for(unsigned call=0;call<513;++call) {
            NativeTimingScope timer(kind);
            for(unsigned n=0;n<64;++n)work.fetch_add(1,std::memory_order_relaxed);
        }
        const auto& counter=native_timing_counters()[section];
        CHECK(counter.calls.load()==(enabled?513u:0u));
        CHECK(counter.samples.load()==(enabled?(section<4?2u:513u):0u));
        if(enabled) {
            CHECK(counter.ns.load()>0);
            CHECK(counter.ns.load()%native_timing_stride(kind)==0);
        } else CHECK(counter.ns.load()==0);
    }
    CHECK(work.load()==513*64*size_t(NativeSection::Count));
    std::cout<<"Native timing build gate, runtime gate, real calls and sampling passed\n";
}
