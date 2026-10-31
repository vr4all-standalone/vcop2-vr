#include "../native/gl_context.h"
#include "../native/presentation.h"
#include "../native/startup_policy.h"
#include "../native/shared/glb_mesh.h"
#include <glad/gl.h>
#include <glm/gtc/matrix_transform.hpp>
#include <cstdlib>
#include <iostream>
#include <fstream>
#include <filesystem>
#include <vector>
using namespace vcop2;
static void check(bool ok,const char* why){if(!ok){std::cerr<<"FAIL: "<<why<<'\n';std::exit(1);}}
#include "arcade_contract_probe.h"
int main(int argc,char** argv){
    if(argc!=2&&argc!=4)return 2;_putenv_s("VCOP2_ASSETS",argv[1]);
    for(const char* name:{"models/controllers/left.glb","models/controllers/right.glb"}) {
        std::ifstream in(std::filesystem::path(argv[1])/name,std::ios::binary);
        std::vector<uint8_t> bytes{std::istreambuf_iterator<char>(in),std::istreambuf_iterator<char>()};
        vr::xr::GlbMesh mesh;std::string error;
        check(!bytes.empty()&&vr::xr::loadGlb(bytes.data(),bytes.size(),mesh,error,8,1024),"bundled controller GLBs load through the production loader");
        check(mesh.positions.size()>300&&!mesh.images.empty(),"controller GLBs contain their original geometry and textures");
    }
    GLContext context;Presentation ui;Settings settings;SharedRuntime runtime;PresentationInput input;
    ui.update(input,settings,runtime,0);input.toggle=true;ui.update(input,settings,runtime,.1);
    check(ui.open(),"menu opens while the arcade engine is loading");
    GLuint image{},fbo{},depth{};glGenTextures(1,&image);glBindTexture(GL_TEXTURE_2D,image);
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,256,256,0,GL_RGBA,GL_UNSIGNED_BYTE,nullptr);
    glGenFramebuffers(1,&fbo);glBindFramebuffer(GL_FRAMEBUFFER,fbo);glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,image,0);
    glGenRenderbuffers(1,&depth);glBindRenderbuffer(GL_RENDERBUFFER,depth);glRenderbufferStorage(GL_RENDERBUFFER,GL_DEPTH_COMPONENT24,256,256);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,GL_RENDERBUFFER,depth);
    check(glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE,"presentation probe framebuffer is complete");
    glViewport(0,0,256,256);auto vp=glm::perspective(glm::radians(70.f),1.f,.05f,100.f);
    probe_arcade_contract(vp,false);
    {
        Presentation menu;PresentationInput drive;Settings options;SharedRuntime isolated;
        options.gun=1;menu.update(drive,options,isolated,0);drive.toggle=true;menu.update(drive,options,isolated,.1);
        drive.toggle=false;menu.update(drive,options,isolated,.2);check(menu.open()&&!options.game_zoom,"Game zoom starts Off in the actual settings menu");
        for(int row=0;row<18;row++){drive.stick={0,-1};menu.update(drive,options,isolated,1+row*2);drive.stick={0,0};menu.update(drive,options,isolated,2+row*2);}
        drive.stick={1,0};menu.update(drive,options,isolated,40);check(options.game_zoom,"Game zoom row turns native camera zoom On");
        drive.stick={0,0};menu.update(drive,options,isolated,41);drive.stick={1,0};menu.update(drive,options,isolated,42);
        check(!options.game_zoom&&options.gun==1&&options.world_scale==1&&options.gun_angle==0,"turning zoom Off preserves the accepted gun and world settings");
    }
    auto render=[&](bool splash){
        glClearColor(0,0,0,1);glClear(GL_COLOR_BUFFER_BIT);ui.draw(vp,settings,GameState{},false,.2,splash);
        std::vector<uint8_t> pixels(256*256*4);glReadPixels(0,0,256,256,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data());return pixels;
    };
    auto brand=render(true),menu=render(false);size_t changed=0;
    for(size_t i=0;i<brand.size();i+=4)if(brand[i]!=menu[i]||brand[i+1]!=menu[i+1]||brand[i+2]!=menu[i+2])++changed;
    check(changed>1000,"after the timed splash, an open loading menu must draw over the brand image");
    {
        NativeFrame credits;credits.width=496;credits.height=384;credits.native.assign(496*384,0xff000000);
        GameState boot;boot.valid=true;boot.mode=1;boot.ready=false;
        check(native_picture_available(&credits)&&!boot.ready,"native intro credits are visually ready before cabinet Start is accepted");
        credits.complete=false;check(!native_picture_available(&credits),"partial boot pictures keep loading available");credits.complete=true;
        Presentation opening;PresentationInput no_hands;runtime.state=boot;opening.update(no_hands,settings,runtime,.25);
        glClearColor(.1f,.2f,.3f,1);glDepthMask(GL_TRUE);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
        std::vector<uint8_t> before(256*256*4),after(before.size());glReadPixels(0,0,256,256,GL_RGBA,GL_UNSIGNED_BYTE,before.data());
        opening.draw(vp,settings,boot,boot.ready,.25,false,&credits);
        glReadPixels(0,0,256,256,GL_RGBA,GL_UNSIGNED_BYTE,after.data());check(before==after,"timed splash completion leaves native boot credits unobscured despite Start not ready");
        opening.draw(vp,settings,boot,boot.ready,.25,true,&credits);
        glReadPixels(0,0,256,256,GL_RGBA,GL_UNSIGNED_BYTE,after.data());check(before!=after,"the explicit launch splash still appears during its focused presentation interval");
        runtime.state={};
    }
    {
        Presentation instructions;PresentationInput hands;Settings help;GameState game;game.valid=game.ready=true;
        for(int h=0;h<2;h++){hands.hands[h].tracked=true;hands.poses[h]=glm::translate(glm::mat4(1),glm::vec3(h==0?-.17f:.17f,0,-.6f));}
        instructions.update(hands,help,runtime,.3);
        glDepthMask(GL_TRUE);glClearColor(0,0,0,1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
        instructions.draw(vp,help,game,true,.3);
        std::vector<uint8_t> pixels(256*256*4);std::vector<float> depths(256*256);
        glReadPixels(0,0,256,256,GL_RGBA,GL_UNSIGNED_BYTE,pixels.data());glReadPixels(0,0,256,256,GL_DEPTH_COMPONENT,GL_FLOAT,depths.data());
        size_t lit[2]{},labels[2]{};
        for(int y=0;y<256;y++)for(int x=0;x<256;x++) {
            size_t i=y*256+x;int h=x/128;
            if(depths[i]<.999f&&(pixels[i*4]||pixels[i*4+1]||pixels[i*4+2]))lit[h]++;
            if(depths[i]>.999f&&pixels[i*4]>90&&pixels[i*4+1]>90&&pixels[i*4+2]>90)labels[h]++;
        }
        check(lit[0]>100&&lit[1]>100,"startup shows both textured tracked controllers");
        check(labels[0]>20&&labels[1]>20,"button-anchored instructional labels are visible beside both controllers");
        size_t inner_grip[2]{};
        for(int y=106;y<125;y++)for(int x=88;x<168;x++){
            const int hand=x<128?0:1;const size_t i=y*256+x;
            if((hand==0&&x>122)||(hand==1&&x<134))continue;
            if(depths[i]>.999f&&pixels[i*4]>90&&pixels[i*4+1]>90&&pixels[i*4+2]>90)inner_grip[hand]++;
        }
        // These letters are only about four pixels high at this test size.
        check(inner_grip[0]>10&&inner_grip[1]>10,"Grip labels sit inside the controllers: left label rightward and right label leftward");
        runtime.state.valid=true;runtime.state.service=true;
        instructions.update(hands,help,runtime,.4);check(instructions.service()&&!instructions.service_guns(),"native Service acknowledgement keeps list navigation separate from gun aiming");
        runtime.state.gun_calibration=true;runtime.state.service_busy=1;
        instructions.update(hands,help,runtime,.5);check(instructions.service_guns(),"actual busy native gun action enables gun-calibration input mode");
        instructions.toggle_service_guns();check(!instructions.service_guns(),"gun-calibration aiming can return to native Test/Service controls");
        runtime.state={};runtime.state.valid=true;runtime.state.ready=true;
        instructions.update(hands,help,runtime,.6);check(!instructions.service()&&!instructions.service_guns(),"only native Service exit acknowledgement returns to game controls");
    }
    {
        // The renderer owns the original game HUD. Presentation must not
        // replace its native revolver cylinder with invented text counters.
        Presentation game_ui;PresentationInput head;Settings preferences;preferences.laser=false;runtime.state={};
        CabinetInput started;started.coin_mask=1;game_ui.feedback(started,1);game_ui.update(head,preferences,runtime,1);
        GameState game;game.valid=game.ready=game.playing=game.counting_shots=true;game.mode=8;game.submode=8;
        for(auto& player:game.players){player.state=1;player.health=4;player.weapon=1;player.ammo=player.capacity=6;}
        auto no_replacement_hud=[&]{
            glDepthMask(GL_TRUE);glClearColor(0,0,0,1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
            game_ui.draw(vp,preferences,game,true,1);std::vector<uint8_t> rgb(256*256*4);
            glReadPixels(0,0,256,256,GL_RGBA,GL_UNSIGNED_BYTE,rgb.data());
            for(size_t i=0;i<rgb.size();i+=4)if(rgb[i]||rgb[i+1]||rgb[i+2])return false;
            return true;
        };
        check(no_replacement_hud(),"active native ammo state cannot draw a custom replacement HUD");
        game.players[0].ammo=0;game.players[1].weapon=5;game.players[1].ammo=game.players[1].capacity=10;
        check(no_replacement_hud(),"empty ammo and native pickups cannot introduce replacement reload or weapon labels");
    }
    {
        Presentation guns;PresentationInput hands;hands.focused=true;
        GameState game;game.valid=true;game.ready=true;game.playing=true;game.mode=8;for(auto& player:game.players){player.state=1;player.health=4;}
        CabinetInput cabinet;cabinet.hand={0,1};cabinet.coin_mask=1;guns.feedback(cabinet,0);
        for(int h=0;h<2;h++){hands.hands[h].tracked=true;hands.poses[h]=glm::translate(glm::mat4(1),glm::vec3(h==0?-.16f:.16f,0,-.65f));}
        guns.update(hands,settings,runtime,.3);settings.laser=false;
        glDepthMask(GL_TRUE);glClearColor(0,0,0,1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
        guns.draw(vp,settings,game,true,1,false);
        std::vector<uint8_t> before(256*256*4);std::vector<float> depths(256*256);
        glReadPixels(0,0,256,256,GL_RGBA,GL_UNSIGNED_BYTE,before.data());glReadPixels(0,0,256,256,GL_DEPTH_COMPONENT,GL_FLOAT,depths.data());
        std::array<std::array<int,2>,2> picks{};
        for(int h=0;h<2;h++) {
            bool found=false;
            for(int y=125;y>=70&&!found;y--)for(int x=h*128+12;x<(h+1)*128-12&&!found;x++){
                bool solid=true;for(int dy=-2;dy<=2;dy++)for(int dx=-2;dx<=2;dx++)if(depths[(y+dy)*256+x+dx]>=.999f)solid=false;
                if(!solid)continue;picks[h]={x,y};found=true;
                auto point=glm::inverse(vp)*glm::vec4((x+.5f)/128.f-1,(y+.5f)/128.f-1,depths[y*256+x]*2-1,1);
                auto behind=glm::vec3(point)/point.w*2.f;hands.aim_targets[h]=Vec3{behind.x,behind.y,behind.z};
            }
            check(found,"laser occlusion fixture has a solid interior gun pixel in each hand");
        }
        guns.update(hands,settings,runtime,1);settings.laser=true;
        glDepthMask(GL_TRUE);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);guns.draw(vp,settings,game,true,1,false);
        std::vector<uint8_t> after(before.size());glReadPixels(0,0,256,256,GL_RGBA,GL_UNSIGNED_BYTE,after.data());
        std::vector<float> after_depth(depths.size());glReadPixels(0,0,256,256,GL_DEPTH_COMPONENT,GL_FLOAT,after_depth.data());
        check(depths==after_depth,"translucent laser effects cannot write depth over either gun or the scene");
        for(auto xy:picks){size_t at=(xy[1]*256+xy[0])*4;check(before[at]==after[at]&&before[at+1]==after[at+1]&&before[at+2]==after[at+2],"laser endpoint behind either gun cannot draw through its solid body");}
    }
    {
        Presentation guns;PresentationInput hands;Settings effects;effects.laser=false;
        GameState game;game.valid=true;game.ready=true;game.playing=true;game.mode=8;for(auto& player:game.players){player.state=1;player.health=4;}
        CabinetInput cabinet;cabinet.hand={0,1};cabinet.coin_mask=1;guns.feedback(cabinet,0);
        for(int h=0;h<2;h++){
            hands.hands[h].tracked=true;
            hands.poses[h]=glm::translate(glm::mat4(1),glm::vec3(h==0?-.16f:.16f,0,-.65f))*glm::rotate(glm::mat4(1),glm::radians(70.f),glm::vec3(0,1,0));
        }
        guns.update(hands,effects,runtime,1);cabinet.shots={1,1};guns.feedback(cabinet,1);
        struct Frame {std::vector<uint8_t> rgb;std::vector<float> depth;};
        unsigned capture=0;
        auto frame=[&](const glm::mat4& view,double now){
            glDepthMask(GL_TRUE);glClearColor(0,0,0,1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
            guns.draw(view,effects,game,true,now,false);Frame f{std::vector<uint8_t>(256*256*4),std::vector<float>(256*256)};
            glReadPixels(0,0,256,256,GL_RGBA,GL_UNSIGNED_BYTE,f.rgb.data());glReadPixels(0,0,256,256,GL_DEPTH_COMPONENT,GL_FLOAT,f.depth.data());
            if(const char* directory=std::getenv("VCOP2_EFFECT_PROBE")){
                std::ofstream out(std::filesystem::path(directory)/(std::to_string(capture++)+".ppm"),std::ios::binary);out<<"P6\n256 256\n255\n";
                for(int y=255;y>=0;y--)for(int x=0;x<256;x++)out.write(reinterpret_cast<const char*>(&f.rgb[(y*256+x)*4]),3);
            }
            return f;
        };
        bool flash_visible=true;
        for(float x:{-.032f,.032f}){
            auto f=frame(vp*glm::lookAt(glm::vec3(x,.08f,.04f),glm::vec3(0,0,-.65f),glm::vec3(0,1,0)),1.045);
            size_t light=0;for(size_t i=0;i<f.depth.size();i++)if(f.depth[i]>.9999f&&f.rgb[i*4]>30&&f.rgb[i*4]>f.rgb[i*4+1]*1.1)++light;
            flash_visible=flash_visible&&light>20;
        }
        // Move both guns away: smoke must remain in the room at the previous shot.
        for(auto& pose:hands.poses)pose[3].x+=.6f;
        guns.update(hands,effects,runtime,1.2);
        auto smoke=frame(vp,1.2),again=frame(vp,1.2);
        check(smoke.rgb==again.rgb&&smoke.depth==again.depth,"stereo/repeated draws do not emit extra smoke or change depth");
        size_t puffs=0;
        for(int y=60;y<180;y++)for(int x=12;x<170;x++){
            size_t i=y*256+x;auto r=smoke.rgb[i*4],g=smoke.rgb[i*4+1],b=smoke.rgb[i*4+2];
            if(smoke.depth[i]>.9999f&&r>2&&std::abs(int(r)-g)<4&&std::abs(int(g)-b)<4)++puffs;
        }
        check(puffs>20,"soft smoke remains at the shot location after the guns move away");
        check(flash_visible,"shaped flash is visible outside angled barrels in both eyes after 35ms");
        auto expired=frame(vp,2.0);size_t lingering=0;
        for(size_t i=0;i<expired.depth.size();i++)if(expired.depth[i]>.9999f&&(expired.rgb[i*4]||expired.rgb[i*4+1]||expired.rgb[i*4+2]))++lingering;
        check(lingering==0,"smoke expires instead of leaving persistent room-space artifacts");
        for(int h=0;h<2;h++)hands.poses[h][3].x=h==0?-.16f:.16f;
        guns.update(hands,effects,runtime,3);guns.feedback(cabinet,3);frame(vp,3.01);
        for(auto& hand:hands.hands)hand.tracked=false;
        guns.update(hands,effects,runtime,3.02);guns.feedback(cabinet,3.02);auto lost=frame(vp,3.03);size_t stale=0;
        for(size_t i=0;i<lost.rgb.size();i+=4)if(lost.rgb[i]||lost.rgb[i+1]||lost.rgb[i+2])++stale;
        check(stale==0,"losing both tracked hands clears stale room-space gun effects");
        auto no_effects=[&](const Frame& f){
            for(size_t i=0;i<f.depth.size();i++)if(f.depth[i]>.9999f&&(f.rgb[i*4]||f.rgb[i*4+1]||f.rgb[i*4+2]))return false;
            return true;
        };
        for(auto& hand:hands.hands)hand.tracked=true;
        guns.update(hands,effects,runtime,3.04);
        check(no_effects(frame(vp,3.04)),"tracking recovery cannot replay the old flash or re-emit its smoke");
        guns.update(hands,effects,runtime,4);guns.feedback(cabinet,4);frame(vp,4.01);
        hands.focused=false;guns.update(hands,effects,runtime,4.02);guns.feedback(cabinet,4.02);frame(vp,4.03);
        hands.focused=true;guns.update(hands,effects,runtime,4.04);
        check(no_effects(frame(vp,4.04)),"focus recovery cannot replay stale accepted-shot effects");
        hands.hands[0].tracked=false;guns.update(hands,effects,runtime,5);cabinet.shots={1,0};guns.feedback(cabinet,5);frame(vp,5.01);
        hands.hands[0].tracked=true;guns.update(hands,effects,runtime,5.02);
        check(no_effects(frame(vp,5.02)),"a late shot for one untracked owning hand cannot replay on recovery");
        runtime.state=game;runtime.state.service=true;runtime.state.ready=runtime.state.playing=false;
        guns.update(hands,effects,runtime,6);auto service_before=frame(vp,6.045);
        guns.feedback(cabinet,6);auto service_after=frame(vp,6.045);
        check(service_before.rgb==service_after.rgb,"a late accepted-shot event cannot recoil or flash after native Service acknowledgement");
    }
    if(argc==4) {
        // Optional owned-media fixture. Text geometry is converted from the
        // independently verified private extractor; material RAM is a complete
        // native snapshot. Neither belongs in a software package.
        std::ifstream mesh_input(argv[2]);check(bool(mesh_input),"original-gun fixture opens");
        unsigned count{};mesh_input>>count;check(count==5||count==6,"fixture has all five actual pickup guns and optional original scene pistol");
        for(unsigned n=0;n<count;n++) {
            auto weapon=std::make_shared<NativeWeapon>();size_t faces{};
            mesh_input>>weapon->weapon>>weapon->model>>weapon->object>>faces;
            check(weapon->weapon>=1&&weapon->weapon<=6&&faces>0&&faces<4096,"original fixture is bounded");
            for(size_t i=0;i<faces;i++) {
                NativeWeaponPolygon face;mesh_input>>face.count>>face.attr;
                check(face.count==3||face.count==4,"fixture has native triangles/quads");
                for(auto& h:face.header)mesh_input>>h;for(auto& normal:face.normal)mesh_input>>normal;
                for(unsigned v=0;v<face.count;v++){auto& vertex=face.vertices[v];mesh_input>>vertex.x>>vertex.y>>vertex.z>>vertex.u>>vertex.v;}
                weapon->polygons.push_back(face);
            }
            check(bool(mesh_input),"original fixture is complete");weapon->valid=true;runtime.weapons[weapon->weapon]=weapon;
        }
        auto snapshot=std::make_shared<NativeFrame>();snapshot->width=496;snapshot->height=384;
        for(const char* name:{"texture0","texture1","palette","colorxlat","luma","gamma"}) {
            std::ifstream in(std::filesystem::path(argv[3])/(std::string(name)+".bin"),std::ios::binary);
            check(bool(in),"private native material snapshot opens");snapshot->blobs.push_back({name,{std::istreambuf_iterator<char>(in),std::istreambuf_iterator<char>()}});
        }
        runtime.scene.store(snapshot);Presentation originals;PresentationInput hands;Settings choice;choice.laser=false;
        GameState game;game.valid=game.ready=game.playing=true;for(auto& player:game.players){player.state=1;player.health=4;}
        runtime.state=game;CabinetInput cabinet;cabinet.hand={0,1};cabinet.coin_mask=1;originals.feedback(cabinet,10);
        for(int h=0;h<2;h++){hands.hands[h].tracked=true;hands.poses[h]=glm::translate(glm::mat4(1),glm::vec3(h==0?-.16f:.16f,0,-.65f));}
        originals.update(hands,choice,runtime,10);
        auto capture=[&]{
            originals.prepare(choice,&runtime);glDepthMask(GL_TRUE);glClearColor(0,0,0,1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
            originals.draw(vp,choice,game,true,10);std::vector<float> result(256*256);glReadPixels(0,0,256,256,GL_DEPTH_COMPONENT,GL_FLOAT,result.data());return result;
        };
        auto fallback=capture();
        for(int w=2;w<=6;w++) {
            choice.gun=8;for(auto& player:game.players)player.weapon=w;runtime.state=game;auto mesh=capture();size_t visible{},different{};
            for(size_t i=0;i<mesh.size();i++){visible+=mesh[i]<.999f;different+=std::abs(mesh[i]-fallback[i])>.00001f;}
            check(visible>40&&different>40,"each original pickup renders its own non-fallback 3D geometry");
            if(const char* directory=std::getenv("VCOP2_WEAPON_PROBE")) {
                std::vector<uint8_t> rgb(256*256*3);glReadPixels(0,0,256,256,GL_RGB,GL_UNSIGNED_BYTE,rgb.data());
                std::ofstream out(std::filesystem::path(directory)/("original-"+std::to_string(w)+".ppm"),std::ios::binary);out<<"P6\n256 256\n255\n";
                for(int y=255;y>=0;y--)out.write(reinterpret_cast<const char*>(rgb.data()+y*256*3),256*3);
            }
        }
        for(auto& player:game.players)player.weapon=2;runtime.state=game;choice.gun=8;auto rifle=capture();std::array<glm::vec3,2> rifle_muzzle{originals.muzzle(hands.poses[0],choice,0),originals.muzzle(hands.poses[1],choice,1)};
        for(auto& player:game.players)player.weapon=6;runtime.state=game;auto magnum=capture();std::array<glm::vec3,2> magnum_muzzle{originals.muzzle(hands.poses[0],choice,0),originals.muzzle(hands.poses[1],choice,1)};
        game.players[0].weapon=2;game.players[1].weapon=6;runtime.state=game;choice.gun=8;auto automatic=capture();
        choice.game_zoom=true;check(capture()==automatic,"optional world zoom does not magnify or reposition either held original gun");choice.game_zoom=false;
        size_t left_difference{},right_difference{};
        for(int y=0;y<256;y++)for(int x=0;x<256;x++){size_t i=y*256+x;if(x<128)left_difference+=automatic[i]!=rifle[i];else right_difference+=automatic[i]!=magnum[i];}
        check(left_difference==0&&right_difference==0,"automatic native guns independently follow each player's actual equipped pickup");
        check(glm::distance(originals.muzzle(hands.poses[0],choice,0),rifle_muzzle[0])<.000001f&&glm::distance(originals.muzzle(hands.poses[1],choice,1),magnum_muzzle[1])<.000001f,"each hand's aim ray and flash share that player's original model muzzle");
        cabinet.hand={1,0};originals.feedback(cabinet,10);auto swapped=capture();left_difference=right_difference=0;
        for(int y=0;y<256;y++)for(int x=0;x<256;x++){size_t i=y*256+x;if(x<128)left_difference+=swapped[i]!=magnum[i];else right_difference+=swapped[i]!=rifle[i];}
        check(left_difference==0&&right_difference==0,"changing player hand ownership moves each equipped original gun to the correct controller");
        check(glm::distance(originals.muzzle(hands.poses[0],choice,0),magnum_muzzle[0])<.000001f&&glm::distance(originals.muzzle(hands.poses[1],choice,1),rifle_muzzle[1])<.000001f,"swapped player ownership also updates both aim ray origins");
        cabinet.hand={0,1};originals.feedback(cabinet,10);
        game.players[0].weapon=1;runtime.state=game;auto changed=capture();
        size_t changed_left{},changed_right{};
        for(int y=0;y<256;y++)for(int x=0;x<256;x++){size_t i=y*256+x;if(x<128)changed_left+=automatic[i]!=changed[i];else changed_right+=automatic[i]!=changed[i];}
        check(changed_left>40&&changed_right==0,"returning P1 to its standard gun preserves P2's original pickup");
        if(runtime.weapons[1]) {
            choice.gun=0;auto arcade_pistol=capture();choice.gun=8;auto original_pistol=capture();size_t scene_difference{};
            for(int y=0;y<256;y++)for(int x=0;x<128;x++){size_t i=y*256+x;scene_difference+=original_pistol[i]!=arcade_pistol[i];}
            check(scene_difference>40,"standard auto mode renders the source scene pistol cosmetic rather than the Arcade fallback");
            if(const char* directory=std::getenv("VCOP2_WEAPON_PROBE")) {
                std::vector<uint8_t> rgb(256*256*3);glReadPixels(0,0,256,256,GL_RGB,GL_UNSIGNED_BYTE,rgb.data());
                std::ofstream out(std::filesystem::path(directory)/"original-1.ppm",std::ios::binary);out<<"P6\n256 256\n255\n";
                for(int y=255;y>=0;y--)out.write(reinterpret_cast<const char*>(rgb.data()+y*256*3),256*3);
            }
            const auto previous_game=game;game.players[1].weapon=1;runtime.state=game;
            auto mirrored=capture();size_t reflected{},mismatch{};
            for(int y=0;y<256;y++)for(int x=0;x<128;x++){
                float l=mirrored[y*256+x],r=mirrored[y*256+255-x];
                reflected+=l<.999f||r<.999f;mismatch+=std::abs(l-r)>.000001f;
            }
            check(reflected>40&&mismatch<8,"actual scene pistol and embedded hand render with mirrored depth on the physical left controller");
            if(const char* directory=std::getenv("VCOP2_WEAPON_PROBE")) {
                std::vector<uint8_t> rgb(256*256*3);glReadPixels(0,0,256,256,GL_RGB,GL_UNSIGNED_BYTE,rgb.data());
                std::ofstream out(std::filesystem::path(directory)/"mirrored-pistol-hands.ppm",std::ios::binary);out<<"P6\n256 256\n255\n";
                for(int y=255;y>=0;y--)out.write(reinterpret_cast<const char*>(rgb.data()+y*256*3),256*3);
            }
            // Explicit grip positions must produce the same geometry as moving
            // the diagnostic aim origins to those positions, at any aim rotation.
            auto before_hands=hands;
            for(int h=0;h<2;h++)hands.grip_positions[h]=glm::vec3(hands.poses[h][3])+glm::vec3(0,-.035f,.045f);
            originals.update(hands,choice,runtime,10);auto gripped=capture();
            for(int h=0;h<2;h++){hands.poses[h][3]=glm::vec4(*hands.grip_positions[h],1);hands.grip_positions[h].reset();}
            originals.update(hands,choice,runtime,10);check(capture()==gripped,"production drawing uses the same physical grip origin as gun aiming");
            hands=before_hands;originals.update(hands,choice,runtime,10);game=previous_game;runtime.state=game;
        }
        // Without an extracted mesh, the selected original must be a visible
        // Arcade fallback until native extraction arrives.
        for(auto& player:game.players)player.weapon=2;runtime.state=game;choice.gun=0;auto arcade=capture();auto available_rifle=runtime.weapons[2];runtime.weapons[2].reset();choice.gun=8;
        check(capture()==arcade,"an unavailable equipped original uses the Arcade fallback");
        runtime.weapons[2]=available_rifle;check(capture()==rifle,"original models become available without restarting after native extraction arrives");
    }
    check(glGetError()==GL_NO_ERROR,"presentation GL operations complete without errors");
    glDeleteRenderbuffers(1,&depth);
    glDeleteFramebuffers(1,&fbo);glDeleteTextures(1,&image);
    std::cout<<"Loading menu remains visible after the shared timed splash\n";
}
