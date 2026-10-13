#include "velocity_profile.h"
#include <stdexcept>
#include <fstream>
#include <sstream>
#include <cstdio>
#include <thread>
#include <vector>
#include <unistd.h>
void require(bool ok){if(!ok)throw std::runtime_error("Profiling contract failed");}
int main(){
#ifdef __ANDROID__
    const std::string directory="/data/local/tmp/";
#else
    const std::string directory="/tmp/";
#endif
    const std::string path=directory+"velocity-profile-"+std::to_string(getpid())+".csv";
#if NFS_VELOCITY_PROFILING
    require(velocity::configure(true,path));require(!velocity::exportReport());
    velocity::metadata("gpu=host\ninjected_header");
    require(velocity::vulkanResult(-4)==-4);
    velocity::vulkanResult(1000001003);
    velocity::frame();
    for(unsigned i=0;i<605;++i){
        velocity::record(velocity::Metric::D3D9Sample,7,64);
        velocity::count(velocity::Counter::CopiedBytes,128);
        velocity::draw();velocity::frame();
    }
    require(velocity::exportReport());
    std::ifstream csv(path);std::string line;unsigned rows=0;bool metadataSeen=false,errorsSeen=false;
    while(std::getline(csv,line)){
        if(line=="# gpu=host injected_header")metadataSeen=true;
        if(line=="# session_vulkan_errors=1")errorsSeen=true;
        if(line.empty()||line[0]=='#'||line.rfind("frame,",0)==0)continue;
        std::istringstream row(line);std::vector<uint64_t> values;
        while(std::getline(row,line,','))values.push_back(std::stoull(line));
        require(values.size()==47);require(values[0]==rows+6);require(values[1]>0);
        require(values[3]==1&&values[5]==448&&values[6]==1&&values[42]==128);++rows;
    }
    require(rows==600&&metadataSeen&&errorsSeen);
    velocity::resetFrameClock();velocity::frame();
    std::vector<std::thread> workers;
    for(unsigned i=0;i<4;++i)workers.emplace_back([]{for(unsigned j=0;j<100;++j)velocity::record(velocity::Metric::FenceWait,9);});
    for(auto& worker:workers)worker.join();
    velocity::frame();require(velocity::exportReport());
    std::ifstream concurrent(path);std::string last;
    while(std::getline(concurrent,line))last=line;
    std::istringstream row(last);std::vector<uint64_t> values;
    while(std::getline(row,line,','))values.push_back(std::stoull(line));
    require(values[21]==3600&&values[22]==400);
    require(velocity::configure(true,path));velocity::frame();
    {velocity::Scope timed(velocity::Metric::Present);}
    {velocity::Scope unsampled(velocity::Metric::Present,1,false);}
    velocity::frame();require(velocity::exportReport());
    std::ifstream scopes(path);
    while(std::getline(scopes,line))last=line;
    std::istringstream scopeRow(last);values.clear();
    while(std::getline(scopeRow,line,','))values.push_back(std::stoull(line));
    require(values[7]>0&&values[8]==1);
    require(!velocity::configure(false,path));require(!std::ifstream(path).good());
    velocity::draw();velocity::frame();require(!velocity::exportReport());
    require(velocity::configure(true,"/nonexistent/velocity/report.csv"));velocity::frame();velocity::frame();
    require(!velocity::exportReport());
    std::puts("Profiling: bounded ring, estimates, draw count, concurrent events, reset, disabled and failed export passed");
#else
    require(!velocity::configure(true,path));
    velocity::Scope noOp(velocity::Metric::Present);velocity::draw();velocity::frame();
    require(!velocity::enabled()&&!velocity::exportReport());
    std::puts("Profiling compiled out: no report and no active profiler passed");
#endif
    std::remove(path.c_str());
}
