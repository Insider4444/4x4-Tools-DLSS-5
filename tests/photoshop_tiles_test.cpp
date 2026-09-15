#include "tiled_image.h"
#include "finishing.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>
#include <stdexcept>
using namespace photoshop_dlss5;
static void require(bool ok,const char* message) {if(!ok)throw std::runtime_error(message);}
static float sample(int x,int y,int channel) {
    if(channel==3)return float((x+y)%17)/16;
    return float((std::uint64_t(x)*31+std::uint64_t(y)*19+channel*17)%32769)/32768;
}
static Callbacks source(ImageSize size) {
    Callbacks cb;
    cb.read=[size](ImageRect r,float* pixels,std::size_t stride,std::string&) {
        require(r.x>=0&&r.y>=0&&r.x+r.width<=size.width&&r.y+r.height<=size.height,"Out-of-bounds image read");
        for(int y=0;y<r.height;++y)for(int x=0;x<r.width;++x)for(int c=0;c<4;++c)
            pixels[std::size_t(y)*stride+x*4+c]=sample(r.x+x,r.y+y,c);
        return true;
    };
    return cb;
}
static const Kernel identity=[](const auto& in,auto& out,int,int,const auto&,std::string&){out=in;return true;};
int main() {
    try {
        std::string error;TilePlan plan;
        require(planTiles({300000,300000},{},plan,error)&&plan.tiles>90000&&plan.workingBytes<512ULL*1024*1024,
            "Large-document plan overflowed or allocated a full frame");
        require(!planTiles({std::numeric_limits<int>::max(),300000},{},plan,error),"Invalid dimensions accepted");
        TileOptions small;small.memoryBudget=1;
        require(!planTiles({8192,8192},small,plan,error),"Memory budget ignored");
        // Awkward final tiles, one-pixel inputs, horizontal and vertical panoramas,
        // genuine 8K pixel coverage and coordinates beyond the legacy 16-bit limit.
        for(const auto size:{ImageSize{1,1},ImageSize{1025,1025},ImageSize{1985,1089},
            ImageSize{8192,4320},ImageSize{40001,7},ImageSize{7,40001}}) {
            auto cb=source(size);std::uint64_t count=0;
            std::vector<unsigned char> coverage(std::size_t(size.width)*size.height,0);
            cb.stage=[&](ImageRect r,const float* p,std::size_t stride,std::string&) {
                for(int y=0;y<r.height;++y)for(int x=0;x<r.width;++x) {
                    require(++coverage[std::size_t(r.y+y)*size.width+r.x+x]==1,"Overlapping output rectangles");
                    for(int c=0;c<4;++c)require(std::abs(p[std::size_t(y)*stride+x*4+c]-sample(r.x+x,r.y+y,c))<(c==3?1.e-20F:2.e-7F),
                        "Tile blend changed pixels, alpha, or left a seam");
                    ++count;
                }return true;
            };
            require(renderTiles(size,{},cb,identity,error)==RenderResult::Success,error.c_str());
            require(count==std::uint64_t(size.width)*size.height,"Not every source pixel was rendered");
        }
        // Compare tiled spatial finishing against a whole-frame reference. This
        // exposes masks restarting per tile and different lighting blur radii.
        const ImageSize size{1379,1171};auto cb=source(size);
        std::vector<float> full(std::size_t(size.width)*size.height*4);
        cb.read({0,0,size.width,size.height},full.data(),std::size_t(size.width)*4,error);
        auto neural=full;
        for(std::size_t i=0;i<neural.size();++i)if(i%4!=3)neural[i]+=.05F;
        adobe_dlss5::Settings settings;settings.exposureHold=75;settings.region=2;settings.texture=35;settings.detail=20;
        adobe_dlss5::finishFrame(full,neural,size.width,size.height,settings);
        cb.stage=[&](ImageRect r,const float* p,std::size_t stride,std::string&) {
            for(int y=0;y<r.height;++y)for(int x=0;x<r.width;++x)for(int c=0;c<4;++c)
                require(std::abs(p[std::size_t(y)*stride+x*4+c]-neural[(std::size_t(r.y+y)*size.width+r.x+x)*4+c])<2.e-5F,
                    "Tiled lighting or masks differ from full-image finishing");
            return true;
        };
        Kernel finish=[&](const auto& in,auto& out,int w,int h,const auto& geometry,std::string&) {
            out=in;for(std::size_t i=0;i<out.size();++i)if(i%4!=3)out[i]+=.05F;
            adobe_dlss5::finishFrame(in,out,w,h,settings,geometry);return true;
        };
        require(renderTiles(size,{},cb,finish,error)==RenderResult::Success,error.c_str());
        // A host transaction must not commit on cancellation or GPU/read/write failure.
        cb=source({2049,2049});int staged=0;
        cb.stage=[&](ImageRect,const float*,std::size_t,std::string&){++staged;return true;};
        cb.progress=[](auto done,auto){return done<1;};
        require(renderTiles({2049,2049},{},cb,identity,error)==RenderResult::Cancelled&&staged==1,"Cancellation ignored");
        cb.progress={};staged=0;
        Kernel fail=[](const auto&,auto&,int,int,const auto&,std::string& e){e="GPU unavailable";return false;};
        require(renderTiles({2049,2049},{},cb,fail,error)==RenderResult::Failed&&staged==0&&error=="GPU unavailable","GPU error lost");
        std::puts("PASS Photoshop: 8K coverage; 40,001-pixel panoramas; 300K coordinate planning; overlap corners; exact alpha; document masks/lighting; memory guards; cancellation and GPU failure.");
        return 0;
    }catch(const std::exception& e){std::fprintf(stderr,"FAIL: %s\n",e.what());return 1;}
}
