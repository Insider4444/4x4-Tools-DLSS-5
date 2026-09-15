#include "filter.h"
#include "parameters.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <map>
#include <stdexcept>
using photoshop_dlss5::FilterSettings;
using Entry=void(*)(int16,FilterRecordPtr,intptr_t*,int16*);
namespace {
std::map<Handle,int32> handles;
Handle createHandle(int32 bytes) {auto h=new char*(new char[bytes]{});handles[h]=bytes;return h;}
void disposeHandle(Handle h) {delete[] *h;delete h;handles.erase(h);}
int32 handleSize(Handle h) {return handles.at(h);}
Ptr lockHandle(Handle h,Boolean) {return *h;}
void unlockHandle(Handle) {}
void require(bool ok,const char* message) {if(!ok)throw std::runtime_error(message);}
struct Host {
    FilterRecord r{};BigDocumentStruct big{};HandleProcs procs{};Str255 error{};
    int w,h,depth,bytes;bool cancel=false,failRead=false;
    std::vector<std::byte> source,output,inputTile,outputTile;
    VRect pending{};
    Host(int width,int height,int bitDepth):w(width),h(height),depth(bitDepth),bytes(bitDepth/8) {
        r.bigDocumentData=&big;r.handleProcs=&procs;r.errorString=&error;r.planes=3;r.depth=depth;
        r.imageMode=depth==32?plugInModeRGB96:depth==16?plugInModeRGB48:plugInModeRGBColor;
        r.inLayerPlanes=3;r.outLayerPlanes=3;
        big.imageSize32={h,w};big.filterRect32={0,0,h,w};
        procs.newProc=createHandle;procs.disposeProc=disposeHandle;procs.getSizeProc=handleSize;
        procs.lockProc=lockHandle;procs.unlockProc=unlockHandle;
        r.parameters=createHandle(sizeof(FilterSettings));
        FilterSettings settings;settings.image.mix=0;settings.tileCore=512;
        std::memcpy(*r.parameters,&settings,sizeof(settings));
        source.resize(std::size_t(w)*h*3*bytes);
        for(int y=0;y<h;++y)for(int x=0;x<w;++x)for(int c=0;c<3;++c) {
            const auto p=((std::size_t(y)*w+x)*3+c)*bytes;
            if(depth==8)source[p]=std::byte((x*11+y*17+c*33)%256);
            else if(depth==16) {auto value=std::uint16_t((std::uint64_t(x)*31+y*17+c*33)%32769);std::memcpy(source.data()+p,&value,2);}
            else {const float value=-.1F+float((x*11+y*17+c*33)%257)/128;std::memcpy(source.data()+p,&value,4);}
        }
        output=source;
    }
    ~Host(){disposeHandle(r.parameters);}
    void flush() {
        if(pending.right<=pending.left)return;
        for(int y=0;y<pending.bottom-pending.top;++y)
            std::copy_n(outputTile.data()+std::size_t(y)*r.outRowBytes,
                std::size_t(pending.right-pending.left)*3*bytes,
                output.data()+(std::size_t(y+pending.top)*w+pending.left)*3*bytes);
        pending={};
    }
    OSErr advance() {
        flush();r.inData=nullptr;r.outData=nullptr;
        auto rect=big.inRect32;
        if(rect.right>rect.left) {
            if(failRead)return paramErr;
            require(rect.left>=0&&rect.top>=0&&rect.right<=w&&rect.bottom<=h,"Invalid input rectangle");
            r.inPlaneBytes=bytes;r.inColumnBytes=3*bytes;r.inRowBytes=(rect.right-rect.left)*3*bytes+16;
            inputTile.resize(std::size_t(r.inRowBytes)*(rect.bottom-rect.top));
            for(int y=0;y<rect.bottom-rect.top;++y)
                std::copy_n(source.data()+(std::size_t(y+rect.top)*w+rect.left)*3*bytes,
                    std::size_t(rect.right-rect.left)*3*bytes,inputTile.data()+std::size_t(y)*r.inRowBytes);
            r.inData=inputTile.data();
        }
        rect=big.outRect32;
        if(rect.right>rect.left) {
            require(rect.left>=big.filterRect32.left&&rect.top>=big.filterRect32.top&&
                rect.right<=big.filterRect32.right&&rect.bottom<=big.filterRect32.bottom,"Output escaped the selection");
            r.outPlaneBytes=bytes;r.outColumnBytes=3*bytes;r.outRowBytes=(rect.right-rect.left)*3*bytes+32;
            outputTile.assign(std::size_t(r.outRowBytes)*(rect.bottom-rect.top),std::byte(0));
            r.outData=outputTile.data();pending=rect;
        }
        return noErr;
    }
};
Host* active=nullptr;
OSErr advance(){return active->advance();}
Boolean abortRender(){return active->cancel;}
int16 run(Entry entry,Host& host) {
    active=&host;host.r.advanceState=advance;host.r.abortProc=abortRender;
    intptr_t data=0;int16 result=0;
    entry(filterSelectorPrepare,&host.r,&data,&result);
    require(!result&&host.big.PluginUsing32BitCoordinates,"Prepare failed to enable large coordinates");
    entry(filterSelectorStart,&host.r,&data,&result);host.flush();
    if(result==errReportString)std::printf("Host message: %.*s\n",host.error[0],&host.error[1]);
    int16 finish=0;entry(filterSelectorFinish,&host.r,&data,&finish);require(!finish,"Finish failed");
    return result;
}
}
int wmain(int argc,wchar_t** argv) {
    try {
        require(argc>=2,"Expected plugin path");
        auto module=LoadLibraryExW(argv[1],nullptr,LOAD_WITH_ALTERED_SEARCH_PATH);
        require(module!=nullptr,"Photoshop module did not load");
        require(FindResourceW(module,L"JSON_PIPL",L"JSON")!=nullptr,"Photoshop registration resource missing");
        auto entry=reinterpret_cast<Entry>(GetProcAddress(module,"PluginMain"));require(entry!=nullptr,"Native entry point missing");
        if(argc>2) {
            Host host(1089,777,16);FilterSettings settings;settings.image=adobe_dlss5::lookRecipe(2);settings.tileCore=512;
            std::memcpy(*host.r.parameters,&settings,sizeof(settings));
            require(run(entry,host)==noErr,"Actual GPU image enhancement failed");
            require(host.output!=host.source,"Neural processing did not change the image");
            std::puts("PASS Photoshop actual GPU: multi-tile RGB16 image processed and committed.");
        }else {
            for(const int depth:{8,16,32}) {
                Host host(1025,513,depth);host.big.filterRect32={7,11,509,1019};
                require(run(entry,host)==noErr,"Native bypass render failed");
                if(depth!=32)require(host.source==host.output,"Integer source precision changed in bypass");
                else for(std::size_t p=0;p<host.source.size();p+=4) {
                    float a,b;std::memcpy(&a,host.source.data()+p,4);std::memcpy(&b,host.output.data()+p,4);
                    require(std::abs(a-b)<3.e-7F,"Float/HDR source precision changed in bypass");
                }
            }
            Host large(40001,3,16);require(run(entry,large)==noErr&&large.output==large.source,"Large Photoshop coordinates failed");
            Host cancelled(37,23,8);cancelled.cancel=true;
            require(run(entry,cancelled)==userCanceledErr&&cancelled.output==cancelled.source,"Cancelled render modified the image");
            Host failed(37,23,8);failed.failRead=true;
            require(run(entry,failed)==errReportString&&failed.output==failed.source,"Failed read modified the image");
            std::puts("PASS Photoshop SDK host contract: registration; RGB8/16/32; padded strides; HDR; selections; 40,001-pixel coordinates; cancellation and failed reads.");
        }
        FreeLibrary(module);return 0;
    }catch(const std::exception& e){std::fprintf(stderr,"FAIL: %s\n",e.what());return 1;}
}
