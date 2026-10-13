#pragma once
#include <shared_mutex>
#include "dxvk_include.h"
#include "velocity_pipeline_cache.h"
namespace dxvk {
class DxvkPipelineCache : public RcObject {
public:
    DxvkPipelineCache(const Rc<vk::DeviceFn>& vkd,const VkPhysicalDeviceProperties& properties);
    ~DxvkPipelineCache();
    VkPipelineCache handle()const{return m_handle;}
    std::shared_lock<std::shared_mutex> compilationLock(){return std::shared_lock<std::shared_mutex>(m_mutex);}
    static void saveAll();
private:
    void save();
    Rc<vk::DeviceFn> m_vkd;
    VkPipelineCache m_handle=VK_NULL_HANDLE;
    velocity::PipelineIdentity m_identity{};
    std::string m_path;
    std::shared_mutex m_mutex;
};
}
