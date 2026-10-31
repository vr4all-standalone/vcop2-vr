// SPDX-License-Identifier: GPL-2.0-or-later
#include "ui_compat.h"
#include <cstdlib>
#include <fstream>
#include <cstring>
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_ONLY_JPEG
#include <stb_image.h>
namespace resource {
std::unique_ptr<u8[]> load(const char* path,size_t& size) {
    const char* root=std::getenv("VCOP2_ASSETS");std::string name=std::string(root?root:"assets")+"/"+path;
    std::ifstream file(name,std::ios::binary|std::ios::ate);
    size=0;if(!file)return {};auto length=file.tellg();if(length<=0||length>64*1024*1024)return {};
    auto data=std::make_unique<u8[]>(size_t(length));file.seekg(0);if(!file.read(reinterpret_cast<char*>(data.get()),length))return {};
    size=size_t(length);return data;
}
}
std::string OpenGlSource::generate() const {
#ifdef __ANDROID__
    return "#version 300 es\nprecision highp float;\nprecision highp int;\n"+source_;
#else
    return "#version 330\n"+source_;
#endif
}
GLuint gl_CompileAndLink(const char* vs,const char* fs) {
    auto compile=[](GLenum type,const char* text)->GLuint {
        auto shader=glCreateShader(type);glShaderSource(shader,1,&text,nullptr);glCompileShader(shader);
        GLint okay{};glGetShaderiv(shader,GL_COMPILE_STATUS,&okay);if(!okay){char error[2048]{};glGetShaderInfoLog(shader,sizeof(error),nullptr,error);std::fprintf(stderr,"%s\n",error);glDeleteShader(shader);return 0;}return shader;
    };
    auto v=compile(GL_VERTEX_SHADER,vs),f=compile(GL_FRAGMENT_SHADER,fs);if(!v||!f){glDeleteShader(v);glDeleteShader(f);return 0;}
    auto p=glCreateProgram();glAttachShader(p,v);glAttachShader(p,f);
    glBindAttribLocation(p,0,"in_pos");glBindAttribLocation(p,1,"in_uv");glBindAttribLocation(p,2,"in_normal");glBindAttribLocation(p,3,"in_color");
    glLinkProgram(p);glDeleteShader(v);glDeleteShader(f);GLint okay{};glGetProgramiv(p,GL_LINK_STATUS,&okay);if(!okay){glDeleteProgram(p);return 0;}return p;
}
