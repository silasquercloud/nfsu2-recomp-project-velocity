#include "velocity_frame_pacer.h"
#include <stdexcept>
#include <cstdio>
void require(bool ok){if(!ok)throw std::runtime_error("Frame deadline regression");}
int main(){
    constexpr uint64_t period=1000000000ull/60,start=1000000000ull;
    velocity::FramePacer normal,experimental;
    require(normal.deadline(start,60,false)==start+period);
    require(experimental.deadline(start,60,true)==start+period);
    uint64_t late=start+5*period;
    require(normal.deadline(late,60,false)==start+2*period);
    require(experimental.deadline(late,60,true)==late);
    require(experimental.deadline(late+1000,60,true)==late+period);
    require(normal.deadline(late,30,false)==late+1000000000ull/30);
    require(experimental.deadline(late+10000,0,true)==late+10000);
    require(experimental.deadline(late+20000,60,true)==late+20000+period);
    require(normal.deadline(start+1000000000,60,false)==start+1000000000+period);
    require(experimental.deadline(start+1000000000,60,false)==start+1000000000+period);
    std::puts("Frame pacing: default catch-up, experimental missed deadline clamp, cap/mode changes and resume passed");
}
