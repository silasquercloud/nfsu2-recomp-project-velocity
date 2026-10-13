#include "velocity_frame_stats.h"
#include <stdexcept>
#include <cmath>
#include <cstdio>
void require(bool ok){if(!ok)throw std::runtime_error("Presentation FPS contract failed");}
int main(){
    velocity::FrameStats frames;
    require(!frames.present(1000000000));
    for(unsigned i=1;i<=30;i++)frames.present(1000000000ull+i*16666667ull);
    require(std::abs(frames.fps()-60)<.001&&std::abs(frames.averageFps()-60)<.001);
    require(std::abs(frames.lastMs()-16.666667)<.00001);
    frames.reset();uint64_t t=2000000000;frames.present(t);
    for(unsigned i=0;i<700;i++){t+=50000000;frames.present(t);}
    require(std::abs(frames.fps()-20)<.001&&std::abs(frames.averageFps()-20)<.001);
    t+=1000000000;frames.present(t);
    require(frames.lastMs()==1000&&frames.averageFps()<20);
    frames.reset();frames.present(t+20000000000ull);
    require(frames.averageFps()==0);
    require(!frames.present(1)&&frames.averageFps()==0);
    std::puts("FPS: measured intervals, 60/20Hz, bounded rolling mean, real stutter, pause reset and nonmonotonic clock passed");
}
