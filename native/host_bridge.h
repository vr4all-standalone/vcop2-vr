// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "shared_runtime.h"
#include "camera_state.h"
#include "realtime_pacer.h"
#include "native_timing.h"
#include "modules/lib/osdlib.h"
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <fstream>
#ifdef __ANDROID__
#include <jni.h>
extern "C" void* SDL_AndroidGetJNIEnv(void);
extern "C" void* SDL_AndroidGetActivity(void);
#endif
namespace vcop2 {
inline bool vr_requested(){static const bool yes=std::getenv("VCOP2_VR")!=nullptr;return yes;}
class HostBridge {
    osd::dynamic_module::ptr module_;
    void(*stop_)(){};
public:
    HostBridge() {
        if(!vr_requested())return;
        if(const char* path=std::getenv("VCOP2_XR_LOG")) {
            auto stamp=std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
            for(const char* suffix:{".engine",".audio"}){std::ofstream out(std::string(path)+suffix,std::ios::app);out<<"Session start "<<stamp<<", ABI "<<SharedRuntime::Abi<<'\n';}
        }
        const char* name=std::getenv("VCOP2_XR_LIBRARY");
#ifdef _WIN32
        module_=osd::dynamic_module::open({name?name:"vcop2_xr.dll"});
#else
        module_=osd::dynamic_module::open({name?name:"libvcop2_xr.so"});
#endif
        auto start=module_->bind<bool(*)(SharedRuntime*)>("vcop2_xr_start");
        stop_=module_->bind<void(*)()>("vcop2_xr_stop");
#ifdef __ANDROID__
        auto init=module_->bind<void(*)(void*,void*)>("vcop2_xr_android_init");
        auto env=static_cast<JNIEnv*>(SDL_AndroidGetJNIEnv());JavaVM* vm{};
        if(env&&init) {env->GetJavaVM(&vm);auto activity=SDL_AndroidGetActivity();init(vm,activity);env->DeleteLocalRef(static_cast<jobject>(activity));}
#endif
        if(!start||!stop_||!start(&shared_runtime())) {
            std::cerr<<"Virtua Cop 2 VR: host could not start. Check the host library and log.\n";
            shared_runtime().quit_requested=true;
        }
    }
    ~HostBridge(){if(stop_)stop_();}
};
inline std::unique_ptr<HostBridge>& host_bridge_slot(){static std::unique_ptr<HostBridge> bridge;return bridge;}
inline void start_host(){auto& bridge=host_bridge_slot();if(!bridge){shared_runtime().reset();bridge=std::make_unique<HostBridge>();}}
inline void stop_host(){host_bridge_slot().reset();shared_runtime().reset();}
template<class Machine> void poll_host(Machine& machine) {
    if(!vr_requested())return;
    start_host();auto& runtime=shared_runtime();
    if(runtime.quit_requested)machine.schedule_exit();
    if(runtime.host_ready) {
        if(runtime.pause_requested&&!machine.paused()){runtime.release_inputs();machine.pause();}
        else if(!runtime.pause_requested&&machine.paused())machine.resume();
    }
    // Sample emulation time independently of captured video or headset refresh.
    using Clock=std::chrono::steady_clock;
    static NativeSpeedTracker speed;
    auto sample=speed.update(machine.time().as_double(),std::chrono::duration<double>(Clock::now().time_since_epoch()).count(),machine.paused());
    if(sample.ready) {
        auto frame=runtime.scene.load();
        const char* path=std::getenv("VCOP2_XR_LOG");
        if(path){
            GameState game;{std::lock_guard lock(runtime.mutex);game=runtime.state;}
            std::ofstream out(std::string(path)+".engine",std::ios::app);
            out<<"Native seconds "<<sample.native<<", speed "<<sample.percent<<"%, captured frame "<<(frame?frame->sequence:0)<<", paused "<<sample.paused
               <<", mode "<<game.mode<<'/'<<game.submode<<", game loops "<<game.native_game_loops<<", backlog events "<<game.native_backlog_events
               <<", pending vblanks "<<game.native_vblank_pending<<", video control "<<game.native_video_control<<'\n';
        }
        if(path&&native_timing_enabled()) {
            std::ofstream out(std::string(path)+".timing",std::ios::app);
            out<<"Native seconds "<<sample.native<<", speed "<<sample.percent<<"%, inclusive estimated section ms/calls/samples:";
            auto& counters=native_timing_counters();
            for(size_t i=0;i<counters.size();++i)out<<' '<<NativeSectionNames[i]<<'='<<counters[i].ns.exchange(0,std::memory_order_relaxed)/1e6<<'/'<<counters[i].calls.exchange(0,std::memory_order_relaxed)<<'/'<<counters[i].samples.exchange(0,std::memory_order_relaxed);
            out<<'\n';
        }
    }
}
inline CabinetInput cabinet_input(){auto& s=shared_runtime();std::lock_guard lock(s.mutex);return s.engine_input;}
inline bool native_panel_requested() {
    auto& s=shared_runtime();std::lock_guard lock(s.mutex);
    return s.state.valid&&(s.state.service||(s.state.mode==8&&s.state.submode==2));
}
inline uint8_t system_input(uint8_t native) {
    if(!vr_requested())return native;
    const auto in=cabinet_input();
    native&=~uint8_t(shared_runtime().operator_latched.load()&0x3c);
    if(in.coin_mask&1)native&=~1u;
    if(in.coin_mask&2)native&=~2u;
    if(in.start_mask&1)native&=~0x10u;
    if(in.start_mask&2)native&=~0x20u;
    using Clock=std::chrono::steady_clock;
    static Clock::time_point service_until{};
    if(shared_runtime().service_requested.exchange(false))service_until=Clock::now()+std::chrono::milliseconds(150);
    if(Clock::now()<service_until)native&=~4u;
    return native;
}
inline uint8_t trigger_input(uint8_t native) {
    if(!vr_requested())return native;
    auto in=cabinet_input();
    for(int p=0;p<2;p++)if(in.guns[p].trigger||in.guns[p].pump)native&=~uint8_t(1<<p);
    return native;
}
inline uint8_t lightgun_byte(unsigned offset) {
    auto& s=shared_runtime();std::lock_guard lock(s.mutex);
    const unsigned player=offset/4,axis=(offset%4)<2?1:0;
    if(player>1)return 0;
    const auto& gun=s.engine_input.guns[player];
    auto raw=calibrated_gun_position(gun.x,gun.y,s.state.calibration[player]);
    return uint8_t(raw[axis]>>((offset&1)*8));
}
inline uint8_t offscreen_input(unsigned offset,uint8_t native) {
    if(!vr_requested())return native;
    auto in=cabinet_input();unsigned bits=(in.guns[0].pump?1:0)|(in.guns[1].pump?2:0);
    return uint8_t((0xfffc|bits)>>((offset&1)*8));
}
template<class Program,class Polygon,class Texture> void native_weapons(Program program,Polygon polygon,Texture texture,uint32_t polygon_words,uint32_t texture_words) {
    if(!vr_requested())return;
    auto& s=shared_runtime();
    {std::lock_guard lock(s.mutex);if(!s.state.ready||s.weapons[1])return;}
    std::array<std::shared_ptr<const NativeWeapon>,7> weapons;
    for(unsigned weapon=1;weapon<=6;weapon++) {
        auto mesh=extract_native_weapon(program,polygon,texture,weapon,polygon_words,texture_words);
        if(!mesh.valid)return;
        weapons[weapon]=std::make_shared<const NativeWeapon>(std::move(mesh));
    }
    std::lock_guard lock(s.mutex);s.weapons=std::move(weapons);
}
template<class Read32> void native_tick(Read32 read,uint32_t video_control=0) {
    if(!vr_requested())return;
    auto& s=shared_runtime();std::lock_guard lock(s.mutex);
    s.state=read_game_state(read);
    s.state.native_video_control=video_control;
    s.engine_input=s.input_latch.update(s.input);
    s.operator_latched=s.operator_latch.update(s.operator_pending.exchange(0));
}
}
