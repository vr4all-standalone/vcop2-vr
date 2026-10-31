// SPDX-License-Identifier: GPL-2.0-or-later
#ifdef NDEBUG
#undef NDEBUG
#endif
#define main replay_reference_main
#include "../tools/replay_model2_frames.cpp"
#undef main
#include <cassert>

static std::shared_ptr<NativeFrame> fixture() {
    auto f=std::make_shared<NativeFrame>();f->width=496;f->height=384;f->yoffset=127;
    for(const auto& named:{std::pair<const char*,size_t>{"texture0",2097152},{"texture1",2097152},{"palette",16384},{"colorxlat",49152},{"luma",32768},{"gamma",256}})f->blobs.push_back({named.first,std::vector<uint8_t>(named.second)});
    unsigned base=5|(10<<5)|(15<<10);f->blobs[2].bytes[8192]=base;f->blobs[2].bytes[8193]=base>>8;
    for(unsigned c=0;c<3;c++)for(unsigned component=0;component<32;component++)for(unsigned luma=0;luma<64;luma++)f->blobs[3].bytes[2*(c*8192+component*256+luma)]=component*4+luma;
    for(unsigned n=0;n<256;n++)f->blobs[5].bytes[n]=255-n;
    f->background.assign(496*384,0xff112233);f->foreground.assign(496*384,0);
    for(int y=350;y<363;y++)for(int x=18;x<120;x++){f->foreground[y*496+x]=0xffff1122;f->foreground[y*496+495-x]=0xff2244ff;}
    NativePolygon world;world.count=4;world.focus={173.651f,173.651f};world.center={248,320};world.viewport={0,128,496,512};world.luma=128;
    world.vertices={NativeVertex{-10000,10000,10},NativeVertex{10000,10000,10},NativeVertex{10000,-10000,10},NativeVertex{-10000,-10000,10}};f->polygons.push_back(world);
    for(int player=0;player<2;player++){
        NativePolygon hud;hud.count=4;hud.object=0x805af4;hud.window=4;hud.focus={1200,1200};hud.center={248,320};hud.viewport={0,128,496,512};hud.luma=252;
        float x0=player?420:18,x1=player?478:76,y0=292,y1=344,z=1200;
        hud.vertices={NativeVertex{(x0-248)*z,(191-y0)*z,z},NativeVertex{(x1-248)*z,(191-y0)*z,z},NativeVertex{(x1-248)*z,(191-y1)*z,z},NativeVertex{(x0-248)*z,(191-y1)*z,z}};f->polygons.push_back(hud);
    }
    f->native=f->background;return f;
}
int main(int argc,char** argv) {
    auto f=argc==4?read_frame(argv[1],argv[2]):fixture();
    if(argc==4){std::ifstream matrices(std::string(argv[1])+".matrices");if(matrices)for(auto& p:f->polygons)for(auto& value:p.matrix)matrices>>value;}
    for(const auto& polygon:f->polygons)if(polygon.window==4&&polygon.focus==std::array<float,2>{1200,1200})assert(native_hud_polygon(polygon));
    auto hud=std::find_if(f->polygons.begin(),f->polygons.end(),native_hud_polygon);assert(hud!=f->polygons.end());
    auto p=*hud;assert(native_hud_polygon(p));p.object=0x805af3;p.matrix[11]=0;assert(!native_hud_polygon(p));
    p=*hud;p.object=0x8bfa2f;p.matrix[9]=-203;p.matrix[10]=-123;p.matrix[11]=1200;assert(native_hud_polygon(p)); // Original Rifle HUD family.
    p.matrix[11]=100;assert(!native_hud_polygon(p));
    p=*hud;p.window=3;assert(!native_hud_polygon(p));p=*hud;p.focus[0]=1199;assert(!native_hud_polygon(p));
    p=*hud;p.center[0]++;assert(!native_hud_polygon(p));p=*hud;p.viewport[0]++;assert(!native_hud_polygon(p));p=*hud;p.vertices[0].z=10;assert(!native_hud_polygon(p));
    auto triangles=scene_triangles(*f,true,true);size_t hud_triangles{};
    for(const auto& triangle:triangles)if(triangle.hud){hud_triangles++;assert(triangle.panel);}
    assert(hud_triangles>0);assert(native_camera_lens(*f).fx!=1200);
    for(const auto& polygon:f->polygons)if(native_hud_polygon(polygon))for(auto position:world_positions(polygon,*f,1,1.8,true))assert(std::abs(position.z+1.8)<1e-5);
    // A HUD cylinder remains a display element and never becomes a gun hit.
    auto only=std::make_shared<NativeFrame>(*f);std::erase_if(only->polygons,[](const auto& p){return !native_hud_polygon(p);});
    auto hit=trace_scene_aim({{0,0,0},{-.52,-.28,-1}},*only,1,2,true,nullptr,true);assert(hit.target&&std::abs(hit.target->z+2)<1e-6);
    GLContext context;GLuint fbo,color,depth;glGenFramebuffers(1,&fbo);glBindFramebuffer(GL_FRAMEBUFFER,fbo);
    glGenTextures(1,&color);glBindTexture(GL_TEXTURE_2D,color);glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,f->width,f->height,0,GL_RGBA,GL_UNSIGNED_BYTE,nullptr);glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,color,0);
    glGenRenderbuffers(1,&depth);glBindRenderbuffer(GL_RENDERBUFFER,depth);glRenderbufferStorage(GL_RENDERBUFFER,GL_DEPTH_COMPONENT24,f->width,f->height);glFramebufferRenderbuffer(GL_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,GL_RENDERBUFFER,depth);assert(glCheckFramebufferStatus(GL_FRAMEBUFFER)==GL_FRAMEBUFFER_COMPLETE);glViewport(0,0,f->width,f->height);
    auto lens=panel_lens(*f);auto vp=projection(lens);
    auto draw=[&](GLScene& scene,float hud_size=1){glDepthMask(GL_TRUE);glClearColor(0,0,0,1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);scene.draw(vp,1,1.8,1,hud_size);std::vector<uint8_t> image(size_t(f->width)*f->height*4);glReadPixels(0,0,f->width,f->height,GL_RGBA,GL_UNSIGNED_BYTE,image.data());return image;};
    GLScene immersive;immersive.set_immersive(true);immersive.prepare(f,true);auto output=draw(immersive);
    // Native mesh and foreground scale together, with unchanged source material.
    if(argc!=4){
        auto small=draw(immersive,.5f),large=draw(immersive,2);
        assert(small!=output&&large!=output&&small!=large);
        assert(draw(immersive,1)==output); // next frame restores every uniform
        auto red_bounds=[&](const auto& image){int lo=f->width,hi=-1;for(int y=0;y<f->height;y++)for(int x=0;x<f->width;x++){auto n=(y*f->width+x)*4;if(image[n]==255&&image[n+1]==17&&image[n+2]==34){lo=std::min(lo,x);hi=std::max(hi,x);}}return std::pair{lo,hi};};
        auto base_bounds=red_bounds(output),small_bounds=red_bounds(small);
        assert(base_bounds.second>base_bounds.first&&small_bounds.second>small_bounds.first);
        assert(std::abs((small_bounds.second-small_bounds.first)*2-(base_bounds.second-base_bounds.first))<=2);
        assert(std::abs((small_bounds.first+small_bounds.second)-((base_bounds.first+base_bounds.second)*.5+f->width*.5))<=2);
        auto center=(size_t(f->height/2)*f->width+f->width/2)*4;
        assert(std::equal(output.begin()+center,output.begin()+center+4,small.begin()+center));
        auto base_vp=vp;vp=glm::translate(vp,glm::vec3(.03f,0,0));auto eye=draw(immersive,.5f);vp=base_vp;
        assert(draw(immersive,.5f)==small&&eye!=small); // both eyes, no stale scale
        immersive.prepare(f,false);assert(draw(immersive,.5f)==draw(immersive,2));
        immersive.prepare(f,true);assert(draw(immersive,1)==output);
        auto fallback=std::make_shared<NativeFrame>(*f);fallback->polygons.clear();fallback->raster_only=true;
        for(int y=61;y<83;y++)for(int x=37;x<56;x++)fallback->native[y*f->width+x]=((x+y)&1)?0xff55aaff:0xffffee44;
        immersive.prepare(fallback,true);assert(draw(immersive,.5f)==draw(immersive,2));
        immersive.prepare(f,true);
    }
    only->background.assign(size_t(f->width)*f->height,0xff000000);GLScene reference;reference.set_presentation_lens(lens);reference.prepare(only);auto expected=draw(reference);
    size_t compared{},different{},native_compared{},native_different{};
    for(int y=280;y<370;y++)for(int x=8;x<f->width-8;x++)if(x<130||x>f->width-130){
        size_t at=(size_t(f->height-1-y)*f->width+x)*4;
        if(expected[at]||expected[at+1]||expected[at+2]){
            compared++;if(!std::equal(expected.begin()+at,expected.begin()+at+3,output.begin()+at))different++;
            auto native=f->native[size_t(y)*f->width+x];native_compared++;
            if(output[at]!=(native>>16&255)||output[at+1]!=(native>>8&255)||output[at+2]!=(native&255))native_different++;
        }
    }
    assert(compared>100);std::cout<<"Native HUD original-mesh/reference pixels "<<compared<<", different "<<different<<"; native raster difference "<<native_different<<'/'<<native_compared<<'\n';
    if(argc!=4)assert(different==0);else {
        assert(different<compared/20); // Native rasterizer edge/texel rounding.
        assert(native_different<compared/20);
        ppm(std::string(argv[3])+"-immersive.ppm",*f,output);ppm(std::string(argv[3])+"-hud-reference.ppm",*f,expected);
    }
    assert(glGetError()==GL_NO_ERROR);glDeleteFramebuffers(1,&fbo);glDeleteTextures(1,&color);glDeleteRenderbuffers(1,&depth);
}
