// SPDX-License-Identifier: GPL-2.0-or-later
// Private-input real-GPU diagnostic. Geometry/resources are never bundled.
#define main replay_reference_main
#include "../tools/replay_model2_frames.cpp"
#undef main
#include <chrono>
int main(int argc,char** argv)try {
    if(argc!=3)return 2;
    auto a=read_frame(argv[1],argv[2]),b=std::make_shared<NativeFrame>(*a);
    GLContext context;
    std::cout<<"GPU "<<glGetString(GL_RENDERER)<<"; polygons "<<a->polygons.size()<<'\n';
    for(int mode:{0,1,2,0,1,2}) {
        GLScene scene;scene.set_immersive(true);
        if(mode==1){b->blobs[2].bytes.back()^=1;b->foreground.back()^=1;b->background.back()^=1;}
        for(int n=0;n<10;n++){if(mode==2)scene.prepare({});scene.prepare(n&1?a:b);glFinish();}
        auto before=scene.upload_stats();auto start=std::chrono::steady_clock::now();
        constexpr int frames=100;
        for(int n=0;n<frames;n++){if(mode==2)scene.prepare({});scene.prepare(n&1?a:b);glFinish();}
        auto ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count()/frames;
        auto after=scene.upload_stats();
        std::cout<<(mode==0?"unchanged":mode==1?"palette+foreground+sky changed":"forced resource reupload")<<"; prepare+finish ms "<<ms<<"; upload bytes/frame "<<(after.bytes-before.bytes)/frames<<'\n';
        if(mode==1){b->blobs[2].bytes.back()^=1;b->foreground.back()^=1;b->background.back()^=1;}
    }
    return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
