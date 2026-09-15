#pragma once
#include "tiled_image.h"
#include <windows.h>
#include <objbase.h>
#include <stdexcept>
#include <vector>

namespace photoshop_dlss5 {
// A private, delete-on-close float RGB file. No host output is touched until
// every neural tile has succeeded. Checked 64-bit offsets support PSB images.
class ScratchImage {
    HANDLE file_=INVALID_HANDLE_VALUE;
    ImageSize size_;
    void seek(std::uint64_t position) {
        LARGE_INTEGER p;p.QuadPart=static_cast<LONGLONG>(position);
        if(!SetFilePointerEx(file_,p,nullptr,FILE_BEGIN))throw std::runtime_error("Could not seek the image scratch file.");
    }
public:
    explicit ScratchImage(ImageSize size):size_(size) {
        wchar_t temp[MAX_PATH+1]{};GUID id{};
        if(!GetTempPathW(MAX_PATH,temp)||FAILED(CoCreateGuid(&id)))throw std::runtime_error("Could not create a private scratch path.");
        wchar_t guid[48]{};StringFromGUID2(id,guid,48);
        const std::wstring path=std::wstring(temp)+L"4x4Tools-Photoshop-"+guid+L".tmp";
        ULARGE_INTEGER free{};const auto bytes=std::uint64_t(size.width)*size.height*3*sizeof(float);
        if(!GetDiskFreeSpaceExW(temp,&free,nullptr,nullptr)||free.QuadPart<bytes+256ULL*1024*1024)
            throw std::runtime_error("Not enough free space on the temporary drive to stage this full-resolution image.");
        file_=CreateFileW(path.c_str(),GENERIC_READ|GENERIC_WRITE,0,nullptr,CREATE_NEW,
            FILE_ATTRIBUTE_TEMPORARY|FILE_FLAG_DELETE_ON_CLOSE,nullptr);
        if(file_==INVALID_HANDLE_VALUE)throw std::runtime_error("Could not create the image scratch file.");
        try {seek(bytes);if(!SetEndOfFile(file_))throw std::runtime_error("Could not reserve image scratch space.");}
        catch(...) {CloseHandle(file_);file_=INVALID_HANDLE_VALUE;throw;}
    }
    ScratchImage(const ScratchImage&)=delete;
    ~ScratchImage(){if(file_!=INVALID_HANDLE_VALUE)CloseHandle(file_);}
    void write(ImageRect rect,const float* rgba,std::size_t stride) {
        std::vector<float> row(std::size_t(rect.width)*3);
        for(int y=0;y<rect.height;++y) {
            for(int x=0;x<rect.width;++x)std::copy_n(rgba+std::size_t(y)*stride+x*4,3,row.data()+x*3);
            seek((std::uint64_t(rect.y+y)*size_.width+rect.x)*3*sizeof(float));
            DWORD done=0;const auto bytes=DWORD(row.size()*sizeof(float));
            if(!WriteFile(file_,row.data(),bytes,&done,nullptr)||done!=bytes)
                throw std::runtime_error("Could not stage the image. Check free space on the temporary drive.");
        }
    }
    void read(ImageRect rect,std::vector<float>& rgb) {
        rgb.resize(std::size_t(rect.width)*rect.height*3);
        const auto bytes=DWORD(std::size_t(rect.width)*3*sizeof(float));
        for(int y=0;y<rect.height;++y) {
            seek((std::uint64_t(rect.y+y)*size_.width+rect.x)*3*sizeof(float));
            DWORD done=0;
            if(!ReadFile(file_,rgb.data()+std::size_t(y)*rect.width*3,bytes,&done,nullptr)||done!=bytes)
                throw std::runtime_error("Could not read the staged image.");
        }
    }
};
}
