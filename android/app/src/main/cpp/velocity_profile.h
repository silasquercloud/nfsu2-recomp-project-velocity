#pragma once
#include <cstdint>
#include <string>
#ifndef NFS_VELOCITY_PROFILING
#define NFS_VELOCITY_PROFILING 0
#endif
namespace velocity {
enum class Metric : unsigned { D3D9Sample, Present, TextureDecode, TextureUpload,
    SchedulerWait, FrameLimiter, VulkanSubmit, VulkanPresent, FenceWait,
    PipelineCompile, PipelineBind, TextureLock, BufferLock, ResourceWait, CsWait,
    VulkanAcquire, ShaderTranslate, ImportLookup, PipelineWait, Count };
enum class Counter : unsigned { DecodedPixels, CopiedBytes, TextureFallbacks,
    VulkanErrors, VulkanSuboptimal, SwapchainRecreates, PipelineHits, PipelineMisses, PrewarmFailures, CompileTimeouts, Count };
#if NFS_VELOCITY_PROFILING
bool enabled();
uint64_t now();
void record(Metric metric,uint64_t elapsed,uint64_t weight=1);
void count(Counter counter,uint64_t amount=1);
void metadata(const std::string& value);
int32_t vulkanResult(int32_t result);
void draw();
void frame();
void resetFrameClock();
bool configure(bool enable,const std::string& path);
bool exportReport();
class Scope {
    Metric metric_;uint64_t start_=0,weight_;
#ifdef __ANDROID__
    bool trace_=false;
#endif
    void begin();
    void finish();
public:
    explicit Scope(Metric metric,uint64_t weight=1,bool sample=true):metric_(metric),weight_(weight){if(sample&&enabled())begin();}
    ~Scope(){if(start_)finish();}
    Scope(const Scope&)=delete;
    Scope& operator=(const Scope&)=delete;
};
#else
inline bool enabled(){return false;}
inline void count(Counter,uint64_t=1){}
inline void metadata(const std::string&){}
inline int32_t vulkanResult(int32_t result){return result;}
inline void draw(){}
inline void frame(){}
inline void resetFrameClock(){}
inline bool configure(bool,const std::string&){return false;}
inline bool exportReport(){return false;}
class Scope {public: explicit Scope(Metric,uint64_t=1,bool=true) {}};
#endif
}
