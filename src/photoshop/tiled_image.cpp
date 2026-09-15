#include "tiled_image.h"
#include <algorithm>
#include <cmath>
#include <exception>
#include <new>

namespace photoshop_dlss5 {
namespace {
constexpr int maxDimension=300000;
std::size_t offset(int x,int y,int width,int channels=4) {
    return (std::size_t(y)*width+x)*channels;
}
int tileCount(int length,int core,int step) {
    return length<=core?1:1+(length-core+step-1)/step;
}
float ramp(int p,int overlap) {
    const float t=(float(p)+.5F)/float(overlap);
    return t*t*(3-2*t);
}
float weight(int p,int core,int overlap,bool first,bool last) {
    if(!first&&p<overlap)return ramp(p,overlap);
    if(!last&&p>=core-overlap)return 1-ramp(p-(core-overlap),overlap);
    return 1;
}
bool readPadded(ImageSize image,ImageRect tile,int extent,const Callbacks& callbacks,
    std::vector<float>& input,std::string& error) {
    const int x=std::max(0,tile.x),y=std::max(0,tile.y);
    const int right=std::min(image.width,tile.x+extent),bottom=std::min(image.height,tile.y+extent);
    const int dx=x-tile.x,dy=y-tile.y,readWidth=right-x,readHeight=bottom-y;
    if(!callbacks.read({x,y,readWidth,readHeight},input.data()+offset(dx,dy,extent),
        std::size_t(extent)*4,error))return false;
    for(int row=dy;row<dy+readHeight;++row) {
        float* line=input.data()+offset(0,row,extent);
        for(int col=0;col<dx;++col)std::copy_n(line+dx*4,4,line+col*4);
        for(int col=dx+readWidth;col<extent;++col)std::copy_n(line+(dx+readWidth-1)*4,4,line+col*4);
    }
    for(int row=0;row<dy;++row)std::copy_n(input.data()+offset(0,dy,extent),
        std::size_t(extent)*4,input.data()+offset(0,row,extent));
    for(int row=dy+readHeight;row<extent;++row)std::copy_n(input.data()+offset(0,dy+readHeight-1,extent),
        std::size_t(extent)*4,input.data()+offset(0,row,extent));
    return true;
}
}

bool planTiles(ImageSize image,const TileOptions& options,TilePlan& plan,std::string& error) {
    error.clear();plan={};
    if(image.width<1||image.height<1||image.width>maxDimension||image.height>maxDimension) {
        error="Image dimensions must be between 1 and 300,000 pixels.";return false;
    }
    if(options.core<16||options.core>2048||options.context<0||options.context>512||
        options.overlap<1||options.overlap>options.core/2) {
        error="Invalid tile size, context or overlap.";return false;
    }
    plan.extent=options.core+2*options.context;plan.step=options.core-options.overlap;
    plan.columns=tileCount(image.width,options.core,plan.step);
    plan.rows=tileCount(image.height,options.core,plan.step);
    plan.tiles=std::uint64_t(plan.columns)*plan.rows;
    // Input, neural output, finished core, a bottom overlap scanline strip and
    // the right overlap of one tile. No full-document RGBA allocation.
    // Reserve ten float planes for the finishing kernel's temporary buffers;
    // driver/NGX GPU allocations and the host's own buffers are separate.
    const auto area=std::uint64_t(plan.extent)*plan.extent;
    plan.workingBytes=sizeof(float)*(area*22+std::uint64_t(options.core)*options.core*4+
        std::uint64_t(image.width)*options.overlap*3+
        std::uint64_t(options.core)*options.overlap*3);
    if(plan.workingBytes>options.memoryBudget) {
        error="The tile working set exceeds the configured memory budget. Reduce tile size or overlap.";
        return false;
    }
    return true;
}

RenderResult renderTiles(ImageSize image,const TileOptions& options,const Callbacks& callbacks,
    const Kernel& kernel,std::string& error) {
    TilePlan plan;
    if(!planTiles(image,options,plan,error))return RenderResult::Failed;
    if(!callbacks.read||!callbacks.stage||!kernel) {
        error="Missing image reader, transaction writer or processing engine.";return RenderResult::Failed;
    }
    std::uint64_t completed=0;
    const auto cancelled=[&]{return callbacks.progress&&!callbacks.progress(completed,plan.tiles);};
    try {
        if(cancelled()) {error="Cancelled.";return RenderResult::Cancelled;}
        const int core=options.core,pad=options.context,overlap=options.overlap,extent=plan.extent;
        std::vector<float> input(std::size_t(extent)*extent*4),neural(input.size());
        std::vector<float> result(std::size_t(core)*core*4);
        std::vector<float> bottom(std::size_t(image.width)*overlap*3,0);
        std::vector<float> left(std::size_t(core)*overlap*3,0);
        for(int row=0;row<plan.rows;++row) {
            std::fill(left.begin(),left.end(),0.0F);
            const int y=row*plan.step;
            const bool lastRow=row==plan.rows-1;
            const int outputHeight=std::min(lastRow?core:plan.step,image.height-y);
            for(int column=0;column<plan.columns;++column) {
                if(cancelled()) {error="Cancelled.";return RenderResult::Cancelled;}
                const int x=column*plan.step;
                const bool lastColumn=column==plan.columns-1;
                const int outputWidth=std::min(lastColumn?core:plan.step,image.width-x);
                if(!readPadded(image,{x-pad,y-pad,extent,extent},extent,callbacks,input,error)) {
                    if(error.empty())error="Could not read the original image.";
                    return RenderResult::Failed;
                }
                if(cancelled()) {error="Cancelled.";return RenderResult::Cancelled;}
                if(!kernel(input,neural,extent,extent,{image.width,image.height,x-pad,y-pad},error)) {
                    if(error.empty())error="The enhancement engine could not process a tile.";
                    return RenderResult::Failed;
                }
                if(neural.size()!=input.size()) {error="The enhancement engine returned an invalid tile.";return RenderResult::Failed;}
                if(cancelled()) {error="Cancelled.";return RenderResult::Cancelled;}
                for(int ty=0;ty<core;++ty)for(int tx=0;tx<core;++tx) {
                    const auto target=offset(tx,ty,core),source=offset(tx+pad,ty+pad,extent);
                    const float scale=weight(tx,core,overlap,column==0,lastColumn)*
                        weight(ty,core,overlap,row==0,lastRow);
                    for(int c=0;c<3;++c) {
                        if(!std::isfinite(neural[source+c])) {
                            error="The enhancement engine returned a non-finite pixel.";return RenderResult::Failed;
                        }
                        result[target+c]=neural[source+c]*scale;
                        if(column>0&&tx<overlap)result[target+c]+=left[offset(tx,ty,overlap,3)+c];
                    }
                    // Transparency is preserved exactly, never re-created by NGX or blended.
                    result[target+3]=input[source+3];
                }
                // Keep only this row's right contributions before adding the previous
                // row. Otherwise the four-way corner would count the top row twice.
                if(!lastColumn)for(int ty=0;ty<core;++ty)for(int tx=0;tx<overlap;++tx)
                    std::copy_n(result.data()+offset(plan.step+tx,ty,core),3,
                        left.data()+offset(tx,ty,overlap,3));
                if(row>0)for(int ty=0;ty<overlap;++ty)for(int tx=0;tx<outputWidth;++tx)
                    for(int c=0;c<3;++c)result[offset(tx,ty,core)+c]+=bottom[offset(x+tx,ty,image.width,3)+c];
                // Overwrite each bottom strip only after consuming the top strip.
                // Adjacent columns never overwrite one another's saved contributions.
                if(!lastRow)for(int ty=0;ty<overlap;++ty)for(int tx=0;tx<outputWidth;++tx)
                    std::copy_n(result.data()+offset(tx,plan.step+ty,core),3,
                        bottom.data()+offset(x+tx,ty,image.width,3));
                if(!callbacks.stage({x,y,outputWidth,outputHeight},result.data(),std::size_t(core)*4,error)) {
                    if(error.empty())error="Could not stage the enhanced image.";
                    return RenderResult::Failed;
                }
                ++completed;
            }
        }
        if(cancelled()) {error="Cancelled.";return RenderResult::Cancelled;}
        error.clear();return RenderResult::Success;
    }catch(const std::bad_alloc&) {
        error="There is not enough system memory for this tile size. Close other applications or use a smaller tile.";
    }catch(const std::exception& e) {error=std::string("Image processing failed: ")+e.what();}
    catch(...) {error="Image processing failed unexpectedly.";}
    return RenderResult::Failed;
}
}
