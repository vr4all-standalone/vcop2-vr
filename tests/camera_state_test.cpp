#include "../native/camera_state.h"
#include <cmath>
#include <iostream>
#include <limits>
#include <unordered_map>
#include <fstream>
#include <iterator>
#include <vector>
#define CHECK(x) do {if(!(x)){std::cerr<<"Failed " #x " line "<<__LINE__<<'\n';return 1;}}while(0)
int main(int argc,char** argv) {
    using namespace vcop2;
    std::unordered_map<uint32_t,uint32_t> memory{{0x500030,5u<<16}};
    auto read=[&](uint32_t address){return memory[address];};
    CHECK(camera_world_phase(read));
    memory[0x500030]=8u<<16;memory[0x500038]=8;CHECK(camera_world_phase(read));
    memory[0x500038]=2;CHECK(!camera_world_phase(read));
    memory[0x500038]=1;CHECK(camera_world_phase(read));
    memory[0x500038]=255;CHECK(!camera_world_phase(read));memory[0x500038]=1;
    memory[0x5d051c]=1u<<8;CHECK(!camera_world_phase(read));
    memory[0x5d051c]=0;memory[0x500030]=1u<<16;CHECK(!camera_world_phase(read));
    const auto widen=[](uint32_t address,float value,float fov=110,bool active=true,uint32_t mask=0xffffffffu){
        return std::bit_cast<float>(widened_camera_write(address,std::bit_cast<uint32_t>(value),mask,fov,active));
    };
    const float focus=widen(CameraFocusAddress,1827.30969f);
    const float tangent=widen(CameraTangentAddress,.13415673375f);
    CHECK(std::abs(focus-173.65147f)<.001f);
    CHECK(std::abs(tangent-1.428148f)<.00001f);
    CHECK(std::abs(focus*tangent-248)<.0001f);
    // Native screen projection and its inverse agree at an off-centre ray.
    const float world_x=8,world_z=20;
    const float pixel=248+focus*world_x/world_z;
    CHECK(pixel>248 && pixel<496);
    CHECK(std::abs((pixel-248)*world_z/focus-world_x)<.00001f);
    // Existing wide views are preserved, and repeated writes do not compound.
    CHECK(widen(CameraFocusAddress,100)==100);
    CHECK(widen(CameraTangentAddress,2)==2);
    CHECK(widen(CameraFocusAddress,focus)==focus);
    CHECK(widen(CameraTangentAddress,tangent)==tangent);
    CHECK(widen(CameraFocusAddress,1200,110,false)==1200);
    CHECK(widen(CameraFocusAddress,1200,110,true,0xffff)==1200);
    CHECK(widen(CameraFocusAddress,1200,0)==1200);
    CHECK(widen(0x506034,38)==38); // Animated FOV source must remain native.
    CHECK(widen(CameraFocusAddress,0)==0);
    CHECK(std::isnan(widen(CameraFocusAddress,std::numeric_limits<float>::quiet_NaN())));
    // The native TGP uses an approximate trigonometric lookup. Its observed
    // 38-degree focal is within 0.03 percent of the analytical value.
    CHECK(std::abs(248.f/camera_target_tangent(38)-720.433777f)<.25f);
    CHECK(!camera_revision_matches(read));
    if(argc==2) {
        std::ifstream in(argv[1],std::ios::binary);
        std::vector<uint8_t> rom{std::istreambuf_iterator<char>(in),{}};
        CHECK(rom.size()==0x200000);
        auto actual=[&](uint32_t address){
            if(address>=rom.size())return read(address);
            return uint32_t(rom[address])|(uint32_t(rom[address+1])<<8)|(uint32_t(rom[address+2])<<16)|(uint32_t(rom[address+3])<<24);
        };
        CHECK(camera_revision_matches(actual));
        memory[0x500030]=8u<<16;memory[0x500038]=8;
        CHECK(std::bit_cast<float>(camera_write(CameraFocusAddress,std::bit_cast<uint32_t>(1827.3f),0xffffffffu,actual,110))==focus);
        rom[0x3a7d4]^=1;CHECK(!camera_revision_matches(actual));
        CHECK(camera_write(CameraFocusAddress,std::bit_cast<uint32_t>(1827.3f),0xffffffffu,actual,110)==std::bit_cast<uint32_t>(1827.3f));
    }
    std::cout<<"Native camera phase, reciprocal widening and projected aim checks passed\n";
}
