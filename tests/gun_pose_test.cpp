// SPDX-License-Identifier: GPL-2.0-or-later
#include "gun_pose.h"
#include <glm/gtc/matrix_transform.hpp>
#include <cstdlib>
#include <iostream>
using namespace vcop2;
void check(bool ok,const char* why){if(!ok){std::cerr<<why<<'\n';std::exit(1);}}
bool near(glm::vec3 a,glm::vec3 b){return glm::distance(a,b)<.000001f;}
int main(){
    auto aim=glm::translate(glm::mat4(1),glm::vec3(.2f,.3f,-.4f))*glm::rotate(glm::mat4(1),.7f,glm::vec3(0,1,0));
    glm::vec3 palm(.17f,.25f,-.32f);
    auto anchored=gun_pose(aim,palm);
    check(near(glm::vec3(anchored[3]),palm),"gun origin follows physical grip, removing aim-origin height/forward offset");
    check(near(glm::vec3(anchored*glm::vec4(0,0,-1,0)),glm::vec3(aim*glm::vec4(0,0,-1,0))),"grip position preserves calibrated aim orientation");
    check(gun_pose(aim,std::nullopt)==aim,"diagnostic poses without a grip retain their explicit origin");
    glm::vec3 authored_grip(.068397f,.002097f,.008521f);
    auto placement=glm::rotate(glm::mat4(1),glm::half_pi<float>(),glm::vec3(0,1,0))*glm::translate(glm::mat4(1),-authored_grip);
    for(int hand=0;hand<2;hand++)for(float scale:{.4f,1.f,1.6f}){
        auto model=gun_model_transform(anchored,scale,hand,placement);
        check(near(glm::vec3(model*glm::vec4(authored_grip,1)),palm),"scaling and handedness keep authored palm pivot fixed");
        auto barrel=glm::normalize(glm::vec3(model*glm::vec4(1,0,0,0)));
        check(near(barrel,glm::vec3(aim*glm::vec4(0,0,-1,0))),"left reflection preserves barrel direction");
    }
    auto left=gun_model_transform(glm::mat4(1),1,0,placement),right=gun_model_transform(glm::mat4(1),1,1,placement);
    glm::vec3 point(.16f,.04f,.03f);
    auto l=glm::vec3(left*glm::vec4(point,1)),r=glm::vec3(right*glm::vec4(point,1));
    check(near(l,glm::vec3(-r.x,r.y,r.z)),"left physical hand mirrors authored gun and embedded hand across controller X");
    check(glm::determinant(left)<0&&glm::determinant(right)>0,"only the physical left hand changes chirality");
    std::cout<<"Gun grip anchoring and physical handedness verified\n";

 // Input aiming, menu pointers and rendering consume the same palm-pivot pose.
 for(auto pair:{std::array<float,2>{-100,-40},{-30,-40},{0,-10},{7,-3},{30,20},{100,20}}){
  auto rest=pitched_gun_pose(aim,pair[0]);const float radians=pair[1]*3.14159265358979323846f/180;
  auto want=glm::vec3(aim*glm::vec4(0,std::sin(radians),-std::cos(radians),0));
  if(glm::distance(glm::vec3(rest*glm::vec4(0,0,-1,0)),want)>1e-5f||glm::distance(glm::vec3(rest[3]),glm::vec3(aim[3]))>1e-6f)return 1;
 }
 if(pitched_gun_pose(aim,NAN)!=pitched_gun_pose(aim,0))return 1;
}
