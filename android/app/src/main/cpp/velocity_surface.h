#pragma once
#include <vulkan/vulkan.h>
namespace velocity {
// Keep logical landscape coordinates and let Android's compositor apply the
// display rotation. INHERIT is a supported compositor fallback, never a forced
// transform. A driver requiring application prerotation is reported explicitly.
inline VkSurfaceTransformFlagBitsKHR compositorTransform(VkSurfaceTransformFlagsKHR supported){
    if(supported&VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR)return VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
    if(supported&VK_SURFACE_TRANSFORM_INHERIT_BIT_KHR)return VK_SURFACE_TRANSFORM_INHERIT_BIT_KHR;
    return static_cast<VkSurfaceTransformFlagBitsKHR>(0);
}
}
