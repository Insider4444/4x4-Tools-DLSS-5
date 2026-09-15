#pragma once
#include "PIFilter.h"
#include "tiled_image.h"
#include <windows.h>
#include <functional>

namespace photoshop_dlss5 {
struct FilterSettings {
    unsigned magic=0x34445053,version=1;
    adobe_dlss5::Settings image;
    int tileCore=1024;
};
using Preview = std::function<bool(const FilterSettings&,float,float,
    std::vector<float>&,std::vector<float>&,int&,int&,std::string&)>;
bool showDialog(HWND owner,FilterSettings& settings,ImageSize size,int depth,const Preview& preview);
inline unsigned descriptorKey(int index) {return 0x78303030U+unsigned(index/10)*256U+unsigned(index%10);}
}
