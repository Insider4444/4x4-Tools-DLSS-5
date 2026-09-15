#include "color_pipeline.h"
namespace photoshop_dlss5 {
ColorPipeline::~ColorPipeline() {
    if(forward_)cmsDeleteTransform(forward_);if(reverse_)cmsDeleteTransform(reverse_);
    if(source_)cmsCloseProfile(source_);if(working_)cmsCloseProfile(working_);
}
bool ColorPipeline::initialize(const void* profile,unsigned bytes,bool linear,std::string& error) {
    source_=profile&&bytes?cmsOpenProfileFromMem(profile,bytes):cmsCreate_sRGBProfile();
    working_=cmsCreate_sRGBProfile();
    if(!source_||!working_||cmsGetColorSpace(source_)!=cmsSigRgbData) {
        error="The document's RGB color profile could not be opened.";return false;
    }
    if(linear) {
        // Photoshop's 32-bit RGB pixels are scene-linear; keep the document's
        // primaries and white point but use linear transfer curves for the transform.
        if(!cmsIsMatrixShaper(source_)) {error="32-bit processing requires a matrix RGB color profile.";return false;}
        auto* gamma=cmsBuildGamma(nullptr,1.0);
        if(!gamma) {error="Could not create a linear color transform.";return false;}
        bool ok=true;
        for(const auto tag:{cmsSigRedTRCTag,cmsSigGreenTRCTag,cmsSigBlueTRCTag}) {
            ok=cmsWriteTag(source_,tag,gamma)&&ok;ok=cmsWriteTag(working_,tag,gamma)&&ok;
        }
        cmsFreeToneCurve(gamma);
        if(!ok) {error="Could not linearize the document color profile.";return false;}
    }
    constexpr auto flags=cmsFLAGS_COPY_ALPHA|cmsFLAGS_NOCACHE|cmsFLAGS_NOOPTIMIZE;
    forward_=cmsCreateTransform(source_,TYPE_RGBA_FLT,working_,TYPE_RGBA_FLT,INTENT_RELATIVE_COLORIMETRIC,flags);
    reverse_=cmsCreateTransform(working_,TYPE_RGBA_FLT,source_,TYPE_RGBA_FLT,INTENT_RELATIVE_COLORIMETRIC,flags);
    if(!forward_||!reverse_) {error="Could not create the document RGB color transforms.";return false;}
    return true;
}
void ColorPipeline::toWorking(std::vector<float>& pixels) const {
    cmsDoTransform(forward_,pixels.data(),pixels.data(),cmsUInt32Number(pixels.size()/4));
}
void ColorPipeline::toDocument(std::vector<float>& pixels) const {
    cmsDoTransform(reverse_,pixels.data(),pixels.data(),cmsUInt32Number(pixels.size()/4));
}
}
