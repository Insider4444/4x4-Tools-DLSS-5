#include "filter.h"
#include "PIAbout.h"
#include "parameters.h"
#include "finishing.h"
#include "color_pipeline.h"
#include "scratch_image.h"
#include "version.h"
#include "Feature18Runtime.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <limits>
#include <stdexcept>

using namespace photoshop_dlss5;
using namespace adobe_dlss5;
namespace {
void trace(const std::string& stage) noexcept {
    if(GetEnvironmentVariableW(L"TOOLS_PS_TRACE",nullptr,0)>0)
        resolve_dlss5::writeDiagnosticLog("Photoshop: "+stage);
}
HWND owner(void* platform) {return platform?reinterpret_cast<HWND>(static_cast<PlatformData*>(platform)->hwnd):nullptr;}
VRect nativeRect(ImageRect r) {return {r.y,r.x,r.y+r.height,r.x+r.width};}
void clearRequests(FilterRecord& r) {
    r.inRect={};r.outRect={};r.maskRect={};
    if(r.bigDocumentData) {r.bigDocumentData->inRect32={};r.bigDocumentData->outRect32={};r.bigDocumentData->maskRect32={};}
}
void fail(FilterRecord& r,int16* result,const std::string& message) {
    *result=errReportString;
    if(r.errorString) {
        const std::string text="4x4Tools DLSS5: "+message;
        const auto count=std::min(std::size_t(255),text.size());
        (*r.errorString)[0]=static_cast<unsigned char>(count);
        std::memcpy(&(*r.errorString)[1],text.data(),count);
    }
}
FilterSettings loadSettings(FilterRecord& r) {
    FilterSettings value;value.image=lookRecipe(2);value.image.look=2;
    if(r.parameters&&r.handleProcs&&r.handleProcs->getSizeProc&&
        r.handleProcs->getSizeProc(r.parameters)==sizeof(FilterSettings)) {
        auto* p=r.handleProcs->lockProc(r.parameters,true);
        if(p) {FilterSettings saved;std::memcpy(&saved,p,sizeof(saved));
            if(saved.magic==value.magic&&saved.version==value.version)value=saved;
            r.handleProcs->unlockProc(r.parameters);
        }
    }
    return value;
}
void saveSettings(FilterRecord& r,const FilterSettings& value) {
    if(!r.handleProcs||!r.handleProcs->newProc||!r.handleProcs->getSizeProc||!r.handleProcs->lockProc||!r.handleProcs->unlockProc)
        throw std::runtime_error("Photoshop's parameter storage is unavailable.");
    if(!r.parameters)r.parameters=r.handleProcs->newProc(sizeof(value));
    if(!r.parameters||r.handleProcs->getSizeProc(r.parameters)!=sizeof(value))
        throw std::runtime_error("Could not store the filter settings.");
    auto* p=r.handleProcs->lockProc(r.parameters,true);
    if(!p)throw std::runtime_error("Could not lock the filter settings.");
    std::memcpy(p,&value,sizeof(value));r.handleProcs->unlockProc(r.parameters);
}
void readDescriptor(FilterRecord& r,FilterSettings& value) {
    auto* descriptor=r.descriptorParameters;
    if(!descriptor||!descriptor->descriptor||!descriptor->readDescriptorProcs)return;
    auto* procs=descriptor->readDescriptorProcs;
    auto token=procs->openReadDescriptorProc(descriptor->descriptor,nullptr);
    if(!token)throw std::runtime_error("Could not read the recorded filter settings.");
    std::array<bool,ParamCount> present{};std::array<float,ParamCount> values{};
    DescriptorKeyID key=0;DescriptorTypeID type=0;int32 flags=0;OSErr status=0;
    while(procs->getKeyProc(token,&key,&type,&flags)) {
        int index=-1;for(int i=1;i<ParamCount;++i)if(isValue(i)&&key==descriptorKey(i))index=i;
        if(index<0&&key!=descriptorKey(99))continue;
        real64 number=0;
        if(type=='long') {int32 integer=0;status=procs->getIntegerProc(token,&integer);number=integer;}
        else status=procs->getFloatProc(token,&number);
        if(status)break;
        if(index>=0) {values[index]=static_cast<float>(number);present[index]=true;}
        else value.tileCore=std::isfinite(number)?int(std::clamp(number,512.0,2048.0)):1024;
    }
    const auto close=procs->closeReadDescriptorProc(token);
    if(status||close)throw std::runtime_error("The recorded filter parameters are invalid.");
    if(present[Look]) {setSetting(value.image,Look,values[Look]);resolveLook(value.image);}
    for(int i=1;i<ParamCount;++i)if(present[i])setSetting(value.image,i,values[i]);
}
void writeDescriptor(FilterRecord& r,const FilterSettings& value) {
    auto* descriptor=r.descriptorParameters;
    if(!descriptor||!descriptor->writeDescriptorProcs)return;
    auto* procs=descriptor->writeDescriptorProcs;
    auto token=procs->openWriteDescriptorProc();
    if(!token)throw std::runtime_error("Could not record filter settings.");
    OSErr status=0;
    for(int i=1;i<ParamCount;++i)if(isValue(i)) {
        real64 number=getSetting(value.image,i);
        const auto next=procs->putFloatProc(token,descriptorKey(i),&number);if(next)status=next;
    }
    real64 core=value.tileCore;
    if(procs->putFloatProc(token,descriptorKey(99),&core))status=paramErr;
    PIDescriptorHandle output=nullptr;const auto close=procs->closeWriteDescriptorProc(token,&output);
    if(status||close) {if(output&&r.handleProcs)r.handleProcs->disposeProc(output);throw std::runtime_error("Could not record filter parameters.");}
    if(descriptor->descriptor&&r.handleProcs)r.handleProcs->disposeProc(descriptor->descriptor);
    descriptor->descriptor=output;descriptor->recordInfo=plugInDialogOptional;
}
float readSample(const std::byte* p,int depth) {
    if(depth==8)return float(*reinterpret_cast<const unsigned char*>(p))/255;
    if(depth==16) {std::uint16_t v;std::memcpy(&v,p,2);return float(v)/32768;}
    float value;std::memcpy(&value,p,4);return std::isfinite(value)?value:0.0F;
}
void writeSample(std::byte* p,int depth,float value) {
    if(!std::isfinite(value))throw std::runtime_error("Invalid output pixel.");
    if(depth==8) {*reinterpret_cast<unsigned char*>(p)=static_cast<unsigned char>(std::lround(std::clamp(value,0.0F,1.0F)*255));}
    else if(depth==16) {const auto v=std::uint16_t(std::lround(std::clamp(value,0.0F,1.0F)*32768));std::memcpy(p,&v,2);}
    else std::memcpy(p,&value,4);
}
bool readPixels(FilterRecord& r,ImageRect rect,float* rgba,std::size_t stride,std::string& error) {
    clearRequests(r);r.bigDocumentData->inRect32=nativeRect(rect);r.inLoPlane=0;r.inHiPlane=2;
    const auto result=r.advanceState();
    if(result||!r.inData) {error="Photoshop could not supply the original RGB pixels ("+std::to_string(result)+").";return false;}
    const int sampleBytes=r.depth/8;
    const int plane=r.inPlaneBytes?r.inPlaneBytes:sampleBytes;
    const int column=r.inColumnBytes?r.inColumnBytes:sampleBytes*3;
    if(plane<sampleBytes||column<sampleBytes||r.inRowBytes==0) {error="Photoshop supplied an invalid pixel layout.";return false;}
    for(int y=0;y<rect.height;++y)for(int x=0;x<rect.width;++x) {
        const auto* p=static_cast<const std::byte*>(r.inData)+std::ptrdiff_t(y)*r.inRowBytes+std::ptrdiff_t(x)*column;
        for(int c=0;c<3;++c)rgba[std::size_t(y)*stride+x*4+c]=readSample(p+c*plane,r.depth);
        rgba[std::size_t(y)*stride+x*4+3]=1;
    }
    return true;
}
bool processTile(const std::vector<float>& input,std::vector<float>& output,int w,int h,
    const FrameGeometry& geometry,const FilterSettings& value,ColorPipeline& color,std::string& error) {
    auto settings=value.image;
    if(settings.mode!=2||settings.mix<=0||settings.strength<=0) {
        output=input;finishFrame(input,output,w,h,settings,geometry);return true;
    }
    std::vector<float> working=input;color.toWorking(working);
    settings.preserveInputPrecision=true;
    if(!processFrame(working,output,w,h,settings,error,geometry))return false;
    color.toDocument(output);
    for(int y=0;y<h;++y)for(int x=0;x<w;++x) {
        const auto p=(std::size_t(y)*w+x)*4;
        for(int c=0;c<3;++c) {
            if(settings.view==2&&x+geometry.originX<geometry.documentWidth*settings.wipe*.01F)output[p+c]=input[p+c];
            else if(settings.view==3)output[p+c]=std::abs(output[p+c]-input[p+c])*10;
        }
        output[p+3]=input[p+3];
    }
    return true;
}
void run(FilterRecord& r,intptr_t* data,int16* result) {
    trace("start; depth="+std::to_string(r.depth)+" planes="+std::to_string(r.planes));
    struct Cleanup {FilterRecord& r;~Cleanup(){clearRequests(r);shutdownEngine();}} cleanup{r};
    if(!r.bigDocumentData||!r.advanceState||r.planes<3||
        (r.imageMode!=plugInModeRGBColor&&r.imageMode!=plugInModeRGB48&&r.imageMode!=plugInModeRGB96)||
        (r.depth!=8&&r.depth!=16&&r.depth!=32)) {
        fail(r,result,"Select the RGB channels of an 8-, 16- or 32-bit image in Photoshop 2026 or newer.");return;
    }
    r.bigDocumentData->PluginUsing32BitCoordinates=1;r.autoMask=true;
    const auto image=r.bigDocumentData->imageSize32;
    const auto region=r.bigDocumentData->filterRect32;
    const ImageSize size{image.h,image.v};
    const ImageRect selected{region.left,region.top,region.right-region.left,region.bottom-region.top};
    if(selected.width<=0||selected.height<=0)return;
    if(selected.x<0||selected.y<0||region.right>size.width||region.bottom>size.height)
        throw std::runtime_error("Photoshop supplied an invalid selection rectangle.");
    trace("loading settings");
    auto value=loadSettings(r);trace("reading descriptor");readDescriptor(r,value);
    value.image.encoding=r.depth==32?2:1;
    ColorPipeline color;std::string error;
    std::vector<std::byte> profile;
    if(r.canUseICCProfiles&&r.iCCprofileData&&r.iCCprofileSize>0) {
        if(r.iCCprofileSize>16*1024*1024||!r.handleProcs)throw std::runtime_error("The document color profile is too large or unavailable.");
        profile.resize(r.iCCprofileSize);
        const auto* p=r.handleProcs->lockProc(r.iCCprofileData,true);
        if(!p)throw std::runtime_error("Could not read the document color profile.");
        std::memcpy(profile.data(),p,profile.size());r.handleProcs->unlockProc(r.iCCprofileData);
    }
    trace("initializing color transform");
    if(!color.initialize(profile.data(),unsigned(profile.size()),r.depth==32,error))throw std::runtime_error(error);
    const bool scripted=r.descriptorParameters&&r.descriptorParameters->descriptor;
    const bool show=r.descriptorParameters?r.descriptorParameters->playInfo==plugInDialogDisplay||(!scripted&&*data!=0):*data!=0;
    Preview preview=[&](const FilterSettings& settings,float cx,float cy,auto& original,auto& enhanced,int& w,int& h,std::string& e) {
        const int extent=1024,pad=256;
        const int ox=int((size.width-1)*cx*.01F)-extent/2,oy=int((size.height-1)*cy*.01F)-extent/2;
        const int left=std::max(0,ox),top=std::max(0,oy),right=std::min(size.width,ox+extent),bottom=std::min(size.height,oy+extent);
        const int dx=left-ox,dy=top-oy;
        std::vector<float> input(std::size_t(extent)*extent*4),output(input.size());
        if(!readPixels(r,{left,top,right-left,bottom-top},input.data()+(std::size_t(dy)*extent+dx)*4,extent*4,e))return false;
        for(int y=dy;y<dy+bottom-top;++y)for(int x=0;x<extent;++x) {
            if(x>=dx&&x<dx+right-left)continue;
            std::copy_n(input.data()+(std::size_t(y)*extent+std::clamp(x,dx,dx+right-left-1))*4,4,input.data()+(std::size_t(y)*extent+x)*4);
        }
        for(int y=0;y<extent;++y)if(y<dy||y>=dy+bottom-top)
            std::copy_n(input.data()+std::size_t(std::clamp(y,dy,dy+bottom-top-1))*extent*4,extent*4,input.data()+std::size_t(y)*extent*4);
        if(!processTile(input,output,extent,extent,{size.width,size.height,ox,oy},settings,color,e))return false;
        w=512;h=512;original.resize(std::size_t(w)*h*4);enhanced.resize(original.size());
        for(int y=0;y<h;++y) {
            std::copy_n(input.data()+(std::size_t(y+pad)*extent+pad)*4,w*4,original.data()+std::size_t(y)*w*4);
            std::copy_n(output.data()+(std::size_t(y+pad)*extent+pad)*4,w*4,enhanced.data()+std::size_t(y)*w*4);
        }
        color.toWorking(original);color.toWorking(enhanced);
        return true;
    };
    if(show&&!showDialog(owner(r.platformData),value,size,r.depth,preview)) {*result=userCanceledErr;return;}
    value.image.encoding=r.depth==32?2:1;
    TileOptions options;options.core=value.tileCore;
    TilePlan plan;if(!planTiles(size,options,plan,error))throw std::runtime_error(error);
    // Record parameters before committing pixels, so descriptor failures cannot
    // turn a completed image into a failed filter operation.
    trace("saving settings and descriptor");
    saveSettings(r,value);writeDescriptor(r,value);
    trace("allocating scratch image");
    ScratchImage scratch({selected.width,selected.height});
    Callbacks callbacks;
    callbacks.read=[&](ImageRect rect,float* p,std::size_t stride,std::string& e){return readPixels(r,rect,p,stride,e);};
    callbacks.stage=[&](ImageRect rect,const float* p,std::size_t stride,std::string&) {
        const int left=std::max(rect.x,selected.x),top=std::max(rect.y,selected.y);
        const int right=std::min(rect.x+rect.width,region.right),bottom=std::min(rect.y+rect.height,region.bottom);
        if(right>left&&bottom>top)scratch.write({left-selected.x,top-selected.y,right-left,bottom-top},
            p+std::size_t(top-rect.y)*stride+(left-rect.x)*4,stride);
        return true;
    };
    callbacks.progress=[&](std::uint64_t done,std::uint64_t total) {
        if(r.progressProc)r.progressProc(int32(done*90/total),100);
        return !r.abortProc||!r.abortProc();
    };
    trace("rendering tiles");
    const auto status=renderTiles(size,options,callbacks,[&](const auto& in,auto& out,int w,int h,const auto& g,std::string& e){
        return processTile(in,out,w,h,g,value,color,e);},error);
    if(status==RenderResult::Cancelled) {*result=userCanceledErr;return;}
    if(status!=RenderResult::Success)throw std::runtime_error(error);
    trace("committing staged pixels");
    std::vector<float> rgb;
    for(int y=0;y<selected.height;y+=256)for(int x=0;x<selected.width;x+=1024) {
        if(r.abortProc&&r.abortProc()) {*result=userCanceledErr;return;}
        const ImageRect rect{x,y,std::min(1024,selected.width-x),std::min(256,selected.height-y)};
        scratch.read(rect,rgb);
        clearRequests(r);r.outLoPlane=0;r.outHiPlane=2;
        r.bigDocumentData->outRect32=nativeRect({x+selected.x,y+selected.y,rect.width,rect.height});
        const auto statusOut=r.advanceState();
        if(statusOut||!r.outData)throw std::runtime_error("Photoshop could not accept the enhanced pixels.");
        const int bytes=r.depth/8,plane=r.outPlaneBytes?r.outPlaneBytes:bytes,column=r.outColumnBytes?r.outColumnBytes:3*bytes;
        if(plane<bytes||column<bytes||r.outRowBytes==0)throw std::runtime_error("Photoshop supplied an invalid output pixel layout.");
        for(int ty=0;ty<rect.height;++ty)for(int tx=0;tx<rect.width;++tx)for(int c=0;c<3;++c)
            writeSample(static_cast<std::byte*>(r.outData)+std::ptrdiff_t(ty)*r.outRowBytes+std::ptrdiff_t(tx)*column+c*plane,
                r.depth,rgb[(std::size_t(ty)*rect.width+tx)*3+c]);
        if(r.progressProc)r.progressProc(90+y*10/selected.height,100);
    }
    clearRequests(r);
    const auto flush=r.advanceState();if(flush)throw std::runtime_error("Photoshop could not finish the filter transaction.");
    if(r.progressProc)r.progressProc(100,100);
}
}

