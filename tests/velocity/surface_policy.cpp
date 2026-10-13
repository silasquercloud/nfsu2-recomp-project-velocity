#include "velocity_surface.h"
#include <stdexcept>
#include <cstdio>
int main(){
    auto require=[](bool ok){if(!ok)throw std::runtime_error("Unsupported surface transform selected");};
    for(unsigned rotation:{VK_SURFACE_TRANSFORM_ROTATE_90_BIT_KHR,VK_SURFACE_TRANSFORM_ROTATE_180_BIT_KHR,VK_SURFACE_TRANSFORM_ROTATE_270_BIT_KHR}){
        require(velocity::compositorTransform(rotation|VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR)==VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR);
        require(velocity::compositorTransform(rotation|VK_SURFACE_TRANSFORM_INHERIT_BIT_KHR)==VK_SURFACE_TRANSFORM_INHERIT_BIT_KHR);
        require(velocity::compositorTransform(rotation)==0);
    }
    require(velocity::compositorTransform(0)==0);
    std::puts("Surface transform: identity/inherit capability fallback and explicit unsupported prerotation passed (not a physical rotation test)");
}
