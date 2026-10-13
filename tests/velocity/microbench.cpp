// Synthetic host-only workloads; never substitute for Android gameplay benchmarks.
#include "guest_memory.h"
#include "dxt_decode.h"
#include <chrono>
#include <cstdio>
int main(){
    using Clock=std::chrono::steady_clock;
    GuestMemory memory;memory.commit(0x400000,4u<<20);
    volatile unsigned valid=0;
    auto start=Clock::now();
    for(unsigned i=0;i<10000;++i)valid+=memory.readable(0x400000,4u<<20);
    auto memoryNs=std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now()-start).count();
    constexpr uint32_t width=1024,height=1024;
    std::vector<uint8_t> blocks(size_t(width/4)*(height/4)*8,0xff);
    volatile uint64_t checksum=0;
    start=Clock::now();
    for(unsigned i=0;i<100;++i){
#ifdef VELOCITY_REGION_BENCH
        auto pixels=decodeDXTRegion(1,width,height,blocks.data(),blocks.size(),512,512,4,4);
        checksum+=pixels[0];
#else
        auto pixels=decodeDXT(1,width,height,blocks.data(),blocks.size());
        checksum+=pixels[(512*width+512)*4];
#endif
    }
    auto textureNs=std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now()-start).count();
    std::printf("memory_10000_checks_ns=%lld dxt_100_partial_updates_ns=%lld valid=%u checksum=%llu\n",
        (long long)memoryNs,(long long)textureNs,unsigned(valid),(unsigned long long)checksum);
}
