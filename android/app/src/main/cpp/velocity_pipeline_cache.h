#pragma once
#include <cstdint>
#include <cstring>
#include <vector>
#include <string>
#include <fstream>
#include <cstdio>
namespace velocity {
constexpr size_t maxPipelineCacheBytes=32u*1024u*1024u;
struct PipelineIdentity {uint32_t vendor,device,driver,api;uint8_t uuid[16];};
inline uint32_t cacheWord(const uint8_t* p){return uint32_t(p[0])|(uint32_t(p[1])<<8)|(uint32_t(p[2])<<16)|(uint32_t(p[3])<<24);}
inline bool validPipelineCache(const uint8_t* p,size_t n,const PipelineIdentity& key){
    if(!p||n<32||n>maxPipelineCacheBytes)return false;
    const auto header=cacheWord(p);
    return header>=32&&header<=n&&cacheWord(p+4)==1&&cacheWord(p+8)==key.vendor&&
        cacheWord(p+12)==key.device&&!std::memcmp(p+16,key.uuid,16);
}
inline std::string pipelineCacheName(const PipelineIdentity& key){
    char prefix[96];std::snprintf(prefix,sizeof prefix,"velocity-vk1-%08x-%08x-%08x-%08x-",key.vendor,key.device,key.driver,key.api);
    std::string name=prefix;
    constexpr char digits[]="0123456789abcdef";
    for(uint8_t b:key.uuid){name+=digits[b>>4];name+=digits[b&15];}
    return name+".cache";
}
inline uint64_t cacheChecksum(const std::vector<uint8_t>& bytes){
    uint64_t hash=14695981039346656037ull;
    for(uint8_t b:bytes){hash^=b;hash*=1099511628211ull;}return hash;
}
inline uint64_t cacheLong(const uint8_t* p){uint64_t v=0;for(unsigned i=0;i<8;i++)v|=uint64_t(p[i])<<(8*i);return v;}
inline std::vector<uint8_t> readPipelineCache(const std::string& path,const PipelineIdentity& key){
    std::ifstream file(path,std::ios::binary|std::ios::ate);if(!file)return {};
    const auto size=file.tellg();if(size<64||size>std::streamoff(maxPipelineCacheBytes+32))return {};
    file.seekg(0);uint8_t wrapper[32]{};
    if(!file.read(reinterpret_cast<char*>(wrapper),32)||std::memcmp(wrapper,"VPVC0002",8))return {};
    const auto bytesCount=cacheLong(wrapper+8);
    if(cacheWord(wrapper+24)!=key.driver||cacheWord(wrapper+28)!=key.api)return {};
    if(bytesCount<32||bytesCount>maxPipelineCacheBytes||bytesCount+32!=uint64_t(size))return {};
    std::vector<uint8_t> bytes(static_cast<size_t>(bytesCount));
    if(!file.read(reinterpret_cast<char*>(bytes.data()),bytes.size())||cacheLong(wrapper+16)!=cacheChecksum(bytes)||!validPipelineCache(bytes.data(),bytes.size(),key))return {};
    return bytes;
}
inline bool writePipelineCache(const std::string& path,const std::vector<uint8_t>& bytes,const PipelineIdentity& key){
    if(path.empty()||!validPipelineCache(bytes.data(),bytes.size(),key))return false;
    const auto temporary=path+".tmp";
    FILE* file=std::fopen(temporary.c_str(),"wb");if(!file)return false;
    uint8_t wrapper[32]{};std::memcpy(wrapper,"VPVC0002",8);
    const uint64_t length=bytes.size(),checksum=cacheChecksum(bytes);
    for(unsigned i=0;i<8;++i){wrapper[8+i]=length>>(8*i);wrapper[16+i]=checksum>>(8*i);}
    for(unsigned i=0;i<4;++i){wrapper[24+i]=key.driver>>(8*i);wrapper[28+i]=key.api>>(8*i);}
    bool ok=std::fwrite(wrapper,1,32,file)==32;
    if(std::fwrite(bytes.data(),1,bytes.size(),file)!=bytes.size())ok=false;
    if(std::fclose(file))ok=false;
    if(ok&&std::rename(temporary.c_str(),path.c_str())==0)return true;
    std::remove(temporary.c_str());return false;
}
}