extern "C" __declspec(dllexport) void PluginMain(const int16 selector,FilterRecordPtr record,intptr_t* data,int16* result) {
    trace("selector="+std::to_string(selector));
    if(!result)return;*result=noErr;
    if(selector==filterSelectorAbout) {
        const auto* about=reinterpret_cast<const AboutRecord*>(record);
        MessageBoxW(about?owner(about->platformData):nullptr,L"4x4Tools DLSS5 for Photoshop\nFull-resolution neural enhancement\n\nRGB 8 / 16 / 32-bit, tiled large-document processing.\nSame dimensions; no super-resolution or frame generation.",L"4x4Tools DLSS5",MB_OK);return;
    }
    if(!record||!data) {*result=paramErr;return;}
    try {
        if(record->bigDocumentData)record->bigDocumentData->PluginUsing32BitCoordinates=1;
        switch(selector) {
            case filterSelectorParameters:*data=1;break;
            case filterSelectorPrepare:record->bufferSpace64=384LL*1024*1024;record->bufferSpace=384*1024*1024;break;
            case filterSelectorStart:run(*record,data,result);*data=0;break;
            case filterSelectorContinue:clearRequests(*record);break;
            case filterSelectorFinish:clearRequests(*record);*data=0;shutdownEngine();break;
            default:break;
        }
    }catch(const std::exception& e){clearRequests(*record);shutdownEngine();fail(*record,result,e.what());}
    catch(...){clearRequests(*record);shutdownEngine();fail(*record,result,"Unexpected processing failure.");}
}
