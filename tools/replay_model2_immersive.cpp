// SPDX-License-Identifier: GPL-2.0-or-later
// Private six-direction world replay; inputs remain locally supplied game data.
#define main vcop2_native_replay_main
#include "replay_model2_frames.cpp"
#undef main
#include <cmath>
static void write_image(const std::filesystem::path& path,int width,int height) {
    std::vector<uint8_t> pixels(size_t(width)*height*3);glReadPixels(0,0,width,height,GL_RGB,GL_UNSIGNED_BYTE,pixels.data());
    std::ofstream out(path,std::ios::binary);out<<"P6\n"<<width<<' '<<height<<"\n255\n";
    for(int y=height-1;y>=0;y--)out.write(reinterpret_cast<const char*>(pixels.data()+size_t(y)*width*3),width*3);
}
int main(int argc,char** argv)try {
    if(argc!=4&&argc!=5){std::cerr<<"usage: replay_model2_immersive geometry capture-directory output-directory [native-sky.ppm]\n";return 2;}
    auto frame=read_frame(argv[1],argv[2]);const std::filesystem::path output=argv[3];std::filesystem::create_directories(output);
    if(argc==5) {
        std::ifstream sky(argv[4],std::ios::binary);std::string type;int maximum{};sky>>type>>frame->sky_width>>frame->sky_height>>maximum;sky.get();
        if(type!="P6"||maximum!=255||frame->sky_width<1||frame->sky_width>2048||frame->sky_height<1||frame->sky_height>2048)throw std::runtime_error("Invalid native sky snapshot");
        frame->sky.resize(size_t(frame->sky_width)*frame->sky_height);
        for(auto& pixel:frame->sky){uint8_t rgb[3];sky.read(reinterpret_cast<char*>(rgb),3);pixel=0xff000000u|(uint32_t(rgb[0])<<16)|(uint32_t(rgb[1])<<8)|rgb[2];}
        if(!sky)throw std::runtime_error("Incomplete native sky snapshot");
    }
    const bool game_zoom=std::getenv("VCOP2_REPLAY_GAME_ZOOM")!=nullptr;
    const float zoom=float(frame_zoom(*frame,game_zoom));
    GLContext context;GLScene scene;scene.set_immersive(true);scene.prepare(frame);
    constexpr int width=640,height=640;GLuint fbo,color,depth;glGenFramebuffers(1,&fbo);glBindFramebuffer(GL_FRAMEBUFFER,fbo);
    glGenTextures(1,&color);glBindTexture(GL_TEXTURE_2D,color);glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,width,height,0,GL_RGBA,GL_UNSIGNED_BYTE,nullptr);glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,color,0);
    glGenRenderbuffers(1,&depth);glBindRenderbuffer(GL_RENDERBUFFER,depth);glRenderbufferStorage(GL_RENDERBUFFER,GL_DEPTH_COMPONENT24,width,height);glFramebufferRenderbuffer(GL_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,GL_RENDERBUFFER,depth);
    if(glCheckFramebufferStatus(GL_FRAMEBUFFER)!=GL_FRAMEBUFFER_COMPLETE)throw std::runtime_error("Invalid immersive diagnostic framebuffer");
    glViewport(0,0,width,height);const auto projection=glm::perspective(glm::radians(100.f),1.f,.02f,10000.f);
    struct View {const char* name;float yaw,pitch,x;};
    for(const auto& view:{View{"forward",0,0,0},View{"left",-60,0,0},View{"right",60,0,0},View{"behind",180,0,0},View{"up",0,60,0},View{"down",0,-60,0},View{"lean-left",0,0,-.25f},View{"lean-right",0,0,.25f}}) {
        const float yaw=glm::radians(view.yaw),pitch=glm::radians(view.pitch);glm::vec3 eye(view.x,0,0);
        glm::vec3 direction(std::sin(yaw)*std::cos(pitch),std::sin(pitch),-std::cos(yaw)*std::cos(pitch));
        glDepthMask(GL_TRUE);glClearColor(.02,.02,.025,1);glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
        scene.draw(projection*glm::lookAt(eye,eye+direction,glm::vec3(0,1,0)),1.f,1.8f,zoom);glFinish();write_image(output/(std::string(view.name)+".ppm"),width,height);
    }
    glDeleteFramebuffers(1,&fbo);glDeleteTextures(1,&color);glDeleteRenderbuffers(1,&depth);
    std::cout<<"Rendered true camera-space world in eight head poses; focus "<<native_camera_lens(*frame).fx<<"; game zoom "<<game_zoom<<" factor "<<zoom<<'\n';return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
