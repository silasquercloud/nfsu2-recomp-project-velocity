"""Prepare the pinned zlib-licensed DXVK native 1.9.2b for Android ARM64."""
import argparse
import pathlib
import shutil
import subprocess

PIN = "c8dc91fabd00cac11d697ccf07426e798393cd40"
parser = argparse.ArgumentParser()
parser.add_argument("source", type=pathlib.Path)
parser.add_argument("output", type=pathlib.Path)
args = parser.parse_args()
revision = subprocess.check_output(["git", "-C", str(args.source), "rev-parse", "HEAD"], text=True).strip()
if revision != PIN:
    raise SystemExit("Unexpected DXVK revision: " + revision)
args.output.mkdir(parents=True, exist_ok=True)
for directory in ("src", "include"):
    shutil.copytree(args.source / directory, args.output / directory, dirs_exist_ok=True)
shutil.copy2(args.source / "LICENSE", args.output / "LICENSE")
for header in ("vulkan_android.h", "vulkan_xlib.h"):
    shutil.copy2(pathlib.Path(__file__).parent / "include" / header, args.output / "include/vulkan" / header)

def replace(path, old, new):
    file = args.output / path
    text = file.read_text()
    if old not in text:
        raise SystemExit("Patch mismatch: " + path)
    file.write_text(text.replace(old, new))

replace("src/util/util_bit.h", "#pragma once", "#pragma once\n#include <cstdint>")
replace("src/d3d9/d3d9_device.cpp", "enabled.core.features.textureCompressionBC = VK_TRUE;", "enabled.core.features.textureCompressionBC = supported.core.features.textureCompressionBC;")

