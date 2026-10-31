// SPDX-License-Identifier: GPL-2.0-or-later
// Headless ARM64 emulation diagnostic. This does not create an OpenXR session.
#include <dlfcn.h>
#include <cstdlib>
#include <cstdio>

extern "C" const char* SDL_AndroidGetExternalStoragePath() { return "/data/local/tmp/vcop2-bench"; }
extern "C" const char* SDL_AndroidGetInternalStoragePath() { return "/data/local/tmp/vcop2-bench"; }
extern "C" void* SDL_AndroidGetJNIEnv() { return nullptr; }

int main(int argc,char** argv) {
    setenv("SDL_VIDEODRIVER","dummy",1);
    setenv("SDL_AUDIODRIVER","dummy",1);
    if(!std::getenv("VCOP2_BENCH_CAPTURE"))unsetenv("VCOP2_VR");
    const char* name=std::getenv("VCOP2_BENCH_LIBRARY");
    void* library=dlopen(name?name:"./libmain.so",RTLD_NOW|RTLD_GLOBAL);
    if(!library) { std::fprintf(stderr,"Cannot load benchmark engine: %s\n",dlerror()); return 2; }
    auto ready=reinterpret_cast<void(*)()>(dlsym(library,"SDL_SetMainReady"));
    auto run=reinterpret_cast<int(*)(int,char**)>(dlsym(library,"SDL_main"));
    if(!ready || !run) { std::fprintf(stderr,"Engine is missing SDL entry points\n"); return 3; }
    ready();
    return run(argc,argv);
}
