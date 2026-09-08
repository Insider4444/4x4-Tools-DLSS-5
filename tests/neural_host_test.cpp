#include <windows.h>
#include "AEConfig.h"
#include "AE_Effect.h"
#include "AE_EffectCB.h"
#include "effect_metadata.h"
#include "parameters.h"
using namespace adobe_dlss5;
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <thread>
#include <vector>
#include <filesystem>
#include <fstream>
#include <chrono>
using MainFn = PF_Err (*)(PF_Cmd, PF_InData*, PF_OutData*, PF_ParamDef*[], PF_LayerDef*, void*);
static void require(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
struct Request {
    PF_InData in{}; PF_OutData out{}; PF_ParamDef controls[ParamCount]{}; PF_ParamDef* params[ParamCount]{};
    PF_LayerDef dest{}; int layerCheckins = 0, paramCheckouts = 0, paramCheckins = 0;
    int bytes = 4, width = 640, height = 360;
    Request(int w = 640, int h = 360) : width(w), height(h) {
        in.effect_ref = reinterpret_cast<PF_ProgPtr>(this); in.width = width; in.height = height;
        in.time_scale = 24; in.time_step = 1;
        for (int i = 0; i < ParamCount; ++i) {
            params[i] = &controls[i];
            const auto& p=specs[i];
            switch(p.kind) {
                case Kind::Popup: controls[i].u.pd.value=int(p.initial);break;
                case Kind::Integer: controls[i].u.sd.value=int(p.initial);break;
                case Kind::Float: controls[i].u.fs_d.value=p.initial;break;
                case Kind::Checkbox: controls[i].u.bd.value=PF_Boolean(p.initial);break;
                default:break;
            }
        }
        controls[1].u.pd.value = 2; controls[2].u.pd.value = 1;
        controls[3].u.sd.value = 100; controls[4].u.pd.value = 1;
        controls[5].u.sd.value = 100; controls[6].u.pd.value = 1;
    }
};
static Request& ctx(PF_ProgPtr p) { return *reinterpret_cast<Request*>(p); }
static PF_Err GetParam(PF_ProgPtr p, PF_ParamIndex i, A_long, A_long, A_u_long, PF_ParamDef* out) {
    auto& r=ctx(p); ++r.paramCheckouts; *out=r.controls[i]; return 0;
}
static PF_Err PutParam(PF_ProgPtr p, PF_ParamDef*) { ++ctx(p).paramCheckins; return 0; }
static PF_Err GetLayer(PF_ProgPtr p, PF_ParamIndex, A_long, const PF_RenderRequest* req,
    A_long, A_long, A_u_long, PF_CheckoutResult* out) {
    auto& r=ctx(p);
    if (req->rect.left != 0 || req->rect.top != 0 || req->rect.right != r.width || req->rect.bottom != r.height) return 990;
    out->result_rect = out->max_result_rect = {0,0,r.width,r.height}; return 0;
}
static PF_Err GetPixels(PF_ProgPtr p, A_long, PF_EffectWorld** out) { *out=&ctx(p).controls[0].u.ld; return 0; }
static PF_Err GetOutput(PF_ProgPtr p, PF_EffectWorld** out) { *out=&ctx(p).dest; return 0; }
static PF_Err PutPixels(PF_ProgPtr p, A_long) { ++ctx(p).layerCheckins; return 0; }
template<class Pixel> static void Frame(MainFn effect, int depth, float max, int width = 640, int height = 360) {
    Request r(width, height);
    r.bytes=sizeof(Pixel);
    const int srcStride=r.width*sizeof(Pixel)+16, dstStride=r.width*sizeof(Pixel)+32;
    std::vector<unsigned char> input(srcStride*r.height,0x5a), output(dstStride*r.height,0xcd);
    for (int y=0;y<r.height;++y) {
        auto* row=reinterpret_cast<Pixel*>(input.data()+y*srcStride);
        for (int x=0;x<r.width;++x) {
            const float a = x==0 ? 0.0F : (x%3==0 ? 0.5F : 1.0F);
            auto& p=row[x];
            p.alpha=static_cast<decltype(p.alpha)>(a*max);
            p.red=static_cast<decltype(p.red)>((x*255/(r.width-1))/255.0F*a*max);
            p.green=static_cast<decltype(p.green)>((y*255/(r.height-1))/255.0F*a*max);
            p.blue=static_cast<decltype(p.blue)>((((x/16+y/16)&1)?0.8F:0.2F)*a*max);
            if (!x) p.red=static_cast<decltype(p.red)>(0.25F*max); // hidden RGB preservation
        }
    }
    auto& world=r.controls[0].u.ld;
    world.width=r.width; world.height=r.height; world.rowbytes=srcStride;
    world.world_flags=depth==16?PF_WorldFlag_DEEP:0;
    world.data=reinterpret_cast<PF_Pixel*>(input.data());
    r.dest=world; r.dest.rowbytes=dstStride; r.dest.data=reinterpret_cast<PF_Pixel*>(output.data());
    r.in.inter.checkout_param=GetParam; r.in.inter.checkin_param=PutParam;
    PF_PreRenderCallbacks preCallbacks{}; preCallbacks.checkout_layer=GetLayer;
    PF_PreRenderInput preInput{}; preInput.bitdepth=static_cast<short>(depth);
    // A small requested tile must be expanded to full input for nonlocal neural processing.
    preInput.output_request.rect={30,20,50,40};
    PF_PreRenderOutput preOutput{};
    PF_PreRenderExtra pre{&preInput,&preOutput,&preCallbacks};
    require(effect(PF_Cmd_SMART_PRE_RENDER,&r.in,&r.out,nullptr,nullptr,&pre)==0,"Smart pre-render failed");
    require(r.paramCheckouts==ValueCount && r.paramCheckins==ValueCount,"Parameters leaked");
    require((preOutput.flags&PF_RenderOutputFlag_RETURNS_EXTRA_PIXELS)!=0,"Full frame output not requested");
    PF_SmartRenderInput smartInput{};
    smartInput.bitdepth=static_cast<short>(depth); smartInput.pre_render_data=preOutput.pre_render_data;
    PF_SmartRenderCallbacks callbacks{};
    callbacks.checkout_layer_pixels=GetPixels; callbacks.checkout_output=GetOutput; callbacks.checkin_layer_pixels=PutPixels;
    PF_SmartRenderExtra extra{&smartInput,&callbacks};
    const PF_Err err=effect(PF_Cmd_SMART_RENDER,&r.in,&r.out,nullptr,nullptr,&extra);
    preOutput.delete_pre_render_data_func(preOutput.pre_render_data);
    if (err) { std::fprintf(stderr,"%s\n",r.out.return_msg); throw std::runtime_error("Neural SmartFX render failed"); }
    require(r.layerCheckins==1,"Neural layer checkout leaked");
    size_t changed=0;
    for(int y=0;y<r.height;++y) {
        const auto* src=reinterpret_cast<const Pixel*>(input.data()+y*srcStride);
        const auto* dst=reinterpret_cast<const Pixel*>(output.data()+y*dstStride);
        for(int x=0;x<r.width;++x) {
            require(src[x].alpha==dst[x].alpha,"Alpha changed");
            if (x==0) require(src[x].red==dst[x].red,"Hidden RGB changed");
            require(std::isfinite(static_cast<float>(dst[x].red)),"Non-finite neural output");
            changed += std::abs(static_cast<float>(src[x].red)-static_cast<float>(dst[x].red)) > max/255.0F;
        }
        for(int x=r.width*sizeof(Pixel);x<dstStride;++x) require(output[y*dstStride+x]==0xcd,"Padding overwritten");
    }
    require(changed>100,"Neural output indistinguishable from source");
    // A split comparison must retain its entire original half; exercise Premiere's legacy route.
    if(depth!=32) {
        r.controls[2].u.pd.value=2;
        require(effect(PF_Cmd_RENDER,&r.in,&r.out,r.params,&r.dest,nullptr)==0,"Legacy neural render failed");
        for(int y=0;y<r.height;++y)
            require(!std::memcmp(input.data()+y*srcStride,output.data()+y*dstStride,r.width/2*sizeof(Pixel)),"Split view changed original half");
    }
    std::printf("PASS %dx%d %d-bit neural pixels: %zu changed red channels, preserved alpha/padding; SmartFX and comparison\n",width,height,depth,changed);
}
static void ControlSweep(MainFn effect,const char* captureDir) {
    constexpr int w=181,h=225;
    std::vector<PF_Pixel> source(w*h);
    for(int y=0;y<h;++y)for(int x=0;x<w;++x) {
        auto& p=source[y*w+x];p.alpha=255;
        p.red=A_u_char(x*255/(w-1));p.green=A_u_char(y*255/(h-1));
        p.blue=A_u_char(((x/8+y/8)&1)?204:51);
    }
    auto write=[&](const std::vector<PF_Pixel>& pixels,const char* name) {
        if(!captureDir)return;
        std::filesystem::create_directories(captureDir);
        std::ofstream f(std::filesystem::path(captureDir)/(std::string(name)+".ppm"),std::ios::binary);
        f<<"P6\n"<<w<<" "<<h<<"\n255\n";
        for(auto p:pixels) {const char rgb[3]={char(p.red),char(p.green),char(p.blue)};f.write(rgb,3);}
        require(bool(f),"Control capture could not be written");
    };
    auto run=[&](int id,float value,int second=0,float secondValue=0) {
        Request r(w,h);std::vector<PF_Pixel> output(source.size());
        auto set=[&](int i,float v) {
            if(!i)return;
            switch(specs[i].kind) {
                case Kind::Popup:r.controls[i].u.pd.value=int(v);break;
                case Kind::Integer:r.controls[i].u.sd.value=int(v);break;
                case Kind::Checkbox:r.controls[i].u.bd.value=PF_Boolean(v);break;
                case Kind::Float:r.controls[i].u.fs_d.value=v;break;
                default:break;
            }
        };
        set(id,value);set(second,secondValue);
        auto& layer=r.controls[0].u.ld;
        layer.width=w;layer.height=h;layer.rowbytes=w*sizeof(PF_Pixel);layer.data=source.data();
        r.dest=layer;r.dest.data=output.data();
        if(effect(PF_Cmd_RENDER,&r.in,&r.out,r.params,&r.dest,nullptr)!=0)
            throw std::runtime_error(r.out.return_msg);
        for(size_t i=0;i<output.size();++i)require(output[i].alpha==source[i].alpha,"Control changed alpha");
        return output;
    };
    auto delta=[](const std::vector<PF_Pixel>& a,const std::vector<PF_Pixel>& b) {
        double d=0;for(size_t i=0;i<a.size();++i)d+=std::abs(int(a[i].red)-b[i].red)+
            std::abs(int(a[i].green)-b[i].green)+std::abs(int(a[i].blue)-b[i].blue);
        return d/(a.size()*3);
    };
    write(source,"source");
    const auto base=run(Style,1);write(base,"style-default");
    auto natural=run(Style,2);auto cinematic=run(Style,3);
    require(delta(base,natural)>1&&delta(base,cinematic)>1&&delta(natural,cinematic)>1,"Neural styles still produce identical output");
    require(delta(base,run(Style,1))==0,"Returning to original style changes deterministic output");
    write(natural,"style-natural");write(cinematic,"style-cinematic");
    const auto weak=run(Intensity,50),strong=run(Intensity,150);
    require(delta(strong,source)>delta(base,source)*1.15&&delta(weak,source)<delta(base,source),"Intensity range does not respond");
    write(strong,"intensity-150");
    std::vector<std::vector<PF_Pixel>> looks;
    for(int look=2;look<=9;++look) {
        auto result=run(Look,float(look));
        require(delta(base,result)>.1,"Footage recipe unchanged from default");
        for(const auto& previous:looks)require(delta(previous,result)>.1,"Two footage presets are identical");
        looks.push_back(result);
        const std::string name="look-"+std::to_string(look);write(result,name.c_str());
        std::printf("PASS footage preset %d mean difference %.3f levels\n",look,delta(base,result));
    }
    for(auto item:std::vector<std::pair<int,float>>{{Tone,0.F},{Structure,0.F},{AutoMask,1.F},{ColorHold,100.F},
        {ExposureHold,100.F},{Highlights,100.F},{Shadows,100.F},{Texture,100.F},{Detail,80.F},{ArtifactGuard,100.F},
        {Saturation,0.F},{Warmth,75.F},{Tint,75.F},{Exposure,1.F}}) {
        require(delta(base,run(item.first,item.second))>.01,"Exposed control has no pixel effect");
        std::printf("PASS pixel response: %s\n",specs[item.first].name);
    }
    require(delta(run(Radius,1,Detail,80),run(Radius,4,Detail,80))>.01,"Detail radius ineffective");
    const auto wipe0=run(Wipe,0,View,2),wipe100=run(Wipe,100,View,2);
    require(delta(wipe0,base)==0&&delta(wipe100,source)==0,"Wipe endpoints not exact");
    const auto selected=run(Region,2);require(delta(base,selected)>.1,"Region selection ineffective");
    require(selected[0].red==source[0].red&&selected[0].green==source[0].green,"Outside selection changed original pixels");
    const auto matte=run(View,4,Region,2);write(matte,"effect-matte");
    require(matte[0].red==0&&matte[(h/2)*w+w/2].red==255,"Effect matte endpoints incorrect");
    const auto start=std::chrono::steady_clock::now();
    for(int i=0;i<30;++i) {auto frame=run(Style,float(i%3+1));require(!frame.empty(),"Style stress render failed");}
    std::printf("PASS styles, all eight recipes, intensity, 16 control responses, comparison/mask endpoints, and 30 repeated style switches in %.2f s\n",
        std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count());
}
int main(int argc,char** argv) {
    std::setvbuf(stdout,nullptr,_IONBF,0);
    try {
        require(argc==2||argc==3,"Expected plugin path and optional capture directory");
        HMODULE module=LoadLibraryExA(argv[1],nullptr,LOAD_WITH_ALTERED_SEARCH_PATH);
        require(module!=nullptr,"Effect DLL could not load");
        auto effect=reinterpret_cast<MainFn>(GetProcAddress(module,"EffectMain"));
        require(effect!=nullptr,"No effect entry point");
        PF_InData in{}; PF_OutData out{};
        require(effect(PF_Cmd_GLOBAL_SETUP,&in,&out,nullptr,nullptr,nullptr)==0,"Setup failed");
        require(GetModuleHandleW(L"nvngx_dlssnr.dll")==nullptr,"Neural runtime loaded during plugin scan");
        Frame<PF_Pixel>(effect,8,255); Frame<PF_Pixel16>(effect,16,32768); Frame<PF_PixelFloat>(effect,32,1);
        Frame<PF_Pixel>(effect,8,255,180,225);
        Frame<PF_Pixel16>(effect,16,32768,720,900);
        Frame<PF_PixelFloat>(effect,32,1,181,225);
        std::puts("STAGE sequence teardown and lazy reinitialization");
        require(effect(PF_Cmd_SEQUENCE_SETDOWN,&in,&out,nullptr,nullptr,nullptr)==0,"Sequence teardown failed");
        Frame<PF_Pixel>(effect,8,255);
        std::atomic<int> errors{0};
        std::thread one([&]{try{Frame<PF_Pixel>(effect,8,255);}catch(const std::exception& e){++errors;std::fprintf(stderr,"%s\n",e.what());}});
        std::thread two([&]{try{Frame<PF_PixelFloat>(effect,32,1);}catch(const std::exception& e){++errors;std::fprintf(stderr,"%s\n",e.what());}});
        one.join(); two.join();
        require(errors.load()==0,"Concurrent neural effect requests failed");
        ControlSweep(effect,argc==3?argv[2]:nullptr);
        std::puts("STAGE shared-engine teardown");
        require(effect(PF_Cmd_GLOBAL_SETDOWN,&in,&out,nullptr,nullptr,nullptr)==0,"Global shutdown failed");
        FreeLibrary(module);
        std::puts("PASS real Adobe adapter GPU evaluation and normal shared-session shutdown");
        return 0;
    }catch(const std::exception& e){std::fprintf(stderr,"FAIL: %s\n",e.what());return 1;}
}
