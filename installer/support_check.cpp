// Runs in its own process; the installer enforces a timeout and preserves the prior install.
#include <windows.h>
#include <dxgi1_6.h>
#include <d3d12.h>
#include <wrl/client.h>
#include "AEConfig.h"
#include "AE_Effect.h"
#include "AE_EffectCB.h"
#include "parameters.h"
#include <array>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <string>
#include <vector>
using Microsoft::WRL::ComPtr;
using namespace adobe_dlss5;
using MainFn=PF_Err(*)(PF_Cmd,PF_InData*,PF_OutData*,PF_ParamDef*[],PF_LayerDef*,void*);
static std::array<PF_ParamDef,ParamCount> controls{};
static PF_Err AddParam(PF_ProgPtr,PF_ParamIndex,PF_ParamDef* p) {
    if(p->uu.id<=0||p->uu.id>=ParamCount)return PF_Err_BAD_CALLBACK_PARAM;
    controls[p->uu.id]=*p;return 0;
}
static std::string escaped(const std::string& in) {
    std::string out;for(unsigned char c:in){if(c=='"'||c=='\\')out+='\\';
        if(c>=32)out+=char(c);else out+=' ';}return out;
}
static int report(int code,const std::string& gpu,const std::string& message,size_t memory=0) {
    std::printf("{\"ok\":%s,\"code\":%d,\"gpu\":\"%s\",\"dedicatedMemoryMiB\":%zu,\"message\":\"%s\"}\n",
        code?"false":"true",code,escaped(gpu).c_str(),memory,escaped(message).c_str());return code;
}
static int check() {
    std::setvbuf(stdout,nullptr,_IONBF,0);
    std::string gpu;size_t memory=0;
    ComPtr<IDXGIFactory1> factory;
    if(FAILED(CreateDXGIFactory1(IID_PPV_ARGS(&factory))))return report(10,gpu,"Windows graphics initialization failed.");
    bool suitable=false;
    for(UINT i=0;;++i) {
        ComPtr<IDXGIAdapter1> adapter;if(factory->EnumAdapters1(i,&adapter)==DXGI_ERROR_NOT_FOUND)break;
        if(!adapter)continue;
        DXGI_ADAPTER_DESC1 d{};if(FAILED(adapter->GetDesc1(&d))||d.VendorId!=0x10de||(d.Flags&DXGI_ADAPTER_FLAG_SOFTWARE))continue;
        char name[512]{};WideCharToMultiByte(CP_UTF8,0,d.Description,-1,name,sizeof(name),nullptr,nullptr);
        gpu=name;memory=d.DedicatedVideoMemory/(1024*1024);
        ComPtr<ID3D12Device> device;
        if(SUCCEEDED(D3D12CreateDevice(adapter.Get(),D3D_FEATURE_LEVEL_12_0,IID_PPV_ARGS(&device)))){suitable=true;break;}
    }
    if(!suitable)return report(11,gpu,"A compatible NVIDIA GPU with Direct3D 12 support is required. Check the NVIDIA driver.",memory);
    if(gpu.find("RTX")==std::string::npos)return report(12,gpu,"The neural runtime requires an NVIDIA RTX GPU. GTX, AMD and Intel graphics are not supported.",memory);
    wchar_t exe[32768]{};GetModuleFileNameW(nullptr,exe,32768);
    const auto modulePath=std::filesystem::path(exe).parent_path()/L"4x4Tools-DLSS5.aex";
    HMODULE module=LoadLibraryExW(modulePath.c_str(),nullptr,LOAD_WITH_ALTERED_SEARCH_PATH);
    if(!module)return report(20,gpu,"The plug-in could not be loaded. Extract the complete package and check that its files were not quarantined.",memory);
    auto effect=reinterpret_cast<MainFn>(GetProcAddress(module,"EffectMain"));
    if(!effect){FreeLibrary(module);return report(21,gpu,"The package is missing the Adobe effect entry point.",memory);}
    PF_InData in{};PF_OutData out{};in.inter.add_param=AddParam;
    PF_Err error=effect(PF_Cmd_GLOBAL_SETUP,&in,&out,nullptr,nullptr,nullptr);
    if(!error)error=effect(PF_Cmd_PARAMS_SETUP,&in,&out,nullptr,nullptr,nullptr);
    constexpr int w=180,h=225;std::vector<PF_Pixel> source(w*h),dest(w*h);
    for(int y=0;y<h;++y)for(int x=0;x<w;++x){auto& p=source[y*w+x];p.alpha=255;
        p.red=A_u_char(x*255/(w-1));p.green=A_u_char(y*255/(h-1));p.blue=A_u_char(((x/12+y/12)&1)?204:51);}
    in.width=w;in.height=h;in.time_scale=24;in.time_step=1;
    auto& input=controls[0].u.ld;input.width=w;input.height=h;input.rowbytes=w*sizeof(PF_Pixel);input.data=source.data();
    PF_LayerDef output=input;output.data=dest.data();
    PF_ParamDef* params[ParamCount];for(int i=0;i<ParamCount;++i)params[i]=&controls[i];
    if(!error)error=effect(PF_Cmd_RENDER,&in,&out,params,&output,nullptr);
    std::string detail=out.return_msg;
    size_t changed=0;bool alpha=true;
    for(size_t i=0;i<source.size();++i){changed+=std::abs(int(source[i].red)-dest[i].red)>1;alpha&=source[i].alpha==dest[i].alpha;}
    const auto cleanup=effect(PF_Cmd_GLOBAL_SETDOWN,&in,&out,nullptr,nullptr,nullptr);FreeLibrary(module);
    if(error)return report(22,gpu,"The bundled neural runtime could not process a frame on this GPU/driver. "+detail,memory);
    if(cleanup)return report(23,gpu,"GPU processing completed but cleanup failed. Installation was not started.",memory);
    if(changed<100||!alpha)return report(24,gpu,"The GPU test did not produce a valid enhanced frame. Installation was not started.",memory);
    return report(0,gpu,"Neural processing and GPU cleanup passed with the bundled runtime.",memory);
}
int main() {
    try { return check(); }
    catch(const std::exception& e) { return report(25,"",std::string("GPU check failed: ")+e.what()); }
    catch(...) { return report(25,"","GPU check failed unexpectedly."); }
}
