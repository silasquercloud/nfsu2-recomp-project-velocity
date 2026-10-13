"""Additional verified patches against the pinned .3 prepared backend."""
def apply(root):
    def replace(path,old,new):
        p=root/path;s=p.read_text()
        if old not in s:raise SystemExit('Velocity .4 patch mismatch: '+path)
        p.write_text(s.replace(old,new))
    # Driver compilation is a long critical section: block instead of spinning.
    for kind in ('graphics','compute'):
        replace(f'src/dxvk/dxvk_{kind}.h','sync::Spinlock','std::mutex')
        replace(f'src/dxvk/dxvk_{kind}.cpp','std::lock_guard<sync::Spinlock>','std::lock_guard<std::mutex>')
    replace('src/dxvk/dxvk_pipemanager.h','#pragma once','#pragma once\n#include "velocity_async.h"\n#include "velocity_profile.h"')
    replace('src/dxvk/dxvk_pipemanager.h','    const DxvkDevice*         m_device;','    const DxvkDevice*         m_device;\n    std::unique_ptr<velocity::AsyncCompiler> m_compiler;')
    replace('src/dxvk/dxvk_pipemanager.h','    ~DxvkPipelineManager();','''    ~DxvkPipelineManager();
    std::shared_future<void> prewarm(std::function<void()> job) {
      return m_compiler ? m_compiler->submit(std::move(job), false) : std::shared_future<void>{};
    }''')
    replace('src/dxvk/dxvk_pipemanager.cpp','    std::string useStateCache = env::getEnvVar("DXVK_STATE_CACHE");','''    const auto mode = env::getEnvVar("NFS_VELOCITY_ASYNC");
    const auto profile = env::getEnvVar("NFS_VELOCITY_GRAPHICS_PROFILE");
    const long pages = sysconf(_SC_PHYS_PAGES), pageSize = sysconf(_SC_PAGESIZE);
    const uint64_t ram = pages > 0 && pageSize > 0 ? uint64_t(pages) * uint64_t(pageSize) : 0;
    const unsigned limit = ram && ram <= (uint64_t(3) << 30) ? 1u : 4u;
    unsigned requested = std::max(0, std::atoi(env::getEnvVar("NFS_VELOCITY_WORKERS").c_str()));
    if (!requested && profile == "experimental") requested = 4;
    if (!requested && profile == "performance") requested = 3;
    const unsigned cores = dxvk::thread::hardware_concurrency();
    const unsigned workers = velocity::shaderWorkers(cores, requested,
      profile == "compatibility", limit);
    if (velocity::shaderAsyncEnabled(mode, cores, ram)) {
      try { m_compiler = std::make_unique<velocity::AsyncCompiler>(workers); }
      catch (const std::exception& e) { Logger::warn(str::format("Velocity: compiler startup failed; synchronous fallback: ", e.what())); }
    }
    velocity::metadata(str::format("async_workers=", m_compiler ? workers : 0, " profile=", profile,
      " policy=required-first; cold first-use waits for correctness; timeout cannot cancel driver"));
    std::string useStateCache = env::getEnvVar("DXVK_STATE_CACHE");''')
    replace('src/dxvk/dxvk_pipemanager.cpp','    if (useStateCache != "0" && device->config().enableStateCache)','    if (useStateCache != "0" && device->config().enableStateCache)')
    replace('src/dxvk/dxvk_pipemanager.cpp','  DxvkPipelineManager::~DxvkPipelineManager() {\n    \n  }','''  DxvkPipelineManager::~DxvkPipelineManager() {
    if (m_stateCache != nullptr) m_stateCache->stopWorkerThreads();
    m_compiler.reset();
  }''')
    replace('src/dxvk/dxvk_pipemanager.cpp','    return m_stateCache != nullptr\n        && m_stateCache->isCompilingShaders();','    return m_compiler && m_compiler->busy();')
    # Required pipelines are compiled off the calling thread, but drawing waits
    # for readiness. No draw skipping or partially initialized pipeline handles.
    for kind, find, args, cache in (
      ('graphics','this->findInstance(state, renderPass)','state, renderPass','state, renderPass->format()'),
      ('compute','this->findInstance(state)','state','state')):
        p=root/f'src/dxvk/dxvk_{kind}.cpp';s=p.read_text()
        start=s.index(f'  VkPipeline Dxvk{kind.title()}Pipeline::getPipelineHandle(')
        body=s.index(') {',start)+3
        end=s.index(f'  void Dxvk{kind.title()}Pipeline::compilePipeline(',body)
        code=f'''
    {{ std::unique_lock<std::mutex> lock(m_mutex, std::defer_lock);
      if (!lock.try_lock()) {{ velocity::Scope wait(velocity::Metric::PipelineWait); lock.lock(); }}
      auto ready = {find};
      if (ready) {{ velocity::count(velocity::Counter::PipelineHits); return ready->pipeline(); }}
    }}
    velocity::count(velocity::Counter::PipelineMisses);
    try {{ velocity::Scope wait(velocity::Metric::PipelineWait);
      if (m_pipeMgr->m_compiler) {{
        try {{ m_pipeMgr->m_compiler->runRequired([this, {args}] {{ this->compilePipeline({args}); }},
          [] {{ velocity::count(velocity::Counter::CompileTimeouts); }}); }}
        catch (const DxvkError&) {{
          Logger::warn("Velocity: asynchronous compilation failed; retrying synchronously");
          this->compilePipeline({args});
        }}
      }} else this->compilePipeline({args});
    }} catch (const DxvkError& e) {{
      velocity::renderingFailure.store(true);
      Logger::err(str::format("Velocity: required pipeline failed: ", e.message()));
      return VK_NULL_HANDLE;
    }}
    VkPipeline result = VK_NULL_HANDLE;
    {{ std::lock_guard<std::mutex> lock(m_mutex);
      auto ready = {find};
      if (ready) result = ready->pipeline();
    }}
    if (result) this->writePipelineStateToCache({cache});
    return result;
  }}


'''
        s=s[:body]+code+s[end:];p.write_text(s)
    # Failed driver compilation must not poison the instance cache with NULL.
    for kind, call in (('graphics','state, renderPass'),('compute','state')):
        replace(f'src/dxvk/dxvk_{kind}.cpp',
          f'    VkPipeline newPipelineHandle = this->createPipeline({call});',
          f'    VkPipeline newPipelineHandle = this->createPipeline({call});\n    if (!newPipelineHandle) throw DxvkError("Velocity: driver pipeline compilation failed");')
    # SPIR-V translation uses the same bounded pool. The caller retains the DXSO
    # module and constant layout until readiness; shader registration happens
    # afterwards, avoiding nested submissions from a compiler worker.
    replace('src/dxvk/dxvk_pipemanager.h','    ~DxvkPipelineManager();','''    ~DxvkPipelineManager();
    void compileShader(std::function<void()> job) {
      if (m_compiler) m_compiler->runRequired(std::move(job), [] { velocity::count(velocity::Counter::CompileTimeouts); }); else job();
    }''')
    replace('src/dxvk/dxvk_device.h','    void waitForIdle();','''    void waitForIdle();
    void compileShaderJob(std::function<void()> job) { m_objects.pipelineManager().compileShader(std::move(job)); }
''')
    replace('src/d3d9/d3d9_shader.cpp',
      '    m_shaders      = pModule->compile(*pDxsoModuleInfo, name, AnalysisInfo, constantLayout);',
      '''    pDevice->GetDXVKDevice()->compileShaderJob([&] {
      m_shaders = pModule->compile(*pDxsoModuleInfo, name, AnalysisInfo, constantLayout);
    });''')
    # State cache prewarming shares that exact worker budget, no second pool.
    replace('src/dxvk/dxvk_state_cache.h','#include <queue>','#include <queue>\n#include <future>\n#include <unordered_set>')
    replace('src/dxvk/dxvk_state_cache.h','    std::queue<WorkerItem>            m_workerQueue;','''    std::queue<WorkerItem>            m_workerQueue;
    std::vector<std::shared_future<void>> m_pending;
    std::unordered_set<DxvkStateCacheKey, DxvkHash, DxvkEq> m_scheduled;''')
    p=root/'src/dxvk/dxvk_state_cache.cpp';s=p.read_text()
    a=s.index('    // Use half the available CPU cores');b=s.index('    m_writerThread = ',a)
    s=s[:a]+'    m_workerBusy.store(0);\n\n'+s[b:];p.write_text(s)
    replace('src/dxvk/dxvk_state_cache.cpp','    m_shaderMap.insert({ key, shader });','''    if (!m_shaderMap.insert({ key, shader }).second) return;
    if (env::getEnvVar("NFS_VELOCITY_PREWARM") == "off" || m_stopThreads.load()) return;''')
    replace('src/dxvk/dxvk_state_cache.cpp','      m_workerQueue.push(item);','''      if (!m_scheduled.insert(p->second).second) continue;
      // Retain futures for lifetime-safe shutdown, pruning completed jobs.
      m_pending.erase(std::remove_if(m_pending.begin(), m_pending.end(), [](const auto& f) {
        return f.wait_for(std::chrono::seconds(0)) == std::future_status::ready;
      }), m_pending.end());
      if (m_pending.size() >= 1024) continue;
      auto future = m_pipeManager->prewarm([this, item] {
        if (m_stopThreads.load()) return;
        ++m_workerBusy;
        try { compilePipelines(item); }
        catch (const DxvkError& e) {
          velocity::count(velocity::Counter::PrewarmFailures);
          static std::atomic<unsigned> reports{0};
          if (reports.fetch_add(1) < 8)
            Logger::warn(str::format("Velocity: prewarm failed; required draw retries compilation: ", e.message()));
        }
        catch (const std::exception& e) {
          velocity::count(velocity::Counter::PrewarmFailures);
          static std::atomic<unsigned> reports{0};
          if (reports.fetch_add(1) < 8)
            Logger::warn(str::format("Velocity: prewarm failed: ", e.what()));
        }
        --m_workerBusy;
      });
      if (future.valid()) m_pending.push_back(std::move(future));''')
    replace('src/dxvk/dxvk_state_cache.cpp','    for (auto& worker : m_workerThreads)\n      worker.join();','''    std::vector<std::shared_future<void>> pending;
    { std::lock_guard<dxvk::mutex> lock(m_workerLock); pending.swap(m_pending); }
    for (auto& f : pending) f.wait();''')
    # Always include no-op-safe counter API even with profiling compiled out.
    for kind in ('state_cache','pipemanager','graphics','compute'):
        p=root/f'src/dxvk/dxvk_{kind}.cpp';p.write_text('#include "velocity_profile.h"\n#include <unistd.h>\n'+p.read_text())