replace("src/util/util_bit.h", "#ifndef _MSC_VER", "#if defined(__i386__) || defined(__x86_64__)\n#ifndef _MSC_VER")
replace("src/util/util_bit.h", '#include "util_likely.h"', '#endif // x86 intrinsics\n#include "util_likely.h"')
replace("src/util/util_bit.h", "#elif defined(__GNUC__) || defined(__clang__)\n    uint32_t res;", "#elif defined(__aarch64__) || defined(__arm__)\n    return n ? __builtin_ctz(n) : 32;\n    #elif defined(__GNUC__) || defined(__clang__)\n    uint32_t res;")
replace("src/util/util_bit.h", "#if defined(__GNUC__) || defined(__clang__) || defined(_MSC_VER)", "#if defined(__i386__) || defined(__x86_64__)")
replace("src/util/sync/sync_spinlock.h", "_mm_pause();", '#if defined(__aarch64__) || defined(__arm__)\n        asm volatile("yield");\n#else\n        _mm_pause();\n#endif')
replace("src/util/util_vector.h", "    __m128 value = _mm_load_ps(a.data);", "#if defined(__aarch64__) || defined(__arm__)\n    for (unsigned i = 0; i < 4; ++i) result.data[i] = a.data[i] == a.data[i] ? a.data[i] : 0.0f;\n#else\n    __m128 value = _mm_load_ps(a.data);")
replace("src/util/util_vector.h", "    _mm_store_ps(result.data, value);", "    _mm_store_ps(result.data, value);\n#endif")
# Android reports VK_SUBOPTIMAL_KHR for every landscape present (the image is not
# pre-rotated; the compositor rotates it). Treating that as a failed present recreated
# the swap chain, with a vkDeviceWaitIdle, on every frame.
replace("src/d3d9/d3d9_swapchain.cpp", '    VkResult status = m_device->waitForSubmission(&m_presentStatus);\n\n    if (status != VK_SUCCESS)\n      RecreateSwapChain(m_vsync);', '    VkResult status = m_device->waitForSubmission(&m_presentStatus);\n\n    if (status != VK_SUCCESS && status != VK_SUBOPTIMAL_KHR)\n      RecreateSwapChain(m_vsync);')
# DXVK resolves Vulkan through the driver selected at runtime, not the linked loader.
replace("src/vulkan/vulkan_loader.cpp", '  static const PFN_vkGetInstanceProcAddr GetInstanceProcAddr = vkGetInstanceProcAddr;', '  // Android: the driver chosen in the launcher (system or adrenotools), see android_wsi.cpp.\n  extern "C" PFN_vkGetInstanceProcAddr dxvkAndroidGetInstanceProcAddr();\n  static PFN_vkVoidFunction GetInstanceProcAddr(VkInstance instance, const char* name) {\n    return dxvkAndroidGetInstanceProcAddr()(instance, name);\n  }')
# D3D9 sets a single viewport; Adreno 610 (Redmi Note 8) has no multiViewport.
replace("src/d3d9/d3d9_device.cpp", '    enabled.core.features.multiViewport = VK_TRUE;', '    enabled.core.features.multiViewport = supported.core.features.multiViewport;')
# Old Adreno 6xx drivers crash resetting command buffers whose descriptor pools were
# already recycled: reset them first in reset(), not later in beginRecording().
replace("src/dxvk/dxvk_cmdlist.cpp", '    if ((m_graphicsPool && m_vkd->vkResetCommandPool(m_vkd->device(), m_graphicsPool, 0) != VK_SUCCESS)\n     || (m_transferPool && m_vkd->vkResetCommandPool(m_vkd->device(), m_transferPool, 0) != VK_SUCCESS))\n      Logger::err("DxvkCommandList: Failed to reset command buffer");\n    ', '    // Command pools were reset in reset(), once the GPU finished with them.\n    ')
replace("src/dxvk/dxvk_cmdlist.cpp", '  void DxvkCommandList::reset() {\n    // Signal resources and events to\n    // avoid stalling main thread\n    m_signalTracker.reset();', '  void DxvkCommandList::reset() {\n    // Reset the command buffers while everything they reference still exists.\n    // Recycling descriptor pools and releasing resources first is legal, but\n    // the 2020 Adreno 6xx driver (Redmi Note 8, Adreno 610) then follows freed\n    // descriptor state while resetting and calls through a null pointer.\n    if ((m_graphicsPool && m_vkd->vkResetCommandPool(m_vkd->device(), m_graphicsPool, 0) != VK_SUCCESS)\n     || (m_transferPool && m_vkd->vkResetCommandPool(m_vkd->device(), m_transferPool, 0) != VK_SUCCESS))\n      Logger::err("DxvkCommandList: Failed to reset command buffer");\n\n    // Signal resources and events to\n    // avoid stalling main thread\n    m_signalTracker.reset();')
# Mali: no clip/cull distances (its compiler crashes on them) and no geometry shaders;
# PowerVR: no sample-rate shading. Enable those only where supported.
replace('src/d3d9/d3d9_device.cpp', '    enabled.core.features.shaderClipDistance = VK_TRUE;\n    enabled.core.features.shaderCullDistance = VK_TRUE;\n', '    // Mali has no clip/cull distances and its compiler crashes (null pointer in\n    // cmpbe_v2_compile_multiple_shaders) on shaders that write them, even though\n    // vkCreateDevice accepts the feature. Without it, shaders skip user clip planes.\n    enabled.core.features.shaderClipDistance = supported.core.features.shaderClipDistance;\n    enabled.core.features.shaderCullDistance = supported.core.features.shaderCullDistance;\n    g_d3d9ClipDistance = supported.core.features.shaderClipDistance;\n')
replace('src/d3d9/d3d9_device.cpp', '    enabled.core.features.geometryShader = VK_TRUE;\n', '    enabled.core.features.geometryShader = supported.core.features.geometryShader;\n')
replace('src/d3d9/d3d9_device.cpp', '    enabled.core.features.sampleRateShading = VK_TRUE;\n', '    enabled.core.features.sampleRateShading = supported.core.features.sampleRateShading;\n')
replace('src/d3d9/d3d9_device.cpp', 'namespace dxvk {\n', 'namespace dxvk {\n\n  // Whether vertex shaders may write gl_ClipDistance (set per device creation).\n  bool g_d3d9ClipDistance = true;\n')
replace('src/dxso/dxso_compiler.cpp', '  void DxsoCompiler::emitVsInit() {\n    m_module.enableCapability(spv::CapabilityClipDistance);\n', '  void DxsoCompiler::emitVsInit() {\n    if (g_d3d9ClipDistance)\n      m_module.enableCapability(spv::CapabilityClipDistance);\n')
replace('src/dxso/dxso_compiler.cpp', '    this->emitLinkerOutputSetup();\n\n    this->emitVsClipping();\n', '    this->emitLinkerOutputSetup();\n\n    if (g_d3d9ClipDistance)\n      this->emitVsClipping();\n')
replace('src/dxso/dxso_compiler.cpp', 'namespace dxvk {\n', 'namespace dxvk {\n\n  extern bool g_d3d9ClipDistance;\n')
replace('src/d3d9/d3d9_fixed_function.cpp', '    // VS Caps\n    m_module.enableCapability(spv::CapabilityClipDistance);\n', '    // VS Caps\n    if (g_d3d9ClipDistance)\n      m_module.enableCapability(spv::CapabilityClipDistance);\n')
replace('src/d3d9/d3d9_fixed_function.cpp', '    if (m_vsKey.Data.Contents.VertexClipping)\n      emitVsClipping(vtx);\n', '    if (m_vsKey.Data.Contents.VertexClipping && g_d3d9ClipDistance)\n      emitVsClipping(vtx);\n')
replace('src/d3d9/d3d9_fixed_function.cpp', 'namespace dxvk {\n', 'namespace dxvk {\n\n  extern bool g_d3d9ClipDistance;\n')
# DXVK warnings and errors reach logcat (tag DXVK) for tester reports.
replace('src/util/log/log.cpp', '#include "log.h"\n', '#include "log.h"\n#ifdef __ANDROID__\n#include <android/log.h>\n#endif\n')
replace('src/util/log/log.cpp', '        std::cerr << prefix << line << std::endl;\n', '        std::cerr << prefix << line << std::endl;\n#ifdef __ANDROID__\n        // stderr goes nowhere on Android: send warnings and errors to logcat,\n        // so tester reports (logcat.txt) show why DXVK failed.\n        if (level >= LogLevel::Warn)\n          __android_log_print(level >= LogLevel::Error ? ANDROID_LOG_ERROR : ANDROID_LOG_WARN, "DXVK", "%s", line.c_str());\n#endif\n')
# Project Velocity: optional host-side timing; never remove/shorten GPU waits.
for path, signature, metric in (
    ("src/dxvk/dxvk_cmdlist.cpp", "  VkResult DxvkCommandList::synchronize() {", "FenceWait"),
    ("src/vulkan/vulkan_presenter.cpp", "  VkResult Presenter::presentImage() {", "VulkanPresent"),
):
    replace(path, signature, signature + '\n#if NFS_VELOCITY_PROFILING\n    velocity::Scope profile(velocity::Metric::' + metric + ');\n#endif')
