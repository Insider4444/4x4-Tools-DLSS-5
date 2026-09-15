#pragma once
#include "lcms2.h"
#include <string>
#include <vector>
namespace photoshop_dlss5 {
class ColorPipeline {
    cmsHPROFILE source_=nullptr,working_=nullptr;
    cmsHTRANSFORM forward_=nullptr,reverse_=nullptr;
public:
    ColorPipeline()=default;
    ColorPipeline(const ColorPipeline&)=delete;
    ~ColorPipeline();
    bool initialize(const void* profile,unsigned bytes,bool linear,std::string& error);
    void toWorking(std::vector<float>& pixels) const;
    void toDocument(std::vector<float>& pixels) const;
};
}
