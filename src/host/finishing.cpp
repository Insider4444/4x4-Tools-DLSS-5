#include "finishing.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>
namespace adobe_dlss5 {
namespace {
float finite(float v) {return std::isfinite(v)?v:0.0F;}
float pct(float v) {return std::clamp(finite(v)*0.01F,0.0F,1.0F);}
float luma(const float* p) {return finite(0.2126F*p[0]+0.7152F*p[1]+0.0722F*p[2]);}
float smooth(float a,float b,float x) {
    const float t=std::clamp((x-a)/std::max(1.e-6F,b-a),0.0F,1.0F);
    return t*t*(3-2*t);
}
// Separable fractional box filter: bounded memory and linear work per frame.
// Edge replication and fractional endpoints keep the radius slider continuous.
void blur(const std::vector<float>& in,std::vector<float>& out,int w,int h,float radius) {
    const float r=std::clamp(radius,0.0F,256.0F);
    const int k=int(r); const double fraction=r-k, divisor=2*k+1+2*fraction;
    std::vector<float> temp(in.size()); out.resize(in.size());
    for(int y=0;y<h;++y) {
        const auto at=[&](int x){return in[size_t(y)*w+std::clamp(x,0,w-1)];};
        double sum=0;for(int d=-k;d<=k;++d)sum+=at(d);
        for(int x=0;x<w;++x) {
            temp[size_t(y)*w+x]=float((sum+fraction*(at(x-k-1)+at(x+k+1)))/divisor);
            sum+=double(at(x+k+1))-at(x-k);
        }
    }
    // Traverse whole rows, not columns: large images otherwise thrash the CPU cache.
    std::vector<double> sums(w,0);
    for(int d=-k;d<=k;++d) {
        const auto* row=temp.data()+size_t(std::clamp(d,0,h-1))*w;
        for(int x=0;x<w;++x)sums[x]+=row[x];
    }
    for(int y=0;y<h;++y) {
        const auto* upper=temp.data()+size_t(std::clamp(y-k-1,0,h-1))*w;
        const auto* lower=temp.data()+size_t(std::clamp(y+k+1,0,h-1))*w;
        const auto* leaving=temp.data()+size_t(std::clamp(y-k,0,h-1))*w;
        auto* row=out.data()+size_t(y)*w;
        for(int x=0;x<w;++x) {
            row[x]=float((sums[x]+fraction*(upper[x]+lower[x]))/divisor);
            sums[x]+=double(lower[x])-leaving[x];
        }
    }
}
float selection(int x,int y,int w,int h,const Settings& s) {
    if(s.region<1.5F)return 1;
    const float dx=((x+0.5F)/w-s.centerX*0.01F)/std::max(.005F,s.regionWidth*.005F);
    const float dy=((y+0.5F)/h-s.centerY*0.01F)/std::max(.005F,s.regionHeight*.005F);
    const float inside=1-smooth(1-pct(s.feather),1,std::sqrt(dx*dx+dy*dy));
    return s.region<2.5F?inside:1-inside;
}
float expose(float v,float gain,int encoding) {
    if(encoding==2)return v*gain;
    const float lin=v<=.04045F?v/12.92F:std::pow((v+.055F)/1.055F,2.4F);
    const float exposed=lin*gain;
    return exposed<=.0031308F?exposed*12.92F:1.055F*std::pow(exposed,1/2.4F)-.055F;
}
}
void finishFrame(const std::vector<float>& source,std::vector<float>& neural,int w,int h,const Settings& s) {
    const size_t n=size_t(w)*h;
    if(w<=0||h<=0||source.size()!=n*4||neural.size()!=source.size())
        throw std::invalid_argument("Invalid finishing frame buffers");
    // Retain the original fast path and exact runtime pixels when no finish is requested.
    if(s.mode==2&&s.strength>0&&s.strength<=100&&s.mix==100&&s.view!=4&&s.region<1.5F&&
        s.colorHold==0&&s.exposureHold==0&&s.highlights==0&&s.shadows==0&&s.texture==0&&
        s.detail==0&&s.artifactGuard==0&&s.saturation==100&&s.warmth==0&&s.tint==0&&s.exposure==0)return;
    const float color=pct(s.colorHold),lighting=pct(s.exposureHold),texture=pct(s.texture);
    const float detail=std::clamp(finite(s.detail)*.01F,-1.0F,1.0F),guard=pct(s.artifactGuard);
    const float boost=std::clamp(s.strength*.01F,1.0F,2.0F);
    const bool needFine=texture>0||detail!=0||guard>0;
    std::vector<float> srcY, neuralY, lowSrc, lowNeural;
    if(lighting>0||needFine) {srcY.resize(n);neuralY.resize(n);}
    for(size_t p=0;p<n;++p) {
        const size_t i=p*4;
        const float ys=luma(&source[i]);
        for(int c=0;c<3;++c) {
            const float original=finite(source[i+c]);
            neural[i+c]=boost==1?finite(neural[i+c]):original+(finite(neural[i+c])-original)*boost;
        }
        const float yn=luma(&neural[i]);
        if(color>0)for(int c=0;c<3;++c)
            neural[i+c]+=color*((finite(source[i+c])-ys)-(neural[i+c]-yn));
        if(!srcY.empty()) {srcY[p]=ys;neuralY[p]=yn;}
    }
    if(lighting>0) {
        const float radius=std::max(1.0F,std::min(w,h)*.025F);
        blur(srcY,lowSrc,w,h,radius);blur(neuralY,lowNeural,w,h,radius);
        for(size_t p=0;p<n;++p) {
            const float correction=lighting*(lowSrc[p]-lowNeural[p]);
            for(int c=0;c<3;++c)neural[p*4+c]+=correction;
            neuralY[p]+=correction;
        }
    }
    if(needFine) {
        const float radius=std::clamp(s.radius*s.renderScale,.1F,5.0F);
        blur(srcY,lowSrc,w,h,radius);blur(neuralY,lowNeural,w,h,radius);
    }
    const float exposureGain=std::exp2(std::clamp(finite(s.exposure),-4.0F,4.0F));
    const float warm=std::clamp(finite(s.warmth)*.01F,-1.0F,1.0F);
    const float tint=std::clamp(finite(s.tint)*.01F,-1.0F,1.0F);
    const float gains[3]={std::exp2(.3F*warm+.1F*tint),std::exp2(-.15F*tint),std::exp2(-.3F*warm+.1F*tint)};
    const float saturation=std::clamp(finite(s.saturation)*.01F,0.0F,2.0F);
    for(int y=0;y<h;++y)for(int x=0;x<w;++x) {
        const size_t p=size_t(y)*w+x,i=p*4;
        const float ys=luma(&source[i]);
        float fineAdjustment=0;
        if(needFine) {
            const float originalDetail=srcY[p]-lowSrc[p],neuralDetail=neuralY[p]-lowNeural[p];
            fineAdjustment=texture*(originalDetail-neuralDetail);
            const float restoredDetail=neuralDetail+fineAdjustment;
            // Positive detail avoids boosting near-flat noise and limits ringing.
            fineAdjustment+=detail<0?detail*restoredDetail:
                detail*std::clamp(restoredDetail,-.08F,.08F)*smooth(.003F,.025F,std::abs(originalDetail));
        }
        for(int c=0;c<3;++c) {
            neural[i+c]=(neural[i+c]+fineAdjustment)*gains[c];
            if(s.exposure!=0)neural[i+c]=expose(neural[i+c],exposureGain,s.encoding);
        }
        const float yn=luma(&neural[i]);
        for(int c=0;c<3;++c)neural[i+c]=yn+(neural[i+c]-yn)*saturation;
        // Protection blends back original pixels, including after color finishing.
        float weight=pct(float(s.mix))*selection(x,y,w,h,s);
        if(s.strength<=0||s.mode!=2)weight=0;
        weight*=1-pct(s.highlights)*smooth(.7F,.98F,ys);
        weight*=1-pct(s.shadows)*(1-smooth(.02F,.2F,ys));
        if(guard>0) {
            float delta=0;for(int c=0;c<3;++c)delta=std::max(delta,std::abs(neural[i+c]-finite(source[i+c])));
            const float limit=.025F+2.5F*std::abs(srcY[p]-lowSrc[p]);
            weight*=1-guard*(1-std::min(1.0F,limit/std::max(delta,1.e-6F)));
        }
        for(int c=0;c<3;++c) {
            const float original=finite(source[i+c]);
            const float value=weight==1?neural[i+c]:original+(neural[i+c]-original)*weight;
            neural[i+c]=s.view==4?weight:(std::isfinite(value)?value:original);
        }
        neural[i+3]=source[i+3];
    }
}
}
