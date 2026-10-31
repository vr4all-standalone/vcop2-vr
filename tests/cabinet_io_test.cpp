#include "../native/cabinet_io.h"
#include <cmath>
#include <iostream>
#define CHECK(value) do { if(!(value)){std::cerr<<"Failed: " #value " line "<<__LINE__<<'\n';return 1;} } while(0)
int main(){
    using namespace vcop2;
    Cabinet c;
    CHECK(in0(0xff,c)==0xff);CHECK(in1(0xff,c)==0xff);
    c.coins=1;c.starts=2;CHECK(in0(0xff,c)==0xde);
    c.test=true;c.service=true;CHECK(in0(0xff,c)==0xd2);
    c.trigger={true,false};CHECK(in1(0xff,c)==0xfe);CHECK(in1(0xf0,c)==0xf0);
    c.trigger={true,true};CHECK(in1(0xff,c)==0xfc);
    c.x={0x234,0x123};c.y={0x1ab,0x3fe};
    const unsigned wanted[]={0xab,1,0x34,2,0xfe,3,0x23,1};
    for(unsigned i=0;i<8;i++)CHECK(serial_byte(i,c)==wanted[i]);
    CHECK(offscreen(0,383,230)==false);CHECK(offscreen(1,380,230)==false);
    CHECK(offscreen(0,161,230));CHECK(!offscreen(0,162,230));
    CHECK(offscreen(0,606,230));CHECK(!offscreen(0,605,230));
    CHECK(offscreen(0,383,55));CHECK(!offscreen(0,383,56));
    CHECK(offscreen(0,383,406));CHECK(!offscreen(0,383,405));
    CHECK(offscreen(1,158,230));CHECK(!offscreen(1,159,230));
    CHECK(offscreen(1,603,230));CHECK(!offscreen(1,602,230));
    c.x={383,380};c.y={230,230};CHECK(serial_byte(8,c)==0xfc);
    c.x[0]=137;CHECK(serial_byte(8,c)==0xfd);
    c.x[1]=627;CHECK(serial_byte(8,c)==0xff);
    c.x[0]=383;CHECK(serial_byte(9,c)==0xfe);
    std::cout<<"Model 2A cabinet bit, serial-order and border checks passed\n";
}
