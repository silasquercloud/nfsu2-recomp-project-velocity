#pragma once
#include <cstdint>
namespace velocity {
// CPU frame cap only: does not change Vulkan present mode or synchronization.
class FramePacer {
    uint64_t next_=0;
    unsigned cap_=0;
    bool experimental_=false;
public:
    uint64_t deadline(uint64_t now,unsigned cap,bool experimental){
        if(cap!=cap_||experimental!=experimental_||now>next_+200000000ull){
            cap_=cap;experimental_=experimental;next_=now;
        }
        if(!cap)return now;
        next_+=1000000000ull/cap;
        // Do not repay missed presentation deadlines with a burst of uncapped
        // frames. Preserve the upstream catch-up behavior in default mode.
        if(experimental&&next_<now)next_=now;
        return next_;
    }
};
}
