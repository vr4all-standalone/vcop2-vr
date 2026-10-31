#include "../native/game_state.h"
#include "../native/native_weapon.h"
#include <filesystem>
#include <vector>
#include <iterator>
#include <iostream>
#include <unordered_map>
#include <fstream>
#define CHECK(value) do {if(!(value)){std::cerr<<"Failed: " #value " line "<<__LINE__<<'\n';return 1;}}while(0)
int main(int argc,char** argv) {
    using namespace vcop2;
    std::unordered_map<uint32_t,uint32_t> words{
        {0x2720,0x80183000},{0x2724,0x00500032},{0x19954,0x8c883000},{0x19958,0x00507260},
        {0x19988,0x8c883000},{0x1998c,0x00507264},{0x19b6c,0x59210981},{0x19b70,0x92245000},
        {0x37c30,0x90703000},{0x37c34,0x0050a2a4},{0x37c7c,0x90703000},{0x37c80,0x0050a2a8},
        {0x530b4,0x82a83000},{0x530b8,0x005d051d},{0x5313c,0x80a03000},{0x53140,0x005d051d}};
    auto read=[&](uint32_t a){return words[a];};
    CHECK(read_game_state(read).valid);CHECK(!read_game_state(read).ready);
    {
        auto diagnostic=read_game_state(read);
        CHECK(diagnostic.native_game_loops==0&&diagnostic.native_backlog_events==0&&diagnostic.native_vblank_pending==0&&diagnostic.native_video_control==0);
        // Older sparse diagnostic fixtures omit these words. Their zero values
        // remain harmless, and arbitrary counters never gate cabinet controls.
        words[0x500028]=0xffffffff;words[0x500020]=0xfedcba98;words[0x500000]=0xa1b2c3fe;
        diagnostic=read_game_state(read);
        CHECK(diagnostic.valid&&!diagnostic.ready&&!diagnostic.playing);
        CHECK(diagnostic.native_game_loops==0xffffffff&&diagnostic.native_backlog_events==0xfedcba98&&diagnostic.native_vblank_pending==0xfe&&diagnostic.native_video_control==0);
        words[0x500028]=words[0x500020]=words[0x500000]=0;
    }
    words[0x500030]=5<<16;words[0x502088]=1;
    auto state=read_game_state(read);CHECK(state.ready);CHECK(!state.playing);CHECK(!state.players[0].joined());
    words[0x500030]=8<<16;words[0x500038]=8;
    for(unsigned i=0;i<2;i++) {words[0x502088+i*4]=1;words[0x502090+i*4]=4;words[0x507260+i*4]=6;words[0x507268+i*4]=6;words[0x507270+i*4]=1;}
    state=read_game_state(read);CHECK(state.playing&&state.counting_shots);CHECK(state.players[0].joined()&&state.players[1].alive());
    words[0x500028]=127;words[0x500020]=8;words[0x500000]=3;auto diagnostic=read_game_state(read);
    CHECK(diagnostic.native_game_loops==127&&diagnostic.native_backlog_events==8&&diagnostic.native_vblank_pending==3);
    CHECK(diagnostic.valid&&diagnostic.ready&&diagnostic.playing&&diagnostic.counting_shots&&diagnostic.players[0].alive()&&diagnostic.players[1].alive());
    words[0x500028]=words[0x500020]=words[0x500000]=0;
    words[0x507264]=0;words[0x50a2a8]=6;state=read_game_state(read);CHECK(state.players[1].ammo==0&&state.players[1].shots==6);
    words[0x50208c]=2;words[0x502094]=0;state=read_game_state(read);CHECK(state.players[1].joined()&&!state.players[1].alive());
    words[0x5d051c]=0x100;words[0x5d0518]=9;state=read_game_state(read);CHECK(state.service&&!state.gun_calibration);CHECK(!state.ready&&!state.playing&&!state.players[0].joined());
    words[0x5d051c]=0x101;words[0x5d0654]=2;state=read_game_state(read);CHECK(state.gun_calibration&&state.service_cursor==1&&state.service_busy==2);
    words[0x5d0654]=0;CHECK(!read_game_state(read).gun_calibration);
    words[0x2720]^=1;CHECK(!read_game_state(read).valid);words[0x2720]^=1;
    words[0x507264]=7;CHECK(!read_game_state(read).valid);words[0x507264]=0;
    GunCalibration c{137,630,36,425};CHECK(c.valid());
    CHECK(native_gun_axis(384,137,630,496)==248);CHECK(native_gun_axis(230,36,425,384)==191);
    auto center=calibrated_gun_position(32768,32768,c);CHECK(center[0]==384&&center[1]==231);
    for(unsigned n=0;n<=65535;n+=257) {
        auto pos=calibrated_gun_position(uint16_t(n),uint16_t(n),c);
        CHECK(std::abs(native_gun_axis(pos[0],137,630,496)-double(n)*495/65535)<1.1);
        CHECK(std::abs(native_gun_axis(pos[1],36,425,384)-double(n)*383/65535)<1.1);
    }
    // Optional real diagnostic RAM fixture: includes native data, never ships.
    auto load_ram=[&](const std::filesystem::path& path){
        std::ifstream input(path,std::ios::binary);if(!input.good())throw std::runtime_error("Missing private RAM fixture");
        for(uint32_t a=0x500000;a<0x600000;a+=4){uint8_t b[4]{};input.read(reinterpret_cast<char*>(b),4);if(!input.good())throw std::runtime_error("Incomplete private RAM fixture");words[a]=uint32_t(b[0])|(uint32_t(b[1])<<8)|(uint32_t(b[2])<<16)|(uint32_t(b[3])<<24);}
        return read_game_state(read);
    };
    for(int f=1;f<argc;f++) {
        if(std::string(argv[f])=="--cadence") {
            CHECK(f+1<argc);const std::filesystem::path root=argv[++f];
            auto first=load_ram(root/"frame-00001439"/"workram.bin");
            auto active=load_ram(root/"frame-00001727"/"workram.bin");
            auto later=load_ram(root/"frame-00002015"/"workram.bin");
            CHECK(first.valid&&active.valid&&later.valid&&active.playing&&later.playing&&active.submode==8&&later.submode==8);
            CHECK(first.native_game_loops==1329&&active.native_game_loops==1617&&later.native_game_loops==1905);
            CHECK(active.native_game_loops-first.native_game_loops==288&&later.native_game_loops-active.native_game_loops==288);
            CHECK(first.native_backlog_events==4&&active.native_backlog_events==4&&later.native_backlog_events==4);
            CHECK(first.native_vblank_pending==0&&active.native_vblank_pending==0&&later.native_vblank_pending==0);
            CHECK(first.native_video_control==0&&active.native_video_control==0&&later.native_video_control==0);
            continue;
        }
        if(std::string(argv[f])=="--service-phases") {
            CHECK(f+1<argc);const std::filesystem::path root=argv[++f];
            auto navigation=load_ram(root/"workram-025.bin");CHECK(navigation.valid&&navigation.service&&navigation.service_menu==9&&navigation.service_cursor==0&&!navigation.gun_calibration);
            auto aiming=load_ram(root/"workram-028.bin");CHECK(aiming.valid&&aiming.gun_calibration&&aiming.service_cursor==1&&aiming.service_busy==2);
            auto returned=load_ram(root/"workram-031.bin");CHECK(returned.valid&&returned.service_menu==9&&returned.service_cursor==1&&!returned.gun_calibration);
            auto top=load_ram(root/"workram-041.bin");CHECK(top.valid&&top.service&&top.service_menu==0&&!top.gun_calibration);
            continue;
        }
        if(std::string(argv[f])=="--weapons") {
            CHECK(f+1<argc);const std::filesystem::path root=argv[++f];
            auto bytes=[&](const char* name){std::ifstream in(root/name,std::ios::binary);return std::vector<uint8_t>(std::istreambuf_iterator<char>(in),{});};
            const auto program=bytes("program.bin"),table=bytes("modeltable.bin"),polygons=bytes("polygons.bin"),textures=bytes("textures.bin");
            CHECK(program.size()==0x200000&&table.size()==0x40000&&polygons.size()==0x400000&&textures.size()==0x400000);
            auto u32=[](const auto& b,uint32_t a){if(a+4>b.size())throw std::out_of_range("ROM read");return uint32_t(b[a])|(uint32_t(b[a+1])<<8)|(uint32_t(b[a+2])<<16)|(uint32_t(b[a+3])<<24);};
            auto cpu=[&](uint32_t a){return a>=0x200000?u32(table,a-0x200000):u32(program,a);};
            auto poly=[&](uint32_t a){return u32(polygons,a*4);};
            auto tex=[&](uint32_t a){if(a*2+2>textures.size())throw std::out_of_range("Texture read");return uint16_t(textures[a*2])|(uint16_t(textures[a*2+1])<<8);};
            constexpr unsigned counts[]={0,49,52,87,59,43,104};
            for(unsigned w=1;w<=6;++w) {
                const auto mesh=extract_native_weapon(cpu,poly,tex,w,polygons.size()/4,textures.size()/2);
                CHECK(mesh.valid&&mesh.weapon==w&&mesh.polygons.size()==counts[w]);
                if(w==1) {
                    CHECK(mesh.model==0x143&&mesh.object==0x814fa7);
                    std::array<float,3> lo{1e6f,1e6f,1e6f},hi{-1e6f,-1e6f,-1e6f};
                    for(const auto& face:mesh.polygons)for(unsigned v=0;v<face.count;++v) {
                        const auto& p=face.vertices[v];const std::array<float,3> xyz{p.x,p.y,p.z};
                        for(unsigned axis=0;axis<3;++axis){lo[axis]=std::min(lo[axis],xyz[axis]);hi[axis]=std::max(hi[axis],xyz[axis]);}
                    }
                    CHECK(std::abs((hi[0]-lo[0])-.257f)<.00001f);
                    CHECK(std::abs((hi[1]-lo[1])-.132f)<.00001f);
                    CHECK(std::abs((hi[2]-lo[2])-.090f)<.00001f);
                    auto mismatched_scene=[&](uint32_t a){return a==0x31604?0u:cpu(a);};
                    CHECK(!extract_native_weapon(mismatched_scene,poly,tex,1,polygons.size()/4,textures.size()/2).valid);
                    CHECK(extract_native_weapon(mismatched_scene,poly,tex,2,polygons.size()/4,textures.size()/2).valid);
                }
                CHECK(mesh.object&0x800000);
                CHECK(!extract_native_weapon(cpu,poly,tex,w,10,textures.size()/2).valid);
            }
            continue;
        }
        std::ifstream input(argv[f],std::ios::binary);CHECK(input.good());
        for(uint32_t a=0x500000;a<0x600000;a+=4) {uint8_t b[4]{};input.read(reinterpret_cast<char*>(b),4);CHECK(input.good());words[a]=uint32_t(b[0])|(uint32_t(b[1])<<8)|(uint32_t(b[2])<<16)|(uint32_t(b[3])<<24);}
        CHECK(read_game_state(read).valid);
    }
    std::cout<<"vcop2 native state, revision rejection, join, service and exact calibration checks passed\n";
}
