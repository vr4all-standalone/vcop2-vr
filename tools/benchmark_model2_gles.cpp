// SPDX-License-Identifier: GPL-2.0-or-later
// Offscreen production-renderer timings; no OpenXR session or game assets.
#define main replay_reference_main
#include "replay_model2_frames.cpp"
#undef main
#include <chrono>
int main(int argc,char** argv)try {
 if(argc!=3)return 2;
 auto frame=read_frame(argv[1],argv[2]);GLContext context;GLScene scene;scene.set_immersive(true);
 auto lens=presentation_lens(*frame);lens.fx=lens.fy=frame->width/(2*std::tan(glm::radians(65.f)/2));
 scene.set_presentation_lens(lens);scene.prepare(frame);
 constexpr int width=1832,height=1920;
 GLuint fbo,color,depth;glGenFramebuffers(1,&fbo);glBindFramebuffer(GL_FRAMEBUFFER,fbo);
 glGenTextures(1,&color);glBindTexture(GL_TEXTURE_2D,color);glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,width,height,0,GL_RGBA,GL_UNSIGNED_BYTE,nullptr);glFramebufferTexture2D(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,GL_TEXTURE_2D,color,0);
 glGenRenderbuffers(1,&depth);glBindRenderbuffer(GL_RENDERBUFFER,depth);glRenderbufferStorage(GL_RENDERBUFFER,GL_DEPTH_COMPONENT24,width,height);glFramebufferRenderbuffer(GL_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,GL_RENDERBUFFER,depth);
 if(glCheckFramebufferStatus(GL_FRAMEBUFFER)!=GL_FRAMEBUFFER_COMPLETE)throw std::runtime_error("Invalid benchmark framebuffer");
 auto vp=glm::perspective(glm::radians(90.f),float(width)/height,.02f,1000.f);
 glViewport(0,0,width,height);
 auto stereo=[&](){for(float eye:{-.032f,.032f}){glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);scene.draw(vp*glm::translate(glm::mat4(1),glm::vec3(-eye,0,0)),1.f,1.8f);}glFinish();};
 for(int i=0;i<5;i++)stereo();
 auto start=std::chrono::steady_clock::now();for(int i=0;i<30;i++)stereo();
 double ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count()/30;
 std::cout<<"GPU "<<glGetString(GL_RENDERER)<<"; "<<width<<'x'<<height<<" per eye; "<<frame->polygons.size()<<" polygons; stereo ms "<<ms<<"; fps "<<1000/ms<<'\n';
 return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
