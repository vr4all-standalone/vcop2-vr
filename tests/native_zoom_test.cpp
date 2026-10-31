// SPDX-License-Identifier: GPL-2.0-or-later
#include "../native/native_zoom.h"
#include "../native/camera_state.h"
#include "../native/model2_capture.h"
#include <fstream>
#include <iostream>
#include <limits>
#include <unordered_map>
#define CHECK(x) do{if(!(x)){std::cerr<<"Failed " #x " line "<<__LINE__<<'\n';return 1;}}while(0)
int main(int argc,char**argv){
    using namespace vcop2;
    CHECK(native_zoom_factor(15.463606f,34,false)==1);
    CHECK(native_zoom_factor(34,34,true)==1);
    CHECK(native_zoom_factor(38,38,true)==1);
    const double zoom=native_zoom_factor(15.463606f,34,true);
    CHECK(zoom>2.24&&zoom<2.26);
    CHECK(native_zoom_factor(38,34,true)<1);
    // The balcony animation legitimately crosses five degrees and reaches
    // 4.0667696. Magnification must continue rather than fall back to 1x.
    double previous=0;
    for(float fov:{5.01f,5.f,4.99618006f,4.94726372f,4.0667696f,4.f,1.f}){
        const double factor=native_zoom_factor(fov,34,true);
        CHECK(factor>previous&&factor>6);
        CHECK(native_zoom_factor(fov,34,false)==1);
        previous=factor;
    }
    CHECK(native_zoom_factor(4.0667696f,34,true)>8.60&&native_zoom_factor(4.0667696f,34,true)<8.62);
    CHECK(native_zoom_factor(std::numeric_limits<float>::denorm_min(),34,true)==1);
    for(float invalid:{0.f,-1.f,151.f,std::numeric_limits<float>::infinity(),std::numeric_limits<float>::quiet_NaN()}){
        CHECK(native_zoom_factor(invalid,34,true)==1);
        CHECK(native_zoom_factor(15,invalid,true)==1);
    }
    Vec3 original{2,-3,-12};auto enlarged=zoomed_world_position(original,zoom);
    CHECK(std::abs(enlarged.x-original.x*zoom)<1e-10&&enlarged.z==original.z);
    auto recovered=unzoomed_world_position(enlarged,zoom);
    CHECK(std::abs(recovered.x-original.x)<1e-10&&std::abs(recovered.y-original.y)<1e-10&&recovered.z==original.z);
    CHECK(zoomed_world_position(original,1).x==original.x);
    NativeFrame recycled;recycled.native_fov=15;recycled.native_reference_fov=34;
    SceneCapture capture;capture.begin(1,0,std::move(recycled));auto fresh=capture.take();
    CHECK(fresh.native_fov==0&&fresh.native_reference_fov==0);
    capture.begin(2,1);capture.camera_zoom(15,34);auto frame=capture.take();
    CHECK(frame.native_fov==15&&frame.native_reference_fov==34);
    if(argc==2){
        std::ifstream in(argv[1],std::ios::binary);std::vector<uint8_t> rom{std::istreambuf_iterator<char>(in),{}};CHECK(rom.size()==0x200000);
        std::unordered_map<uint32_t,uint32_t> ram{{0x500030,8u<<16},{0x500038,8},{0x502010,0},{0x506034,std::bit_cast<uint32_t>(15.f)}};
        auto read=[&](uint32_t a){if(a<rom.size())return uint32_t(rom[a])|(uint32_t(rom[a+1])<<8)|(uint32_t(rom[a+2])<<16)|(uint32_t(rom[a+3])<<24);return ram[a];};
        auto z=read_native_camera_zoom(read);CHECK(z.fov==15&&z.reference_fov==34);
        for(float fov:{5.01f,4.99618006f,4.0667696f,1.f}){
            ram[0x506034]=std::bit_cast<uint32_t>(fov);
            z=read_native_camera_zoom(read);CHECK(z.fov==fov&&z.reference_fov==34);
        }
        ram[0x506034]=std::bit_cast<uint32_t>(15.f);
        ram[0x502010]=1;CHECK(read_native_camera_zoom(read).reference_fov==38);
        ram[0x502010]=7;CHECK(read_native_camera_zoom(read).fov==0);ram[0x502010]=0;
        ram[0x500038]=2;CHECK(read_native_camera_zoom(read).fov==0);ram[0x500038]=8;
        ram[0x5d051c]=1u<<8;CHECK(read_native_camera_zoom(read).fov==0);ram[0x5d051c]=0;
        ram[0x506034]=std::bit_cast<uint32_t>(std::numeric_limits<float>::quiet_NaN());CHECK(read_native_camera_zoom(read).fov==0);ram[0x506034]=std::bit_cast<uint32_t>(15.f);
        rom[0x1b890]^=1;CHECK(read_native_camera_zoom(read).fov==0);
    }
    std::cout<<"Optional native zoom, exact Off path, inverse mapping, snapshot reset and owned-ROM guards passed\n";
}
