#pragma once
#include <algorithm>
#include <cstdint>
#include <memory>
#include <chrono>
#include <atomic>
#include <condition_variable>
#include <deque>
#include <functional>
#include <future>
#include <mutex>
#include <thread>
#include <vector>
#include <string>
#include <cstdlib>
namespace velocity {
// Fatal required-pipeline failure is reported by the bridge, not hidden in the CS thread.
inline std::atomic<bool> renderingFailure{false};
// A bounded shared worker budget. Required first-use jobs outrank prewarming.
// A caller must retain referenced objects until its future completes.
class AsyncCompiler {
    std::mutex mutex_;
    std::condition_variable ready_;
    std::deque<std::function<void()>> required_,warm_;
    std::vector<std::thread> workers_;
    bool stopping_=false;
    std::atomic<unsigned> busy_{0};
    void worker(){
        for(;;){std::function<void()> task;
            {std::unique_lock<std::mutex> lock(mutex_);ready_.wait(lock,[&]{return stopping_||!required_.empty()||!warm_.empty();});
             if(required_.empty()&&warm_.empty()){if(stopping_)return;continue;}
             auto& queue=required_.empty()?warm_:required_;task=std::move(queue.front());queue.pop_front();++busy_;}
            task();--busy_;
        }
    }
public:
    explicit AsyncCompiler(unsigned count){
        try{for(unsigned i=0;i<std::min(4u,count);++i)workers_.emplace_back([this]{worker();});}
        catch(...){shutdown();throw;}
    }
    ~AsyncCompiler(){shutdown();}
    AsyncCompiler(const AsyncCompiler&)=delete;
    AsyncCompiler& operator=(const AsyncCompiler&)=delete;
    std::shared_future<void> submit(std::function<void()> task,bool required,
        std::shared_ptr<std::atomic<unsigned>> state={}){
        auto job=std::make_shared<std::packaged_task<void()>>([task=std::move(task),state]{
            if(state){unsigned pending=0;if(!state->compare_exchange_strong(pending,1))return;}
            task();
        });auto future=job->get_future().share();
        {std::lock_guard<std::mutex> lock(mutex_);
         if(stopping_||workers_.empty()||required_.size()+warm_.size()>=1024)return {};
         (required?required_:warm_).emplace_back([job]{(*job)();});}
        ready_.notify_one();return future;
    }
    void runRequired(std::function<void()> task,std::function<void()> timeout={},
                     std::chrono::milliseconds deadline=std::chrono::milliseconds(2000)){
        auto state=std::make_shared<std::atomic<unsigned>>(0);
        auto f=submit(task,true,state);
        if(!f.valid()){task();return;}
        if(f.wait_for(deadline)!=std::future_status::ready){
            if(timeout)timeout();
            unsigned pending=0;
            // Only a job that has not started can be cancelled safely. The
            // queued wrapper will skip it without dereferencing captured objects.
            if(state->compare_exchange_strong(pending,2)){task();return;}
        }
        // A running Vulkan compiler cannot safely be cancelled or duplicated.
        f.get();
    }
    bool busy(){std::lock_guard<std::mutex> lock(mutex_);return busy_||!required_.empty()||!warm_.empty();}
    void shutdown(){
        {std::lock_guard<std::mutex> lock(mutex_);stopping_=true;}
        ready_.notify_all();for(auto& worker:workers_)if(worker.joinable())worker.join();workers_.clear();
    }
};
inline bool shaderAsyncEnabled(const std::string& mode,unsigned cores,uint64_t ram){
    if(mode=="off")return false;
    if(mode=="on")return true;
    return cores>=4&&(!ram||ram>=(uint64_t(2)<<30));
}
inline unsigned shaderWorkers(unsigned cores,unsigned requested,bool conservative,unsigned limit=4){
    cores=std::max(1u,cores);unsigned available=cores<=4?1:cores<=8?2:3;
    if(conservative)available=1;
    return std::max(1u,std::min({requested?requested:available,limit,4u,std::max(1u,cores-1)}));
}
}