for path in ("src/dxvk/dxvk_cmdlist.cpp", "src/vulkan/vulkan_presenter.cpp",
             "src/dxvk/dxvk_graphics.cpp", "src/dxvk/dxvk_compute.cpp"):
    file = args.output / path
    file.write_text('#if NFS_VELOCITY_PROFILING\n#include "velocity_profile.h"\n#endif\n' + file.read_text())
replace("src/dxvk/dxvk_cmdlist.cpp", "    return m_vkd->vkQueueSubmit(queue, 1, &submitInfo, fence);",
    "#if NFS_VELOCITY_PROFILING\n    velocity::Scope profile(velocity::Metric::VulkanSubmit);\n#endif\n    return m_vkd->vkQueueSubmit(queue, 1, &submitInfo, fence);")
replace("src/dxvk/dxvk_graphics.cpp", "    const DxvkRenderPass*                renderPass) const {", "    const DxvkRenderPass*                renderPass) const {\n#if NFS_VELOCITY_PROFILING\n    velocity::Scope profile(velocity::Metric::PipelineCompile);\n#endif")
replace("src/dxvk/dxvk_compute.cpp", "  VkPipeline DxvkComputePipeline::createPipeline(\n    const DxvkComputePipelineStateInfo& state) const {", "  VkPipeline DxvkComputePipeline::createPipeline(\n    const DxvkComputePipelineStateInfo& state) const {\n#if NFS_VELOCITY_PROFILING\n    velocity::Scope profile(velocity::Metric::PipelineCompile);\n#endif")
replace("src/dxvk/dxvk_cmdlist.h", "#pragma once", '#pragma once\n#if NFS_VELOCITY_PROFILING\n#include "velocity_profile.h"\n#endif')
replace("src/dxvk/dxvk_cmdlist.h", "      m_vkd->vkCmdBindPipeline(m_execBuffer,", "#if NFS_VELOCITY_PROFILING\n      if(velocity::enabled())velocity::record(velocity::Metric::PipelineBind,0);\n#endif\n      m_vkd->vkCmdBindPipeline(m_execBuffer,")
(args.output / "version.h").write_text('#define DXVK_VERSION "native-1.9.2b-android-arm64"\n')
(args.output / "PINNED_REVISION").write_text(PIN + "\n")
print("Prepared DXVK " + PIN)
