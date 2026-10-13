#include "velocity_profile.h"
#include "velocity_frame_stats.h"
#if NFS_VELOCITY_PROFILING
#include <array>
#include <atomic>
#include <mutex>
#include <cstdio>
#include <ctime>
#include <thread>
#ifdef __ANDROID__
#include <android/trace.h>
#endif
namespace velocity {
namespace {
constexpr size_t metrics=static_cast<size_t>(Metric::Count),counters=static_cast<size_t>(Counter::Count),capacity=600;
constexpr const char* names[]={"d3d9_sample_estimate","d3d9_present","texture_decode",
    "texture_upload_copy","scheduler_wait","frame_limiter","vulkan_submit_host",
    "vulkan_present_host","fence_wait_host","pipeline_compile_host","pipeline_bind",
    "texture_lock_host","buffer_lock_host","resource_wait_host","cs_sync_host",
    "vulkan_acquire_host","shader_translate_host","import_lookup_estimate","pipeline_ready_wait_host"};
constexpr const char* counterNames[]={"decoded_pixels","copied_bytes","texture_fallbacks",
    "vulkan_errors","vulkan_suboptimal","swapchain_recreates","pipeline_object_hits","pipeline_object_misses","prewarm_failures","compiler_wait_over_2s"};
std::atomic<bool> active{false};
std::array<std::atomic<uint64_t>,metrics> times{},counts{};
std::array<std::atomic<uint64_t>,counters> events{},totals{};
std::atomic<uint64_t> draws{0};
struct Frame {uint64_t id{},wall{},cpu{},threadCpu{},draws{};std::array<uint64_t,metrics> times{},counts{};std::array<uint64_t,counters> events{};};
std::array<Frame,capacity> history{};
std::mutex historyMutex;
size_t used=0,cursor=0;
uint64_t lastWall=0,lastCpu=0,lastThreadCpu=0,sequence=0;
std::thread::id lastThread;
std::string reportPath;
std::string deviceMetadata;
uint64_t clockNs(clockid_t id){timespec t{};if(clock_gettime(id,&t))return 0;return uint64_t(t.tv_sec)*1000000000ull+uint64_t(t.tv_nsec);}
}
bool enabled(){return active.load(std::memory_order_relaxed);}
uint64_t now(){return clockNs(CLOCK_MONOTONIC);}
void record(Metric metric,uint64_t elapsed,uint64_t weight){
    const auto index=static_cast<size_t>(metric);
    times[index].fetch_add(elapsed*weight,std::memory_order_relaxed);
    counts[index].fetch_add(1,std::memory_order_relaxed);
}
void count(Counter counter,uint64_t amount){
    if(!enabled())return;
    const auto i=static_cast<size_t>(counter);
    events[i].fetch_add(amount,std::memory_order_relaxed);
    totals[i].fetch_add(amount,std::memory_order_relaxed);
}
int32_t vulkanResult(int32_t result){
    if(result<0)count(Counter::VulkanErrors);
    if(result==1000001003)count(Counter::VulkanSuboptimal);
    return result;
}
void metadata(const std::string& value){
    if(!enabled())return;
    std::lock_guard<std::mutex> lock(historyMutex);
    if(deviceMetadata.size()+value.size()>8192)return;
    deviceMetadata+="# ";
    for(char c:value)deviceMetadata+=(c=='\n'||c=='\r')?' ':c;
    deviceMetadata+='\n';
}
void Scope::begin(){
    start_=now();
#ifdef __ANDROID__
    trace_=ATrace_isEnabled();
    if(trace_)ATrace_beginSection(names[static_cast<unsigned>(metric_)]);
#endif
}
void Scope::finish(){
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
    reportPath=path;deviceMetadata.clear();used=cursor=0;lastWall=lastCpu=lastThreadCpu=sequence=0;
    for(size_t i=0;i<metrics;++i){times[i]=0;counts[i]=0;}draws=0;
    for(size_t i=0;i<counters;++i){events[i]=0;totals[i]=0;}
    // Remove a previous run so a no-frame run cannot export stale measurements.
    if(!reportPath.empty())std::remove(reportPath.c_str());
    active.store(enable&&!path.empty(),std::memory_order_relaxed);
    return enabled();
}
void frame(){
    if(!enabled())return;
    const uint64_t wall=now(),cpu=clockNs(CLOCK_PROCESS_CPUTIME_ID),threadCpu=clockNs(CLOCK_THREAD_CPUTIME_ID);
    const auto thread=std::this_thread::get_id();
    Frame f;f.draws=draws.exchange(0,std::memory_order_relaxed);
    for(size_t i=0;i<metrics;++i){f.times[i]=times[i].exchange(0,std::memory_order_relaxed);f.counts[i]=counts[i].exchange(0,std::memory_order_relaxed);}
    for(size_t i=0;i<counters;++i)f.events[i]=events[i].exchange(0,std::memory_order_relaxed);
    std::lock_guard<std::mutex> lock(historyMutex);
    if(lastWall){f.id=++sequence;f.wall=wall-lastWall;f.cpu=cpu-lastCpu;f.threadCpu=thread==lastThread&&threadCpu>=lastThreadCpu?threadCpu-lastThreadCpu:0;
        history[cursor]=f;cursor=(cursor+1)%capacity;if(used<capacity)++used;}
    lastWall=wall;lastCpu=cpu;lastThreadCpu=threadCpu;lastThread=thread;
}
void resetFrameClock(){std::lock_guard<std::mutex> lock(historyMutex);lastWall=lastCpu=lastThreadCpu=0;}
bool exportReport(){
    static std::mutex exportMutex;
    std::lock_guard<std::mutex> exporting(exportMutex);
    std::array<Frame,capacity> snapshot;size_t count;std::string path,metadata;
    {std::lock_guard<std::mutex> lock(historyMutex);count=used;path=reportPath;metadata=deviceMetadata;
        for(size_t i=0;i<count;++i)snapshot[i]=history[(cursor+capacity-count+i)%capacity];}
    if(!enabled()||path.empty()||!count)return false;
    const std::string temporary=path+".tmp";
    FILE* out=std::fopen(temporary.c_str(),"w");if(!out)return false;
    std::fprintf(out,"# Project Velocity 0.14.0-velocity.4; last 600 completed presentation intervals; timing units ns\n"
        "# cpu_process includes all native/DXVK threads; host durations are NOT GPU timestamps\n"
        "# d3d9 estimated from 1/64 calls, excluding Present; asynchronous metrics use completion interval\n"
        "# import lookup estimated from 1/256 calls; not full HLE execution\n"
        "# nested/parallel metrics overlap; no exact guest runtime attribution; first interval discarded\n"
        "# vulkan_present_host measures vkQueuePresentKHR; acquisition measured separately\n");
    FrameStats stats;uint64_t elapsed=1,compilations=0,translations=0,compileOverlap=0;stats.present(elapsed);
    for(size_t i=0;i<count;++i){const auto& f=snapshot[i];elapsed+=f.wall;stats.present(elapsed);
        auto n=f.counts[static_cast<size_t>(Metric::PipelineCompile)];compilations+=n;translations+=f.counts[static_cast<size_t>(Metric::ShaderTranslate)];
        if(f.wall>33333333&&n)++compileOverlap;
    }
    const auto d=stats.distribution();
    std::fprintf(out,"# avg_fps=%.3f; low_1_percent_fps=%.3f; p50_ms=%.3f; p95_ms=%.3f; p99_ms=%.3f\n",
        stats.averageFps(),d.low1,d.p50,d.p95,d.p99);
    std::fprintf(out,"# pipeline_compilations_per_second=%.3f; slow_intervals_with_compile_completion=%llu; correlation_not_causation\n",
        elapsed>1?double(compilations)*1e9/double(elapsed-1):0,(unsigned long long)compileOverlap);
    std::fprintf(out,"# shader_translations_per_second=%.3f\n",elapsed>1?double(translations)*1e9/double(elapsed-1):0);
    std::fputs("# pipeline_object_hits/misses refer to DXVK in-memory objects; opaque driver cache hits unavailable\n"
        "# gpu_execution_ns=unavailable; timestamp support alone does not measure execution\n",out);
    std::fputs(metadata.c_str(),out);
    for(size_t i=0;i<counters;++i)std::fprintf(out,"# session_%s=%llu\n",counterNames[i],(unsigned long long)totals[i].load(std::memory_order_relaxed));
    std::fputs("frame,wall_ns,cpu_process_ns,draw_calls,over_33ms",out);
    for(auto name:names)std::fprintf(out,",%s_ns,%s_samples",name,name);
    for(auto name:counterNames)std::fprintf(out,",%s",name);
    std::fputs(",present_calling_thread_cpu_ns",out);
    std::fputc('\n',out);
    for(size_t j=0;j<count;++j){const auto& f=snapshot[j];
        std::fprintf(out,"%llu,%llu,%llu,%llu,%u",(unsigned long long)f.id,(unsigned long long)f.wall,
            (unsigned long long)f.cpu,(unsigned long long)f.draws,f.wall>33333333?1u:0u);
        for(size_t i=0;i<metrics;++i)std::fprintf(out,",%llu,%llu",(unsigned long long)f.times[i],(unsigned long long)f.counts[i]);
        for(auto value:f.events)std::fprintf(out,",%llu",(unsigned long long)value);
        std::fprintf(out,",%llu",(unsigned long long)f.threadCpu);
        std::fputc('\n',out);
    }
    const bool wrote=!std::ferror(out);const bool closed=std::fclose(out)==0;
    if(!wrote||!closed||std::rename(temporary.c_str(),path.c_str())){std::remove(temporary.c_str());return false;}
    return true;
}
}
#endif
