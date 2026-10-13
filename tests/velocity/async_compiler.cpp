#include "velocity_async.h"
#include <stdexcept>
#include <iostream>
void check(bool ok){if(!ok)throw std::runtime_error("Async compiler contract");}
int main(){
 check(velocity::shaderAsyncEnabled("auto",8,uint64_t(4)<<30));
 check(!velocity::shaderAsyncEnabled("auto",2,uint64_t(4)<<30));
 check(!velocity::shaderAsyncEnabled("auto",8,uint64_t(1)<<30));
 check(velocity::shaderAsyncEnabled("on",2,uint64_t(1)<<30));
 check(!velocity::shaderAsyncEnabled("off",16,uint64_t(8)<<30));
 check(velocity::shaderWorkers(8,0,false)==2);
 check(velocity::shaderWorkers(1,4,false)==1);
 check(velocity::shaderWorkers(16,4,true)==4); // explicit request remains bounded
 check(velocity::shaderWorkers(16,0,true)==1);
 velocity::AsyncCompiler pool(1);std::promise<void> entered,release;auto gate=release.get_future().share();
 auto first=pool.submit([&]{entered.set_value();gate.wait();},false);entered.get_future().wait();
 std::vector<int> order;
 auto warm=pool.submit([&]{order.push_back(1);},false);
 auto needed=pool.submit([&]{order.push_back(2);},true);
 release.set_value();first.get();needed.get();warm.get();check(order==std::vector<int>({2,1}));
 bool error=false;try{pool.runRequired([]{throw std::runtime_error("driver failure");});}catch(const std::runtime_error&){error=true;}
 check(error);int complete=0;pool.runRequired([&]{++complete;});check(complete==1);
 pool.shutdown();pool.runRequired([&]{++complete;});check(complete==2);
 velocity::AsyncCompiler timeoutPool(1);std::promise<void> occupied,unblock;
 auto block=unblock.get_future().share();auto pending=timeoutPool.submit([&]{occupied.set_value();block.wait();},false);occupied.get_future().wait();
 int ran=0,timeouts=0;timeoutPool.runRequired([&]{++ran;},[&]{++timeouts;},std::chrono::milliseconds(1));
 check(ran==1&&timeouts==1);unblock.set_value();timeoutPool.shutdown();pending.get();check(ran==1);
 velocity::AsyncCompiler disabled(0);disabled.runRequired([&]{++complete;});check(complete==3);
 velocity::AsyncCompiler bounded(1);std::promise<void> release2,entered2;auto gate2=release2.get_future().share();
 auto blocker=bounded.submit([&]{entered2.set_value();gate2.wait();},false);entered2.get_future().wait();
 for(unsigned i=0;i<1024;i++)check(bounded.submit([]{},false).valid());
 check(!bounded.submit([]{},true).valid());release2.set_value();bounded.shutdown();blocker.get();
 std::cout<<"Shared workers: priority, readiness, exceptions, bounded queue, fallback and draining shutdown passed\n";
}
