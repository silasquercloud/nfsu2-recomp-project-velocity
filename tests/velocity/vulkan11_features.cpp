#include "vulkan_compat.h"
#include <stdexcept>
#include <cstdio>
void require(bool ok){if(!ok)throw std::runtime_error("Vulkan 1.1 capability mask regression");}
int main(){
    VkPhysicalDeviceFeatures none{};
    auto empty=nfsProbeDeviceFeatures(none);
    require(!empty.geometryShader&&!empty.multiViewport&&!empty.textureCompressionBC);
    VkPhysicalDeviceFeatures adreno{};
    adreno.robustBufferAccess=VK_TRUE;adreno.fullDrawIndexUint32=VK_TRUE;
    adreno.depthClamp=VK_TRUE;adreno.samplerAnisotropy=VK_TRUE;
    // Older system Adreno drivers may lack BC and multiple viewports.
    auto requested=nfsProbeDeviceFeatures(adreno);
    require(requested.robustBufferAccess&&requested.fullDrawIndexUint32);
    require(requested.depthClamp&&requested.samplerAnisotropy);
    require(!requested.multiViewport&&!requested.textureCompressionBC);
    VkPhysicalDeviceFeatures mali=adreno;
    mali.textureCompressionETC2=VK_TRUE;mali.textureCompressionASTC_LDR=VK_TRUE;
    auto fallback=nfsProbeDeviceFeatures(mali);
    require(!fallback.shaderClipDistance&&!fallback.shaderCullDistance);
    require(!fallback.geometryShader&&!fallback.sampleRateShading);
    require(!fallback.textureCompressionBC);
    VkPhysicalDeviceFeatures capable=mali;
    capable.textureCompressionBC=VK_TRUE;capable.geometryShader=VK_TRUE;
    capable.multiViewport=VK_TRUE;capable.shaderClipDistance=VK_TRUE;
    capable.sampleRateShading=VK_TRUE;
    auto full=nfsProbeDeviceFeatures(capable);
    require(full.textureCompressionBC&&full.geometryShader&&full.multiViewport);
    require(full.shaderClipDistance&&full.sampleRateShading);
    // No unrelated device feature is enabled just because it is available.
    require(!full.textureCompressionETC2&&!full.textureCompressionASTC_LDR);
    std::puts("Vulkan 1.1 feature-mask tests passed: missing BC/viewport/clip, optional feature support and no unsupported requests (synthetic capabilities, not GPU execution)");
}
