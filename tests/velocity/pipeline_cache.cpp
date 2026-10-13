#include "velocity_pipeline_cache.h"
#include <stdexcept>
#include <cstdio>
#include <unistd.h>
void require(bool ok){if(!ok)throw std::runtime_error("Persistent pipeline cache validation failed");}
int main(){
    velocity::PipelineIdentity key{0x5143,610,7,4198400,{1,2,3}};
    std::vector<uint8_t> bytes(64,0);
    bytes[0]=32;bytes[4]=1;bytes[8]=0x43;bytes[9]=0x51;bytes[12]=610&255;bytes[13]=610>>8;
    std::memcpy(bytes.data()+16,key.uuid,16);
    require(velocity::validPipelineCache(bytes.data(),bytes.size(),key));
#ifdef __ANDROID__
    std::string dir="/data/local/tmp/";
#else
    std::string dir="/tmp/";
#endif
    std::string path=dir+"velocity-cache-"+std::to_string(getpid());
    require(velocity::writePipelineCache(path,bytes,key));
    require(velocity::readPipelineCache(path,key)==bytes);
    auto changed=key;changed.device++;require(velocity::readPipelineCache(path,changed).empty());
    changed=key;changed.uuid[15]++;require(velocity::readPipelineCache(path,changed).empty());
    changed=key;changed.driver++;require(velocity::readPipelineCache(path,changed).empty());require(velocity::pipelineCacheName(key)!=velocity::pipelineCacheName(changed));
    changed=key;changed.api++;require(velocity::readPipelineCache(path,changed).empty());require(velocity::pipelineCacheName(key)!=velocity::pipelineCacheName(changed));
    {std::fstream corrupt(path,std::ios::binary|std::ios::in|std::ios::out);corrupt.seekp(60);corrupt.put('x');}
    require(velocity::readPipelineCache(path,key).empty());
    require(velocity::writePipelineCache(path,bytes,key));
    truncate(path.c_str(),55);require(velocity::readPipelineCache(path,key).empty());
    for(unsigned bad=0;bad<4;++bad){auto data=bytes;if(bad==0)data[0]=0;else data[bad==1?4:bad==2?8:16]++;require(!velocity::validPipelineCache(data.data(),data.size(),key));}
    require(!velocity::validPipelineCache(nullptr,64,key));
    require(!velocity::validPipelineCache(bytes.data(),31,key));
    require(!velocity::validPipelineCache(bytes.data(),velocity::maxPipelineCacheBytes+1,key));
    require(!velocity::writePipelineCache("/nonexistent/velocity/cache",bytes,key));
    std::remove(path.c_str());
    std::puts("Pipeline cache: roundtrip, opaque-data checksum, identity/driver/API isolation, malformed/truncated/null/oversized inputs and I/O fallback passed");
}
