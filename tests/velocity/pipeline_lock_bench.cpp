// Synthetic contention experiment against the actual pinned DXVK spinlock.
// This measures waiting-thread CPU, not driver or game performance.
#include "src/util/sync/sync_spinlock.h"
#include <future>
#include <iostream>
#include <ctime>
uint64_t cpu(){timespec t{};clock_gettime(CLOCK_THREAD_CPUTIME_ID,&t);return uint64_t(t.tv_sec)*1000000000ull+t.tv_nsec;}
template<class Lock> double sample(){
 Lock lock;std::promise<void> acquired;
 std::thread compiler([&]{lock.lock();acquired.set_value();std::this_thread::sleep_for(std::chrono::milliseconds(50));lock.unlock();});
 acquired.get_future().wait();auto start=cpu();lock.lock();auto elapsed=cpu()-start;lock.unlock();compiler.join();return double(elapsed)/1e6;
}
int main(){for(int i=0;i<3;i++)std::cout<<"spin_wait_cpu_ms="<<sample<dxvk::sync::Spinlock>()<<", blocking_wait_cpu_ms="<<sample<std::mutex>()<<'\n';}
