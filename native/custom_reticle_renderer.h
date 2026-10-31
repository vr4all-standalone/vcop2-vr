// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include "custom_reticle.h"
#include "shared/ui_compat.h"
#include <glm/glm.hpp>
namespace vcop2 {
// Static vector arms: one upload for the lifetime of the presentation, one draw
// per player/eye. No texture generation or scene geometry work on this path.
class CustomReticleRenderer {
 GLuint program_{},vao_{},buffer_{};GLint mvp_{-1},size_{-1},color_{-1};
public:
 ~CustomReticleRenderer(){if(program_)glDeleteProgram(program_);if(vao_)glDeleteVertexArrays(1,&vao_);if(buffer_)glDeleteBuffers(1,&buffer_);}
 void draw(const glm::mat4& mvp,const glm::vec2& half_size,const glm::vec3& color){
  if(!program_){
   OpenGlSource vs,fs;
   vs.addSource("in vec3 in_pos;uniform mat4 mvp;uniform vec2 half_size;void main(){gl_Position=mvp*vec4(in_pos.xy*half_size,0,1);}");
   fs.addSource("uniform vec3 player_color;out vec4 result;void main(){result=vec4(player_color,1);}");
   program_=gl_CompileAndLink(vs.generate().c_str(),fs.generate().c_str());if(!program_)return;
   mvp_=glGetUniformLocation(program_,"mvp");size_=glGetUniformLocation(program_,"half_size");color_=glGetUniformLocation(program_,"player_color");
   glGenVertexArrays(1,&vao_);glBindVertexArray(vao_);glGenBuffers(1,&buffer_);glBindBuffer(GL_ARRAY_BUFFER,buffer_);
   const auto mesh=custom_reticle_mesh();glBufferData(GL_ARRAY_BUFFER,sizeof(mesh),mesh.data(),GL_STATIC_DRAW);
   glEnableVertexAttribArray(0);glVertexAttribPointer(0,3,GL_FLOAT,GL_FALSE,sizeof(CustomReticleVertex),nullptr);
  }
  glUseProgram(program_);glBindVertexArray(vao_);glUniformMatrix4fv(mvp_,1,GL_FALSE,&mvp[0][0]);glUniform2f(size_,half_size.x,half_size.y);glUniform3f(color_,color.r,color.g,color.b);
  glEnable(GL_DEPTH_TEST);glDepthFunc(GL_LEQUAL);glDepthMask(GL_FALSE);glDisable(GL_CULL_FACE);glDisable(GL_BLEND);glDrawArrays(GL_TRIANGLES,0,72);
 }
};
}
