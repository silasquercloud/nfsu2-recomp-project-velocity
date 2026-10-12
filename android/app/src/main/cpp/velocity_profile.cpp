#include "velocity_profile.h"
#if NFS_VELOCITY_PROFILING
#include <array>
#include <atomic>
#include <mutex>
#include <cstdio>
#include <ctime>
#ifdef __ANDROID__
#include <android/trace.h>
#endif
namespace velocity {
namespace {
constexpr size_t metrics=static_cast<size_t>(Metric::Count),capacity=600;
constexpr const char* names[]={"d3d9_sample_estimate","d3d9_present","texture_decode",
    "texture_upload_copy","scheduler_wait","frame_limiter","vulkan_submit_host",
    "vulkan_present_host","fence_wait_host","pipeline_compile_host","pipeline_bind"};
std::atomic<bool> active{false};
std::array<std::atomic<uint64_t>,metrics> times{},counts{};
std::atomic<uint64_t> draws{0};
struct Frame {uint64_t id{},wall{},cpu{},draws{};std::array<uint64_t,metrics> times{},counts{};};
std::array<Frame,capacity> history{};
std::mutex historyMutex;
size_t used=0,cursor=0;
uint64_t lastWall=0,lastCpu=0,sequence=0;
std::string reportPath;
uint64_t clockNs(clockid_t id){timespec t{};if(clock_gettime(id,&t))return 0;return uint64_t(t.tv_sec)*1000000000ull+uint64_t(t.tv_nsec);}
}
bool enabled(){return active.load(std::memory_order_relaxed);}
uint64_t now(){return clockNs(CLOCK_MONOTONIC);}
void record(Metric metric,uint64_t elapsed,uint64_t weight){
    const auto index=static_cast<size_t>(metric);
    times[index].fetch_add(elapsed*weight,std::memory_order_relaxed);
    counts[index].fetch_add(1,std::memory_order_relaxed);
}
Scope::Scope(Metric metric,uint64_t weight,bool sample):metric_(metric),weight_(weight){
    if(!sample||!enabled())return;
    start_=now();
#ifdef __ANDROID__
    trace_=ATrace_isEnabled();
    if(trace_)ATrace_beginSection(names[static_cast<unsigned>(metric)]);
#endif
}
Scope::~Scope(){
    if(!start_)return;
#ifdef __ANDROID__
    if(trace_)ATrace_endSection();
#endif
    record(metric_,now()-start_,weight_);
}
void draw(){if(enabled())draws.fetch_add(1,std::memory_order_relaxed);}
bool configure(bool enable,const std::string& path){
    // Startup only, before guest threads. Do not reconfigure while scopes run.
    active.store(false,std::memory_order_relaxed);
    std::lock_guard<std::mutex> lock(historyMutex);
    reportPath=path;used=cursor=0;lastWall=lastCpu=sequence=0;
    for(size_t i=0;i<metrics;++i){times[i]=0;counts[i]=0;}draws=0;
    // Remove a previous run so a no-frame run cannot export stale measurements.
    if(!reportPath.empty())std::remove(reportPath.c_str());
    active.store(enable&&!path.empty(),std::memory_order_relaxed);
    return enabled();
}
void frame(){
    if(!enabled())return;
    const uint64_t wall=now(),cpu=clockNs(CLOCK_PROCESS_CPUTIME_ID);
    Frame f;f.draws=draws.exchange(0,std::memory_order_relaxed);
    for(size_t i=0;i<metrics;++i){f.times[i]=times[i].exchange(0,std::memory_order_relaxed);f.counts[i]=counts[i].exchange(0,std::memory_order_relaxed);}
    std::lock_guard<std::mutex> lock(historyMutex);
    if(lastWall){f.id=++sequence;f.wall=wall-lastWall;f.cpu=cpu-lastCpu;
        history[cursor]=f;cursor=(cursor+1)%capacity;if(used<capacity)++used;}
    lastWall=wall;lastCpu=cpu;
}
void resetFrameClock(){std::lock_guard<std::mutex> lock(historyMutex);lastWall=lastCpu=0;}
bool exportReport(){
    static std::mutex exportMutex;
    std::lock_guard<std::mutex> exporting(exportMutex);
    std::array<Frame,capacity> snapshot;size_t count;std::string path;
    {std::lock_guard<std::mutex> lock(historyMutex);count=used;path=reportPath;
        for(size_t i=0;i<count;++i)snapshot[i]=history[(cursor+capacity-count+i)%capacity];}
    if(!enabled()||path.empty()||!count)return false;
    const std::string temporary=path+".tmp";
    FILE* out=std::fopen(temporary.c_str(),"w");if(!out)return false;
    std::fprintf(out,"# Project Velocity 0.14.0-velocity.1; last 600 completed presentation intervals; units ns\n"
        "# cpu_process includes all native/DXVK threads; host durations are NOT GPU timestamps\n"
        "# d3d9 estimated from 1/64 calls, excluding Present; asynchronous metrics use completion interval\n"
        "# nested/parallel metrics overlap; no exact guest runtime attribution; first interval discarded\n"
        "frame,wall_ns,cpu_process_ns,draw_calls,over_33ms");
    for(auto name:names)std::fprintf(out,",%s_ns,%s_samples",name,name);
    std::fputc('\n',out);
    for(size_t j=0;j<count;++j){const auto& f=snapshot[j];
        std::fprintf(out,"%llu,%llu,%llu,%llu,%u",(unsigned long long)f.id,(unsigned long long)f.wall,
            (unsigned long long)f.cpu,(unsigned long long)f.draws,f.wall>33333333?1u:0u);
        for(size_t i=0;i<metrics;++i)std::fprintf(out,",%llu,%llu",(unsigned long long)f.times[i],(unsigned long long)f.counts[i]);
        std::fputc('\n',out);
    }
    const bool wrote=!std::ferror(out);const bool closed=std::fclose(out)==0;
    if(!wrote||!closed||std::rename(temporary.c_str(),path.c_str())){std::remove(temporary.c_str());return false;}
    return true;
}
}
#endif
