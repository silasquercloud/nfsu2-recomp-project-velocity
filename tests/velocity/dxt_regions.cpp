#include "dxt_decode.h"
#include <random>
#include <cstdio>
void require(bool ok){if(!ok)throw std::runtime_error("DXT region pixels differ from full mip");}
int main(){
    std::mt19937 random(278276);unsigned rectangles=0;
    for(unsigned mode:{1,3,5})for(uint32_t width:{1,4,5,16,31})for(uint32_t height:{1,4,7,16,29}){
        std::vector<uint8_t> blocks(size_t((width+3)/4)*((height+3)/4)*(mode==1?8:16));
        for(auto& byte:blocks)byte=random();
        auto full=decodeDXT(mode,width,height,blocks.data(),blocks.size());
        for(uint32_t top=0;top<height;top+=4)for(uint32_t left=0;left<width;left+=4){
            uint32_t rw=std::min(4u,width-left),rh=std::min(4u,height-top);
            auto region=decodeDXTRegion(mode,width,height,blocks.data(),blocks.size(),left,top,rw,rh);
            require(region.size()==size_t(rw)*rh*4);
            for(uint32_t y=0;y<rh;++y)require(!std::memcmp(region.data()+y*rw*4,full.data()+((top+y)*width+left)*4,rw*4));
            ++rectangles;
        }
        bool rejected=false;
        try{decodeDXTRegion(mode,width,height,blocks.data(),blocks.size()-1,0,0,width,height);}catch(const std::invalid_argument&){rejected=true;}
        require(rejected);
    }
    uint8_t block[8]{};
    for(unsigned kind=0;kind<4;++kind){bool rejected=false;
        try{decodeDXTRegion(1,4,4,kind==3?nullptr:block,8,kind==0?1:0,0,kind==1?0:kind==2?5:4,4);}
        catch(const std::invalid_argument&){rejected=true;}require(rejected);}
    std::printf("DXT1/3/5: %u regions, partial edge blocks, source stride, invalid/truncated/null inputs passed\n",rectangles);
}
