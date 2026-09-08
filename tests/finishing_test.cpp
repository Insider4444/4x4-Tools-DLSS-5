#include "finishing.h"
#include "parameters.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <limits>
#include <stdexcept>
using namespace adobe_dlss5;
static void require(bool v,const char* message) {if(!v)throw std::runtime_error(message);}
static double distance(const std::vector<float>& a,const std::vector<float>& b) {
    double sum=0;for(size_t i=0;i<a.size();++i)if(i%4!=3)sum+=std::abs(a[i]-b[i]);return sum;
}
int main() {
    try {
        constexpr int w=181,h=225; const size_t n=size_t(w)*h;
        std::vector<float> source(n*4),raw(n*4);
        for(int y=0;y<h;++y)for(int x=0;x<w;++x) {
            const size_t i=(size_t(y)*w+x)*4;
            const float base=.1F+.65F*y/(h-1),pattern=((x/4+y/4)%2)*.08F;
            source[i]=base+pattern;source[i+1]=base*.8F;source[i+2]=base*.6F;
            source[i+3]=(x%3)*.5F;
            raw[i]=source[i]+.06F;raw[i+1]=source[i+1]+.04F;raw[i+2]=source[i+2]+.02F;raw[i+3]=source[i+3];
        }
        auto run=[&](Settings s){auto out=raw;finishFrame(source,out,w,h,s);return out;};
        const auto normal=run({});
        require(distance(raw,normal)<.002,"Neutral finishing changed neural output");
        Settings s;s.mix=0;require(distance(run(s),source)==0,"Zero mix must preserve source");
        s={};s.strength=0;require(distance(run(s),source)==0,"Zero intensity must preserve source");
        s={};s.strength=150;require(distance(run(s),source)>distance(normal,source)*1.45,"Above-100 intensity has no effect");
        s={};s.colorHold=100;auto color=run(s);
        for(size_t i=0;i<source.size();i+=4)require(std::abs((color[i]-color[i+1])-(source[i]-source[i+1]))<1.e-5F,"Color preservation lost chroma");
        s={};s.exposureHold=100;require(distance(run(s),source)<distance(normal,source)*.5,"Lighting preservation ineffective");
        s={};s.artifactGuard=100;require(distance(run(s),source)<distance(normal,source)*.8,"Artifact guard did not limit changes");
        s={};s.saturation=0;const auto mono=run(s);
        for(size_t i=0;i<mono.size();i+=4)require(std::abs(mono[i]-mono[i+1])<1.e-6F&&std::abs(mono[i]-mono[i+2])<1.e-6F,"Zero saturation not neutral");
        s={};s.warmth=100;require(run(s)[0]>normal[0]&&run(s)[2]<normal[2],"Warmth has wrong direction");
        s={};s.tint=100;require(run(s)[1]<normal[1],"Tint has no effect");
        s={};s.exposure=1;require(run(s)[0]>normal[0],"Exposure has no effect");
        s={};s.detail=70;const auto crisp=run(s);
        s.detail=-70;require(distance(crisp,run(s))>1,"Detail soft/crisp endpoints are identical");
        s.detail=70;s.radius=4;require(distance(crisp,run(s))>1,"Detail radius has no effect");
        s={};s.view=4;s.region=2;const auto inside=run(s);
        s.region=3;const auto outside=run(s);
        for(size_t i=0;i<inside.size();i+=4)require(std::abs(inside[i]+outside[i]-1)<1.e-6F,"Inside/outside mattes do not complement");
        require(inside[0]==0&&inside[(size_t(h/2)*w+w/2)*4]==1,"Ellipse selection incorrect");
        s={};s.mix=0;s.view=4;require(run(s)[0]==0,"Zero mix matte is not black");
        // Full protections restore black shadows and white highlights exactly.
        const auto saved=source;
        std::fill(source.begin(),source.end(),1.0F);s={};s.highlights=100;
        require(distance(run(s),source)==0,"White highlight not protected");
        std::fill(source.begin(),source.end(),0.0F);s={};s.shadows=100;
        require(distance(run(s),source)==0,"Black shadow not protected");source=saved;
        // Texture control restores source luminance detail removed from the neural image.
        auto smoothRaw=source;for(size_t i=0;i<smoothRaw.size();i+=4)for(int c=0;c<3;++c)smoothRaw[i+c]=.4F;
        s={};s.texture=100;auto restored=smoothRaw;finishFrame(source,restored,w,h,s);
        require(distance(restored,smoothRaw)>1,"Texture recovery has no effect");
        for(int look=2;look<=9;++look) {
            s={};s.look=look;resolveLook(s);const auto out=run(s);
            for(size_t i=0;i<out.size();++i)require(i%4==3?out[i]==source[i]:std::isfinite(out[i]),"Preset changed alpha or produced invalid pixels");
        }
        s={};setSetting(s,Tone,std::numeric_limits<float>::quiet_NaN());require(s.tone==100,"Non-finite control not sanitized");
        // Representative overhead for the most commonly useful restoration recipe.
        constexpr int bw=1920,bh=1080;std::vector<float> bigSource(size_t(bw)*bh*4,.3F),bigNeural(bigSource.size(),.4F);
        s=lookRecipe(2);const auto begin=std::chrono::steady_clock::now();
        finishFrame(bigSource,bigNeural,bw,bh,s);
        const double ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-begin).count();
        std::printf("PASS finishing: neutral/zero endpoints, intensity boost, chroma, lighting, protections, detail, texture, masks, alpha and finite presets; 1080p natural finishing %.1f ms\n",ms);
        return 0;
    }catch(const std::exception& e){std::fprintf(stderr,"FAIL: %s\n",e.what());return 1;}
}
