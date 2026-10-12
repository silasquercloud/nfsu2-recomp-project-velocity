#pragma once
#include <cstdint>
#include <vector>
#include <stdexcept>
#include <cstring>

// Decode only a block-aligned rectangle into RGBA8, including partial mip edges.
inline std::vector<uint8_t> decodeDXTRegion(unsigned mode,uint32_t width,uint32_t height,
        const uint8_t* source,size_t sourceBytes,uint32_t left,uint32_t top,
        uint32_t regionWidth,uint32_t regionHeight){
    if(mode!=1&&mode!=3&&mode!=5)throw std::invalid_argument("Unsupported DXT mode");
    uint64_t blocksWide=(uint64_t(width)+3)/4,blocksHigh=(uint64_t(height)+3)/4;
    uint32_t blockBytes=mode==1?8:16;
    if(!width||!height||blocksWide*blocksHigh*blockBytes>sourceBytes||uint64_t(width)*height>0x1fffffff)
        throw std::invalid_argument("Invalid DXT dimensions or truncated blocks");
    if(!source||!regionWidth||!regionHeight||left>=width||top>=height||
       regionWidth>width-left||regionHeight>height-top||left%4||top%4||
       ((left+regionWidth)%4&&left+regionWidth!=width)||
       ((top+regionHeight)%4&&top+regionHeight!=height))
        throw std::invalid_argument("Invalid DXT rectangle");
    std::vector<uint8_t> rgba(size_t(regionWidth)*regionHeight*4);
    for(uint32_t by=top/4;by<(uint64_t(top)+regionHeight+3)/4;++by)
    for(uint32_t bx=left/4;bx<(uint64_t(left)+regionWidth+3)/4;++bx){
        const uint8_t* block=source+(size_t(by)*blocksWide+bx)*blockBytes;
        const uint8_t* colors=block+(mode==1?0:8);
        uint16_t endpoints[2];std::memcpy(endpoints,colors,4);
        uint8_t palette[4][4]{};
        for(unsigned i=0;i<2;++i){uint32_t r=(endpoints[i]>>11)&31,g=(endpoints[i]>>5)&63,b=endpoints[i]&31;
            palette[i][0]=(r<<3)|(r>>2);palette[i][1]=(g<<2)|(g>>4);palette[i][2]=(b<<3)|(b>>2);palette[i][3]=255;}
        if(mode!=1||endpoints[0]>endpoints[1]){for(unsigned c=0;c<3;++c){palette[2][c]=(2*palette[0][c]+palette[1][c])/3;palette[3][c]=(palette[0][c]+2*palette[1][c])/3;}palette[2][3]=palette[3][3]=255;}
        else {for(unsigned c=0;c<3;++c)palette[2][c]=(palette[0][c]+palette[1][c])/2;palette[2][3]=255;}
        uint32_t indices;std::memcpy(&indices,colors+4,4);
        uint8_t alpha[8]{};uint64_t alphaIndices=0;
        if(mode==5){alpha[0]=block[0];alpha[1]=block[1];
            if(alpha[0]>alpha[1])for(unsigned i=1;i<=6;++i)alpha[i+1]=((7-i)*alpha[0]+i*alpha[1])/7;
            else {for(unsigned i=1;i<=4;++i)alpha[i+1]=((5-i)*alpha[0]+i*alpha[1])/5;alpha[6]=0;alpha[7]=255;}
            for(unsigned i=0;i<6;++i)alphaIndices|=uint64_t(block[2+i])<<(8*i);
        }
        for(unsigned p=0;p<16;++p){uint32_t x=bx*4+(p&3),y=by*4+(p>>2);if(x>=left+regionWidth||y>=top+regionHeight)continue;
            auto out=rgba.data()+(size_t(y-top)*regionWidth+x-left)*4;std::memcpy(out,palette[(indices>>(p*2))&3],4);
            if(mode==3)out[3]=((block[p/2]>>((p&1)*4))&15)*17;
            if(mode==5)out[3]=alpha[(alphaIndices>>(p*3))&7];
        }
    }
    return rgba;
}

// Preserve the full-mip API used by existing callers and tests.
inline std::vector<uint8_t> decodeDXT(unsigned mode,uint32_t width,uint32_t height,
                                    const uint8_t* source,size_t sourceBytes){
    return decodeDXTRegion(mode,width,height,source,sourceBytes,0,0,width,height);
}
