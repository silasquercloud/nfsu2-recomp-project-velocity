// Vulkan driver report for the launcher (GpuProbeActivity, own process). Based on the MW port's
// android_diagnostico.cpp; the checks are what this port's DXVK d3d9 device enables unconditionally
// (D3D9DeviceEx::GetDeviceFeatures), so "compatible" means the game's device can be created.
#include "vulkan_driver.h"
#include "vulkan_compat.h"
#include <jni.h>
#include <dlfcn.h>
#include <sstream>
#include <string>
#include <vector>

namespace {
std::string json(const std::string& value) {
    std::string result = "\"";
    for (unsigned char c : value) {
        if (c == '\\' || c == '"') result += '\\';
        if (c >= 32) result += char(c);
    }
    return result + '"';
}

std::string probe(const char* hooks, const char* temp, const char* directory, const char* library) {
    struct Loader { void* handle; ~Loader() { if (handle) dlclose(handle); } }
        loader{nfsOpenVulkan(hooks, temp, directory, library)};
    if (!loader.handle) return "{\"driverLoadFailed\":true,\"probeError\":\"driver_open\"}";
    auto getProc = nfsVulkanEntry(loader.handle);
    if (!getProc) return "{\"driverLoadFailed\":true,\"probeError\":\"driver_entry\"}";
    auto createInstance = reinterpret_cast<PFN_vkCreateInstance>(getProc(nullptr, "vkCreateInstance"));
    if (!createInstance) return "{\"driverLoadFailed\":true,\"probeError\":\"driver_instance\"}";
    uint32_t loaderVersion = VK_API_VERSION_1_0;
    if (auto version = reinterpret_cast<PFN_vkEnumerateInstanceVersion>(getProc(nullptr, "vkEnumerateInstanceVersion")))
        version(&loaderVersion);
    VkApplicationInfo app{VK_STRUCTURE_TYPE_APPLICATION_INFO};
    app.pApplicationName = "NFSU2 driver probe";
    app.apiVersion = loaderVersion >= VK_API_VERSION_1_1 ? VK_API_VERSION_1_1 : VK_API_VERSION_1_0;
    VkInstanceCreateInfo instanceInfo{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
    instanceInfo.pApplicationInfo = &app;
    VkInstance instance{};
    VkResult result = createInstance(&instanceInfo, nullptr, &instance);
    if (result != VK_SUCCESS)
        return "{\"driverLoadFailed\":true,\"probeError\":\"vkCreateInstance: " + std::to_string(result) + "\"}";
#define LOAD(name) auto name = reinterpret_cast<PFN_##name>(getProc(instance, #name)); \
    if (!name) return "{\"driverLoadFailed\":true,\"probeError\":\"driver_functions\"}"
    LOAD(vkDestroyInstance);
    LOAD(vkEnumeratePhysicalDevices);
    LOAD(vkGetPhysicalDeviceProperties);
    LOAD(vkGetPhysicalDeviceMemoryProperties);
    LOAD(vkGetPhysicalDeviceFeatures);
    LOAD(vkGetPhysicalDeviceFormatProperties);
    LOAD(vkGetPhysicalDeviceQueueFamilyProperties);
    LOAD(vkEnumerateDeviceExtensionProperties);
    LOAD(vkCreateDevice);
#undef LOAD
    uint32_t count = 0;
    if (vkEnumeratePhysicalDevices(instance, &count, nullptr) != VK_SUCCESS || !count) {
        vkDestroyInstance(instance, nullptr);
        return "{\"driverLoadFailed\":true,\"probeError\":\"no_vulkan_gpu\"}";
    }
    std::vector<VkPhysicalDevice> devices(count);
    vkEnumeratePhysicalDevices(instance, &count, devices.data());
    VkPhysicalDevice gpu = devices[0];
    VkPhysicalDeviceProperties props{};
    VkPhysicalDeviceFeatures features{};
    vkGetPhysicalDeviceProperties(gpu, &props);
    vkGetPhysicalDeviceFeatures(gpu, &features);

    std::vector<std::string> extensions;
    uint32_t extensionCount = 0;
    if (vkEnumerateDeviceExtensionProperties(gpu, nullptr, &extensionCount, nullptr) == VK_SUCCESS && extensionCount) {
        std::vector<VkExtensionProperties> list(extensionCount);
        if (vkEnumerateDeviceExtensionProperties(gpu, nullptr, &extensionCount, list.data()) == VK_SUCCESS)
            for (auto& e : list) extensions.emplace_back(e.extensionName);
    }
    auto hasExtension = [&](const char* name) { for (auto& e : extensions) if (e == name) return true; return false; };

    std::vector<std::string> missing;
    auto require = [&](bool supported, const char* name) { if (!supported) missing.emplace_back(name); };
    require(props.apiVersion >= VK_API_VERSION_1_1, "Vulkan 1.1");
    require(hasExtension(VK_KHR_SWAPCHAIN_EXTENSION_NAME), "VK_KHR_swapchain");
    // Enabled unconditionally by the d3d9 device in this DXVK build (clip/cull distance,
    // geometry shaders and sample-rate shading are optional here: Mali/PowerVR lack them).
    require(features.robustBufferAccess, "robustBufferAccess");
    require(features.fullDrawIndexUint32, "fullDrawIndexUint32");
    require(features.imageCubeArray, "imageCubeArray");
    require(features.independentBlend, "independentBlend");
    require(features.depthClamp, "depthClamp");
    require(features.depthBiasClamp, "depthBiasClamp");
    require(features.fillModeNonSolid, "fillModeNonSolid");
    require(features.occlusionQueryPrecise, "occlusionQueryPrecise");
    require(features.shaderStorageImageWriteWithoutFormat, "shaderStorageImageWriteWithoutFormat");

    // DXT1/3/5 textures: on the GPU when BC is supported, otherwise decoded on the CPU (slower).
    bool bc = features.textureCompressionBC;
    for (VkFormat format : {VK_FORMAT_BC1_RGBA_UNORM_BLOCK, VK_FORMAT_BC2_UNORM_BLOCK, VK_FORMAT_BC3_UNORM_BLOCK}) {
        VkFormatProperties fp{};
        vkGetPhysicalDeviceFormatProperties(gpu, format, &fp);
        constexpr auto needed = VK_FORMAT_FEATURE_SAMPLED_IMAGE_BIT | VK_FORMAT_FEATURE_SAMPLED_IMAGE_FILTER_LINEAR_BIT;
        if ((fp.optimalTilingFeatures & needed) != needed) bc = false;
    }

    VkPhysicalDeviceDriverProperties driver{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DRIVER_PROPERTIES};
    if (props.apiVersion >= VK_API_VERSION_1_2 || hasExtension(VK_KHR_DRIVER_PROPERTIES_EXTENSION_NAME)) {
        if (auto query = reinterpret_cast<PFN_vkGetPhysicalDeviceProperties2>(getProc(instance, "vkGetPhysicalDeviceProperties2"))) {
            VkPhysicalDeviceProperties2 properties{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2};
            properties.pNext = &driver;
            query(gpu, &properties);
        }
    }

    const auto requested=nfsProbeDeviceFeatures(features);

    // Capabilities alone do not prove an imported driver can create a device.
    VkResult deviceResult = VK_ERROR_INITIALIZATION_FAILED;
    uint32_t familyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(gpu, &familyCount, nullptr);
    std::vector<VkQueueFamilyProperties> families(familyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(gpu, &familyCount, families.data());
    for (uint32_t family = 0; family < familyCount; ++family) {
        if (!(families[family].queueFlags & VK_QUEUE_GRAPHICS_BIT)) continue;
        float priority = 1.0f;
        VkDeviceQueueCreateInfo queue{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};
        queue.queueFamilyIndex = family; queue.queueCount = 1; queue.pQueuePriorities = &priority;
        const char* swapchain = VK_KHR_SWAPCHAIN_EXTENSION_NAME;
        VkDeviceCreateInfo info{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};
        info.pEnabledFeatures = &requested;
        info.queueCreateInfoCount = 1; info.pQueueCreateInfos = &queue;
        info.enabledExtensionCount = hasExtension(swapchain) ? 1 : 0; info.ppEnabledExtensionNames = &swapchain;
        VkDevice device{};
        deviceResult = vkCreateDevice(gpu, &info, nullptr, &device);
        if (deviceResult == VK_SUCCESS)
            if (auto destroy = reinterpret_cast<PFN_vkDestroyDevice>(getProc(instance, "vkDestroyDevice"))) destroy(device, nullptr);
        break;
    }
    if (deviceResult != VK_SUCCESS) missing.emplace_back("vkCreateDevice: " + std::to_string(deviceResult));

    std::ostringstream out;
    out << "{\"gpu\":" << json(props.deviceName)
        << ",\"vulkan\":" << json(std::to_string(VK_VERSION_MAJOR(props.apiVersion)) + "." +
               std::to_string(VK_VERSION_MINOR(props.apiVersion)) + "." + std::to_string(VK_VERSION_PATCH(props.apiVersion)))
        << ",\"vendorId\":" << props.vendorID
        << ",\"requestedDriver\":" << json(library && *library ? library : "system")
        << ",\"driverName\":" << json(driver.driverName)
        << ",\"driverInfo\":" << json(driver.driverInfo)
        << ",\"logicalDeviceResult\":" << int(deviceResult)
        << ",\"driverLoadFailed\":" << (deviceResult == VK_SUCCESS ? "false" : "true")
        << ",\"bcTextures\":" << (bc ? "true" : "false")
        << ",\"compatible\":" << (missing.empty() ? "true" : "false") << ",\"missing\":[";
    for (size_t i = 0; i < missing.size(); ++i) out << (i ? "," : "") << json(missing[i]);
    out << "],\"deviceId\":" << props.deviceID << ",\"driverVersion\":" << props.driverVersion;
    out << ",\"limits\":{\"maxImageDimension2D\":" << props.limits.maxImageDimension2D
        << ",\"maxBoundDescriptorSets\":" << props.limits.maxBoundDescriptorSets
        << ",\"maxPerStageDescriptorSamplers\":" << props.limits.maxPerStageDescriptorSamplers
        << ",\"maxUniformBufferRange\":" << props.limits.maxUniformBufferRange
        << ",\"maxMemoryAllocationCount\":" << props.limits.maxMemoryAllocationCount << "}";
    out << ",\"features\":{\"multiViewport\":" << (features.multiViewport?"true":"false")
        << ",\"clipDistance\":" << (features.shaderClipDistance?"true":"false")
        << ",\"geometryShader\":" << (features.geometryShader?"true":"false")
        << ",\"sampleRateShading\":" << (features.sampleRateShading?"true":"false") << "}";
    VkPhysicalDeviceMemoryProperties memory{};vkGetPhysicalDeviceMemoryProperties(gpu,&memory);
    uint64_t deviceHeap=0;for(uint32_t i=0;i<memory.memoryHeapCount;++i)if(memory.memoryHeaps[i].flags&VK_MEMORY_HEAP_DEVICE_LOCAL_BIT)deviceHeap+=memory.memoryHeaps[i].size;
    out << ",\"deviceLocalHeapBytes\":" << deviceHeap << ",\"timestampPeriodNs\":" << props.limits.timestampPeriod;
    out << ",\"timestampValidBits\":[";for(size_t i=0;i<families.size();++i)out << (i?",":"") << families[i].timestampValidBits;
    out << "],\"pipelineCapabilities\":{\"graphicsPipelineLibraryAdvertised\":" << (hasExtension("VK_EXT_graphics_pipeline_library")?"true":"false")
        << ",\"cacheControlAdvertised\":" << (hasExtension("VK_EXT_pipeline_creation_cache_control")?"true":"false")
        << ",\"pipelineBinaryAdvertised\":" << (hasExtension("VK_KHR_pipeline_binary")?"true":"false")
        << ",\"advancedBackendPathsImplemented\":false}";
    out << ",\"extensions\":[";
    for(size_t i=0;i<extensions.size();++i)out << (i?",":"") << json(extensions[i]);
    out << "],\"formats\":[";
    unsigned formatIndex=0;
    for(VkFormat format:{VK_FORMAT_BC1_RGBA_UNORM_BLOCK,VK_FORMAT_BC2_UNORM_BLOCK,VK_FORMAT_BC3_UNORM_BLOCK,VK_FORMAT_R8G8B8A8_UNORM,VK_FORMAT_D24_UNORM_S8_UINT,VK_FORMAT_D32_SFLOAT}){
        VkFormatProperties fp{};vkGetPhysicalDeviceFormatProperties(gpu,format,&fp);
        out << (formatIndex++?",":"") << "{\"format\":" << int(format) << ",\"optimalFeatures\":" << fp.optimalTilingFeatures << "}";
    }
    out << "]}";
    vkDestroyInstance(instance, nullptr);
    return out.str();
}
}  // namespace

extern "C" JNIEXPORT jstring JNICALL
Java_com_nfsu2_androidevolved_GpuProbeActivity_nativeGpuReport(JNIEnv* env, jclass, jstring hooks, jstring temp,
                                                               jstring directory, jstring library) {
    auto text = [&](jstring value) {
        const char* chars = value ? env->GetStringUTFChars(value, nullptr) : nullptr;
        std::string result = chars ? chars : "";
        if (chars) env->ReleaseStringUTFChars(value, chars);
        return result;
    };
    std::string h = text(hooks), t = text(temp), d = text(directory), l = text(library);
    return env->NewStringUTF(probe(h.c_str(), t.c_str(), d.c_str(), l.c_str()).c_str());
}
