#pragma once
#include <windows.h>
#include <filesystem>
#include <fstream>
#include <ctime>
#include "update_policy.h"
namespace adobe_dlss5::updates {
inline std::filesystem::path cacheFolder() {
    wchar_t path[32768]{};const auto n=GetEnvironmentVariableW(L"LOCALAPPDATA",path,32768);
    if(!n||n>=32768)return {};return std::filesystem::path(path)/L"4x4Tools"/L"Updates";
}
struct Cache {long long checked=0;std::string version;};
inline Cache readCache() {
    Cache result;const auto folder=cacheFolder();if(folder.empty())return result;
    std::error_code ec;const auto file=folder/L"state.txt";
    if(std::filesystem::file_size(file,ec)>128||ec)return result;
    std::ifstream stream(file);std::string signature;std::getline(stream,signature);
    if(signature!="4x4Tools-update-v1")return {};
    stream>>result.checked>>result.version;
    const auto now=static_cast<long long>(std::time(nullptr));
    if(result.checked<0||result.checked>now+300)return {};
    Version version;if(!result.version.empty()&&!Version::parse(result.version,version))result.version.clear();
    return result;
}
inline bool automaticEnabled(){const auto path=cacheFolder();return !path.empty()&&!std::filesystem::exists(path/L"disabled");}
inline void setAutomatic(bool enabled){const auto path=cacheFolder();if(path.empty())return;std::filesystem::create_directories(path);
    if(enabled){std::error_code ignored;std::filesystem::remove(path/L"disabled",ignored);}else{std::ofstream file(path/L"disabled");file<<"Automatic checks disabled by the user.\n";}}
inline void writeCache(const Cache& cache){const auto folder=cacheFolder();if(folder.empty())return;std::filesystem::create_directories(folder);
    const auto temporary=folder/(L"state-"+std::to_wstring(GetCurrentProcessId())+L".tmp");
    {std::ofstream stream(temporary);stream<<"4x4Tools-update-v1\n"<<cache.checked<<'\n'<<cache.version<<'\n';if(!stream)throw std::runtime_error("Update cache write failed");}
    if(!MoveFileExW(temporary.c_str(),(folder/L"state.txt").c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)){
        std::error_code ignored;std::filesystem::remove(temporary,ignored);
    }
}
}
