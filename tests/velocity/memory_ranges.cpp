#include "guest_memory.h"
#include <vector>
#include <random>
#include <cstdio>
void require(bool ok){if(!ok)throw std::runtime_error("Sparse permissions differ from page oracle");}
int main(){
    GuestMemory memory;const uint32_t page=sysconf(_SC_PAGESIZE),base=0x100000;
    constexpr unsigned pages=256;
    memory.commit(base,page*pages);
    std::vector<uint32_t> flags(pages,4);
    constexpr uint32_t valid[]={1,2,4,8,0x10,0x20,0x40,0x80};
    std::mt19937 random(189250);
    for(unsigned trial=0;trial<3000;++trial){
        unsigned start=random()%pages,length=1+random()%(pages-start);uint32_t previous=0,flag=valid[random()%8];
        require(memory.protect(base+start*page,length*page,flag,previous)&&previous==flags[start]);
        for(unsigned i=start;i<start+length;++i)flags[i]=flag;
        const uint32_t offset=random()%(pages*page);const uint32_t bytes=1+random()%(pages*page-offset);
        bool read=true,write=true;
        for(unsigned i=offset/page;i<=(offset+bytes-1)/page;++i){
            read&=flags[i]!=1&&flags[i]!=0x10;
            write&=flags[i]==4||flags[i]==8||flags[i]==0x40||flags[i]==0x80;
        }
        require(memory.readable(base+offset,bytes)==read&&memory.writable(base+offset,bytes)==write);
        memory.commit(base+start*page,1); // Existing protection must survive neighboring heap commits.
        require(memory.writable(base+start*page,1)==(flag==4||flag==8||flag==0x40||flag==0x80));
    }
    require(!memory.readable(base-1,2)&&!memory.writable(base+pages*page-1,2));
    require(!memory.readable(0xffffffffu,2)&&memory.readable(base,0));
    GuestMemory sparse;sparse.commit(base,pages*page);
    require(sparse.writable(base,pages*page));uint32_t old=0;
    require(sparse.protect(base+101*page,page,2,old));
    require(!sparse.writable(base,pages*page)&&sparse.readable(base,pages*page));
    require(sparse.writable(base,100*page)&&sparse.writable(base+102*page,154*page));
    std::puts("Memory: 3000 randomized updates against per-page oracle, sparse overrides, boundaries and commit preservation passed");
}
