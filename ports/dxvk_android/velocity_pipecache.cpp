#include "dxvk_pipecache.h"
#include "../util/util_env.h"
#include "velocity_profile.h"
#include <mutex>
#include <algorithm>
namespace {
std::mutex registryMutex;
std::vector<dxvk::DxvkPipelineCache*> caches;
}
namespace dxvk {
DxvkPipelineCache::DxvkPipelineCache(const Rc<vk::DeviceFn>& vkd,const VkPhysicalDeviceProperties& props):m_vkd(vkd){
    m_identity.vendor=props.vendorID;m_identity.device=props.deviceID;
    m_identity.driver=props.driverVersion;m_identity.api=props.apiVersion;
    std::memcpy(m_identity.uuid,props.pipelineCacheUUID,16);
    const auto directory=env::getEnvVar("NFS_VELOCITY_PIPELINE_CACHE_PATH");
    if(!directory.empty())m_path=directory+"/"+velocity::pipelineCacheName(m_identity);
    auto bytes=m_path.empty()?std::vector<uint8_t>{}:velocity::readPipelineCache(m_path,m_identity);
    VkPipelineCacheCreateInfo info{VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO};
    info.initialDataSize=bytes.size();info.pInitialData=bytes.empty()?nullptr:bytes.data();
    VkResult result=m_vkd->vkCreatePipelineCache(m_vkd->device(),&info,nullptr,&m_handle);
    if(result!=VK_SUCCESS&&!bytes.empty()){
        Logger::warn("Velocity: driver rejected cached pipeline data; retrying empty cache");
        info.initialDataSize=0;info.pInitialData=nullptr;m_handle=VK_NULL_HANDLE;
        result=m_vkd->vkCreatePipelineCache(m_vkd->device(),&info,nullptr,&m_handle);bytes.clear();
    }
    if(result!=VK_SUCCESS)throw DxvkError("Velocity: vkCreatePipelineCache failed");
    velocity::metadata("pipeline_cache_loaded_bytes="+std::to_string(bytes.size()));
    std::lock_guard<std::mutex> guard(registryMutex);caches.push_back(this);
}
void DxvkPipelineCache::save(){
    if(m_path.empty())return;
    std::vector<uint8_t> bytes;
    {
        // Compilers share the lock; only a background snapshot needs exclusive
        // access to meet vkGetPipelineCacheData's external synchronization rules.
        std::unique_lock<std::shared_mutex> lock(m_mutex);
        size_t size=0;
        if(m_vkd->vkGetPipelineCacheData(m_vkd->device(),m_handle,&size,nullptr)!=VK_SUCCESS||size<32||size>velocity::maxPipelineCacheBytes)return;
        bytes.resize(size);
        if(m_vkd->vkGetPipelineCacheData(m_vkd->device(),m_handle,&size,bytes.data())!=VK_SUCCESS||size>bytes.size())return;
        bytes.resize(size);
    }
    const bool saved=velocity::writePipelineCache(m_path,bytes,m_identity);
    velocity::metadata("pipeline_cache_saved_bytes="+std::to_string(saved?bytes.size():0));
    if(!saved)Logger::warn("Velocity: pipeline cache snapshot could not be saved");
}
void DxvkPipelineCache::saveAll(){
    // Protect object lifetime while asynchronous Android exports run.
    std::lock_guard<std::mutex> guard(registryMutex);
    for(auto* cache:caches)cache->save();
}
DxvkPipelineCache::~DxvkPipelineCache(){
    std::lock_guard<std::mutex> guard(registryMutex);
    caches.erase(std::remove(caches.begin(),caches.end(),this),caches.end());
    m_vkd->vkDestroyPipelineCache(m_vkd->device(),m_handle,nullptr);
}
}
extern "C" void dxvkAndroidSavePipelineCaches(){dxvk::DxvkPipelineCache::saveAll();}
