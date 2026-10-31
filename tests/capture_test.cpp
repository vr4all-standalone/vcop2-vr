#include "../native/model2_capture.h"
#include <iostream>
#include <limits>
#define CHECK(v) do{if(!(v)){std::cerr<<"Failed: " #v " at "<<__LINE__<<'\n';return 1;}}while(0)
int main(){
    using namespace vcop2;
    SceneCapture capture;capture.begin(1,5.0);
    NativePolygon polygon;polygon.count=3;polygon.focus={300,300};polygon.vertices[0]={1,2,3,4,5};polygon.vertices[1]={2,2,3,4,5};polygon.vertices[2]={1,3,3,4,5};
    polygon.culled=true;polygon.cull_reasons=CullKnown|CullBehind;
    polygon.vertices[0].z=-3;
    CHECK(capture.append(polygon));
    std::array<uint8_t,4> texture{1,2,3,4};capture.bytes("texture0",texture.data(),texture.size());
    auto frame=capture.finish();CHECK(frame->polygons.size()==1);CHECK(frame->blobs[0].bytes[0]==1);
    texture[0]=99;polygon.vertices[0].x=99;CHECK(frame->polygons[0].vertices[0].x==1);CHECK(frame->blobs[0].bytes[0]==1);
    CHECK(frame->polygons[0].vertices[0].z==-3);
    CHECK(frame->polygons[0].cull_reasons==(CullKnown|CullBehind));
    capture.begin(2,6);CHECK(capture.finish()->polygons.empty());CHECK(frame->sequence==1);
    capture.begin(3,7);polygon.vertices[0].x=std::numeric_limits<float>::quiet_NaN();CHECK(!capture.append(polygon));
    polygon.vertices[0].x=1;polygon.count=5;CHECK(!capture.append(polygon));
    polygon.count=3;for(unsigned i=0;i<SceneCapture::MaxPolygons;i++)CHECK(capture.append(polygon));CHECK(!capture.append(polygon));
    bool rejected=false;try{capture.bytes("oversize",texture.data(),SceneCapture::MaxBlobBytes+1);}catch(const std::invalid_argument&){rejected=true;}CHECK(rejected);
    {
        NativeFrame recycled;recycled.sequence=77;recycled.time=3;recycled.xoffset=11;recycled.yoffset=22;recycled.width=496;recycled.height=384;
        recycled.complete=false;recycled.raster_only=true;recycled.lens={496,384,200,200,248,192};recycled.rejected=13;
        recycled.sky_width=512;recycled.sky_height=512;
        recycled.background.assign(64,1);recycled.foreground.assign(32,2);recycled.native.assign(64,3);recycled.sky.assign(128,4);
        recycled.polygons.reserve(256);recycled.polygons.push_back(polygon);
        recycled.blobs.push_back({"texture0",std::vector<uint8_t>(64,5)});recycled.blobs.push_back({"gamma",std::vector<uint8_t>(128,6)});
        const auto* texture_storage=std::as_const(recycled.blobs[0].bytes).data();const auto* gamma_storage=std::as_const(recycled.blobs[1].bytes).data();
        const auto* raster_storage=recycled.background.data();const auto polygon_capacity=recycled.polygons.capacity();
        capture.begin(100,9,std::move(recycled));
        // Reordering inputs must reuse their corresponding buffers, and an
        // old unused name is not a duplicate in the next snapshot.
        capture.bytes("gamma",texture.data(),texture.size());capture.bytes("texture0",texture.data(),texture.size());
        bool duplicate=false;try{capture.bytes("gamma",texture.data(),texture.size());}catch(const std::invalid_argument&){duplicate=true;}CHECK(duplicate);
        auto reused=capture.take();CHECK(reused.sequence==100&&reused.time==9);
        CHECK(reused.complete&&!reused.raster_only&&!reused.lens.valid()&&reused.rejected==0);
        CHECK(reused.width==0&&reused.height==0&&reused.xoffset==0&&reused.yoffset==0);
        CHECK(reused.sky_width==0&&reused.sky_height==0&&reused.sky.empty());
        CHECK(reused.background.empty()&&reused.foreground.empty()&&reused.native.empty());
        CHECK(reused.polygons.empty()&&reused.polygons.capacity()==polygon_capacity);
        CHECK(reused.blobs.size()==2&&reused.blobs[0].name=="gamma"&&std::as_const(reused.blobs[0].bytes).data()==gamma_storage);
        CHECK(reused.blobs[1].name=="texture0"&&std::as_const(reused.blobs[1].bytes).data()==texture_storage);
        CHECK(reused.blobs[0].bytes==std::vector<uint8_t>(texture.begin(),texture.end()));
        reused.background.resize(64);CHECK(reused.background.data()==raster_storage);
        capture.begin(101,10,std::move(reused));capture.bytes("texture0",texture.data(),texture.size());
        auto fewer=capture.take();CHECK(fewer.blobs.size()==1&&fewer.blobs[0].name=="texture0");
        CHECK(std::as_const(fewer.blobs[0].bytes).data()==texture_storage);
        capture.begin(102,11,std::move(fewer));CHECK(capture.take().blobs.empty());
    }
    {
        capture.begin(103,12);
        for(unsigned n=0;n<SceneCapture::MaxBlobs;n++){auto name="blob"+std::to_string(n);capture.bytes(name.c_str(),texture.data(),1);}
        auto full=capture.take();capture.begin(104,13,std::move(full));
        // A previously full frame may begin a new used prefix at zero.
        capture.bytes("new_blob",texture.data(),1);auto sparse=capture.take();CHECK(sparse.blobs.size()==1);
        capture.begin(105,14,std::move(sparse));
        for(unsigned n=0;n<SceneCapture::MaxBlobs;n++){auto name="blob"+std::to_string(n);capture.bytes(name.c_str(),texture.data(),1);}
        bool too_many=false;try{capture.bytes("extra",texture.data(),1);}catch(const std::invalid_argument&){too_many=true;}CHECK(too_many);
        capture.begin(106,15);std::vector<uint8_t> large(SceneCapture::MaxBlobBytes,17);
        for(unsigned n=0;n<SceneCapture::MaxFrameBytes/SceneCapture::MaxBlobBytes;n++){auto name="large"+std::to_string(n);capture.bytes(name.c_str(),large.data(),large.size());}
        bool budget=false;try{capture.bytes("extra",texture.data(),1);}catch(const std::invalid_argument&){budget=true;}CHECK(budget);
    }
    std::cout<<"Owned Model 2 scene data, finite vertices and size bounds passed\n";
}
