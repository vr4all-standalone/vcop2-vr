#include "../native/model2_trace.h"
#include <chrono>
#include <iostream>

#define CHECK(v) do{if(!(v)){std::cerr<<"Failed: " #v " at "<<__LINE__<<'\n';return 1;}}while(0)
int main(int argc,char** argv) {
    using namespace vcop2;
    if(argc!=2)return 2;
    const auto root=std::filesystem::path(argv[1])/("trace-commit-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    NativeFrame frame;frame.sequence=7;frame.width=2;frame.height=2;
    write_trace(frame,root);
    const auto destination=trace_pending_raster();
    trace_raster("foreground",2,2,[](int,int){return 0u;});
    trace_raster("native",2,2,[](int,int){return 0xff123456u;});
    bool refused=false;
    try{commit_trace_rasters();}catch(const std::exception&){refused=true;}
    CHECK(refused);
    CHECK(std::filesystem::exists(destination/"metadata.partial"));
    CHECK(!std::filesystem::exists(destination/"metadata.json"));
    trace_raster("background",2,2,[](int,int){return 0xff000000u;});
    commit_trace_rasters();
    CHECK(std::filesystem::exists(destination/"metadata.json"));
    CHECK(!std::filesystem::exists(destination/"metadata.partial"));
    std::cout<<"Incomplete raster capture rejected; complete capture committed\n";
}
