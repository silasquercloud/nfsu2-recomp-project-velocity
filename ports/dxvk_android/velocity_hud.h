#pragma once
#include "dxvk_hud_item.h"
#include "velocity_frame_stats.h"
#include <ctime>
#include <iomanip>
namespace dxvk::hud {
extern "C" uint32_t dxvkAndroidHudEpoch();
class HudVelocityItem : public HudItem {
    Rc<DxvkDevice> m_device;
    velocity::FrameStats m_frames;
    bool m_detailed;
    uint32_t m_epoch=0;
    uint64_t m_lastCpu=0,m_cpuFrames=0;
    std::string m_gpu,m_api,m_fps="-- FPS",m_frame,m_average,m_cpu,m_percentiles,m_low;
    static uint64_t cpuNs(){timespec t{};if(clock_gettime(CLOCK_PROCESS_CPUTIME_ID,&t))return 0;return uint64_t(t.tv_sec)*1000000000ull+t.tv_nsec;}
public:
    HudVelocityItem(const Rc<DxvkDevice>& device,bool detailed):m_device(device),m_detailed(detailed){
        const auto& p=device->adapter()->deviceProperties();m_gpu=p.deviceName;
        m_api=str::format("Vulkan ",VK_VERSION_MAJOR(p.apiVersion),".",VK_VERSION_MINOR(p.apiVersion),".",VK_VERSION_PATCH(p.apiVersion));
    }
    void update(dxvk::high_resolution_clock::time_point time)override{
        auto epoch=dxvkAndroidHudEpoch();
        if(epoch!=m_epoch){m_epoch=epoch;m_frames.reset();m_lastCpu=0;m_cpuFrames=0;m_fps="-- FPS";m_frame.clear();m_average.clear();m_cpu.clear();m_percentiles.clear();m_low.clear();}
        ++m_cpuFrames;
        if(!m_frames.present(uint64_t(std::chrono::duration_cast<std::chrono::nanoseconds>(time.time_since_epoch()).count())))return;
        // Strings and process CPU sampling update at most twice per second.
        m_fps=str::format(std::fixed,std::setprecision(1),m_frames.fps()," FPS");
        if(!m_detailed)return;
        m_frame=str::format(std::fixed,std::setprecision(1),"Frame: ",m_frames.lastMs()," ms");
        m_average=str::format(std::fixed,std::setprecision(1),"Avg: ",m_frames.averageFps()," FPS (last 600)");
        const auto d=m_frames.distribution();
        m_percentiles=str::format(std::fixed,std::setprecision(1),"P50/95/99: ",d.p50," / ",d.p95," / ",d.p99," ms");
        m_low=str::format(std::fixed,std::setprecision(1),"1% low: ",d.low1," FPS");
        auto cpu=cpuNs();
        if(m_lastCpu&&cpu>=m_lastCpu)m_cpu=str::format(std::fixed,std::setprecision(2),"CPU: ",double(cpu-m_lastCpu)/double(m_cpuFrames)/1e6," ms/frame (process)");
        m_lastCpu=cpu;m_cpuFrames=0;
    }
    HudPos render(HudRenderer& renderer,HudPos p)override{
        const std::string* lines[]={&m_gpu,&m_fps,&m_api,&m_frame,&m_average,&m_cpu,&m_percentiles,&m_low};
        for(unsigned i=0;i<(m_detailed?8u:3u);++i){
            if(lines[i]->empty())continue;p.y+=20;
            renderer.drawText(16,{p.x,p.y},{1,1,1,1},*lines[i]);
        }
        p.y+=8;return p;
    }
};
}
