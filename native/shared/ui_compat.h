// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "types.h"
#include <glad/gl.h>
#include <memory>
#include <string>
#include <cstdio>
#define WARN_LOG(kind,...) do {std::fprintf(stderr,__VA_ARGS__);std::fputc('\n',stderr);}while(0)
#define ERROR_LOG(kind,...) WARN_LOG(kind,__VA_ARGS__)
namespace resource {std::unique_ptr<u8[]> load(const char* path,size_t& size);}
class OpenGlSource {
    std::string source_;
public:
    OpenGlSource& addSource(const char* source){source_+=source;return *this;}
    std::string generate() const;
};
inline constexpr const char* VertexCompatShader="";
inline constexpr const char* PixelCompatShader="out lowp vec4 cv_output;\n#define gl_FragColor cv_output\n";
inline constexpr unsigned VERTEX_POS_ARRAY=0,VERTEX_UV_ARRAY=1;
GLuint gl_CompileAndLink(const char* vertex,const char* fragment);
inline GLuint createVertexArray(){GLuint name{};glGenVertexArrays(1,&name);return name;}
inline void bindVertexArray(GLuint name){glBindVertexArray(name);}
inline void deleteVertexArray(GLuint name){glDeleteVertexArrays(1,&name);}
