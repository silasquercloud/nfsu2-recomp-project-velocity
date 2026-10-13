#pragma once
#include <array>
#include <algorithm>
#include <cstdint>
namespace velocity {
// Completed presentation intervals, not display refresh or a GPU timestamp.
class FrameStats {
    static constexpr unsigned capacity=600;
    std::array<uint64_t,capacity> intervals_{};
    unsigned used_=0,cursor_=0;
    uint64_t previous_=0,sum_=0,windowStart_=0,windowFrames_=0,last_=0;
    double fps_=0;
public:
    void reset(){used_=cursor_=0;previous_=sum_=windowStart_=windowFrames_=last_=0;fps_=0;}
    bool present(uint64_t ns){
        if(!previous_){previous_=windowStart_=ns;return false;}
        if(ns<=previous_){reset();previous_=windowStart_=ns;return false;}
        last_=ns-previous_;previous_=ns;
        if(used_==capacity)sum_-=intervals_[cursor_];else ++used_;
        intervals_[cursor_]=last_;sum_+=last_;cursor_=(cursor_+1)%capacity;
        ++windowFrames_;
        if(ns-windowStart_<500000000ull)return false;
        fps_=double(windowFrames_)*1e9/double(ns-windowStart_);
        windowStart_=ns;windowFrames_=0;return true;
    }
    struct Distribution {double p50=0,p95=0,p99=0,low1=0;};
    Distribution distribution()const {
        if(!used_)return {};
        auto ordered=intervals_;std::sort(ordered.begin(),ordered.begin()+used_);
        auto percentile=[&](unsigned p){return double(ordered[(used_*p+99)/100-1])/1e6;};
        const unsigned tail=std::max(1u,(used_+99)/100);uint64_t total=0;
        for(unsigned i=used_-tail;i<used_;++i)total+=ordered[i];
        return {percentile(50),percentile(95),percentile(99),total?double(tail)*1e9/double(total):0};
    }
    double fps()const{return fps_;}
    double averageFps()const{return sum_?double(used_)*1e9/double(sum_):0;}
    double lastMs()const{return double(last_)/1e6;}
};
}
