#pragma once
#include <vulkan/vulkan.h>

// Core features used by this D3D9 backend. Optional features follow the selected
// driver's capabilities; this mask never requests a newer Vulkan API/extension.
inline VkPhysicalDeviceFeatures nfsProbeDeviceFeatures(const VkPhysicalDeviceFeatures& supported) {
    VkPhysicalDeviceFeatures requested{};
#define COPY(name) requested.name=supported.name
    COPY(robustBufferAccess); COPY(fullDrawIndexUint32); COPY(imageCubeArray);
    COPY(independentBlend); COPY(depthClamp); COPY(depthBiasClamp);
    COPY(fillModeNonSolid); COPY(occlusionQueryPrecise);
    COPY(shaderStorageImageWriteWithoutFormat); COPY(multiViewport);
    COPY(shaderClipDistance); COPY(shaderCullDistance); COPY(geometryShader);
    COPY(sampleRateShading); COPY(textureCompressionBC); COPY(samplerAnisotropy);
    COPY(vertexPipelineStoresAndAtomics); COPY(pipelineStatisticsQuery); COPY(depthBounds);
#undef COPY
    return requested;
}
