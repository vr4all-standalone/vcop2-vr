// SPDX-License-Identifier: GPL-2.0-or-later
#include "../native/model2_capture.h"
#include <array>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <thread>
#define CHECK(v) do{if(!(v)){std::cerr<<"Failed: " #v " at "<<__LINE__<<'\n';return 1;}}while(0)
using namespace vcop2;
static const uint8_t* address(const Blob& blob){return blob.bytes.data();}
int main(int argc,char**) {
    const bool disabled=argc>1;
#ifdef _WIN32
    _putenv_s("VCOP2_NO_RESOURCE_CACHE",disabled?"1":"");
#else
    if(disabled)setenv("VCOP2_NO_RESOURCE_CACHE","1",1);else unsetenv("VCOP2_NO_RESOURCE_CACHE");
#endif
    SceneCapture capture;std::array<uint8_t,32> ram{};for(unsigned n=0;n<ram.size();n++)ram[n]=uint8_t(n);
    capture.begin(1,1);capture.bytes("texture0",ram.data(),ram.size());auto first=capture.finish();
    capture.begin(2,2);capture.bytes("texture0",ram.data(),ram.size());auto second=capture.finish();
    CHECK((address(first->blobs[0])==address(second->blobs[0]))==!disabled);
    // Changes at the same emulated RAM address require a new owned payload.
    ram[17]^=0x55;capture.begin(3,3);capture.bytes("texture0",ram.data(),ram.size());auto changed=capture.finish();
    CHECK(address(changed->blobs[0])!=address(first->blobs[0]));CHECK(first->blobs[0].bytes[17]==17);
    CHECK(changed->blobs[0].bytes[17]==ram[17]);ram[17]^=0x55;
    CHECK(changed->blobs[0].bytes[17]!=ram[17]);
    // A returned frame owns the last snapshot even if its collector is gone.
    std::shared_ptr<const NativeFrame> survivor;
    {SceneCapture temporary;temporary.begin(4,4);temporary.bytes("gamma",ram.data(),ram.size());survivor=temporary.finish();}
    CHECK(survivor->blobs[0].bytes.back()==31);
    if(!disabled) {
        // Frame copies share resources until a mutable operation detaches.
        auto copy=*first;CHECK(address(copy.blobs[0])==address(first->blobs[0]));
        copy.blobs[0].bytes[0]=199;CHECK(address(copy.blobs[0])!=address(first->blobs[0]));CHECK(first->blobs[0].bytes[0]==0);
        copy=*first;copy.blobs[0].bytes.front()=198;CHECK(first->blobs[0].bytes.front()==0);
        copy=*first;copy.blobs[0].bytes.back()=197;CHECK(first->blobs[0].bytes.back()==31);
        copy=*first;copy.blobs[0].bytes.data()[1]=196;CHECK(first->blobs[0].bytes[1]==1);
        copy=*first;std::fill(copy.blobs[0].bytes.begin(),copy.blobs[0].bytes.end(),195);CHECK(first->blobs[0].bytes[16]==16);
        copy=*first;copy.blobs[0].bytes.assign(ram.begin(),ram.end());CHECK(first->blobs[0].bytes[2]==2);
    }
    // Names, lengths and presence belong to this frame, not a prior cache entry.
    capture.begin(5,5);capture.bytes("palette",ram.data(),ram.size());capture.bytes("gamma",ram.data(),4);auto a=capture.finish();
    capture.begin(6,6);capture.bytes("gamma",ram.data(),4);capture.bytes("palette",ram.data(),ram.size());auto b=capture.finish();
    CHECK(b->blobs[0].name=="gamma"&&b->blobs[1].name=="palette");
    CHECK((address(a->blobs[0])==address(b->blobs[1]))==!disabled);
    capture.begin(7,7);capture.bytes("gamma",ram.data(),8);auto resized=capture.finish();CHECK(resized->blobs.size()==1&&resized->blobs[0].bytes.size()==8);
    capture.begin(8,8);CHECK(capture.finish()->blobs.empty());
    capture.begin(9,9);capture.bytes("gamma",ram.data(),4);bool duplicate=false;
    try{capture.bytes("gamma",ram.data(),4);}catch(const std::invalid_argument&){duplicate=true;}CHECK(duplicate);
    // Reusing identical snapshots does not waive logical frame size limits.
    std::vector<uint8_t> large(SceneCapture::MaxBlobBytes,42);
    for(int pass=0;pass<2;pass++) {
        capture.begin(10+pass,10+pass);
        for(unsigned n=0;n<4;n++){auto name="large"+std::to_string(n);capture.bytes(name.c_str(),large.data(),large.size());}
        bool budget=false;try{capture.bytes("extra",ram.data(),1);}catch(const std::invalid_argument&){budget=true;}CHECK(budget);
        auto maximum=capture.finish();CHECK(maximum->blobs.size()==4);
    }
    // Consumer-held resources survive independent producer copies and mutation.
    auto held=first;bool intact=true;
    std::thread consumer([held,&intact]{for(int n=0;n<10000;n++)if(held->blobs[0].bytes[0]!=0||held->blobs[0].bytes.back()!=31)intact=false;});
    for(int n=0;n<100;n++){auto copy=*first;copy.blobs[0].bytes[0]=uint8_t(n);}
    consumer.join();CHECK(intact);
    std::cout<<(disabled?"Disabled cache retains owned snapshots":"Exact resource reuse and mutable detachment")<<" passed\n";
}
