// Manual hardware characterization: compare real runtime controls with a fixed frame.
#include "Feature18Runtime.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>
int main() {
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    constexpr int w=640,h=360,stride=w*4*sizeof(float);
    std::vector<float> input(w*h*4),output(input.size()),baseline;
    for(int y=0;y<h;++y) for(int x=0;x<w;++x) {
        const int i=(y*w+x)*4;
        input[i]=float(x*255/(w-1))/255;
        input[i+1]=float(y*255/(h-1))/255;
        input[i+2]=((x/16+y/16)&1)?0.8F:0.2F;
        input[i+3]=1;
    }
    resolve_dlss5::Feature18Runtime runtime;
    resolve_dlss5::Feature18Settings defaults;
    defaults.inputEncoding=resolve_dlss5::InputEncoding::SdrSrgb;
    auto run=[&](const char* label, resolve_dlss5::Feature18Settings s) {
        if(!runtime.process(input.data(),stride,output.data(),stride,w,h,s,true)) {
            std::fprintf(stderr,"FAIL %s: %s\n",label,runtime.lastError().c_str()); return false;
        }
        if(baseline.empty()) baseline=output;
        double mae=0; size_t changed=0; float maxDelta=0;
        for(size_t i=0;i<output.size();++i) if(i%4!=3) {
            if(!std::isfinite(output[i])) return false;
            const float d=std::abs(output[i]-baseline[i]);
            mae+=d; changed+=d>1.0F/255; maxDelta=std::max(maxDelta,d);
        }
        std::printf("{\"control\":\"%s\",\"mae_levels\":%.6f,\"changed_channels\":%zu,\"max_levels\":%.4f}\n",
            label,mae*255/(w*h*3),changed,maxDelta*255);
        return true;
    };
    if(!run("baseline",defaults)) return 1;
    for(int p=2;p<=3;++p) { auto s=defaults;s.preset=static_cast<resolve_dlss5::NrPreset>(p);
        if(!run(p==2?"hint2":"hint3",s)) return 1; }
    for(int v=1;v<=2;++v) { auto s=defaults;s.style=v;if(!run(v==1?"style1":"style2",s)) return 1; }
    auto s=defaults;s.intensity=0.5F;if(!run("intensity50",s))return 1;
    s=defaults;s.intensity=1.5F;if(!run("intensity150",s))return 1;
    s=defaults;s.localToneStrength=0;if(!run("tone0",s))return 1;
    s=defaults;s.localToneStrength=2;if(!run("tone200",s))return 1;
    s=defaults;s.localStructureStrength=0;if(!run("structure0",s))return 1;
    s=defaults;s.localStructureStrength=2;if(!run("structure200",s))return 1;
    s=defaults;s.skinStructureStrength=0;if(!run("skin0",s))return 1;
    s=defaults;s.skinStructureStrength=2;if(!run("skin200",s))return 1;
    s=defaults;s.useAutoMask=true;if(!run("autoMask",s))return 1;
    runtime.reset();
    return 0;
}
