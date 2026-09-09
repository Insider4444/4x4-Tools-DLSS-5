#include "update_notice.h"
#include "update_storage.h"
#include "version.h"
#include <shellapi.h>
#include <atomic>
namespace adobe_dlss5::updates {
static std::atomic<long long> lastLaunch{0};
std::string statusText() noexcept {
    try{const auto cache=readCache();Version latest,current;
        if(Version::parse(cache.version,latest)&&Version::parse(TOOLS_PRODUCT_VERSION,current)&&latest>current)
            return "Update "+latest.text()+" available";
    }catch(...){}
    return "v" TOOLS_PRODUCT_VERSION;
}
bool enabled() noexcept {try{return automaticEnabled();}catch(...){return false;}}
void setEnabled(bool value) noexcept {try{setAutomatic(value);}catch(...){}}
void requestCheck(bool force) noexcept {
    try{
        if(GetEnvironmentVariableW(L"TOOLS_DISABLE_UPDATE_CHECK",nullptr,0)>0)return;
        const auto now=static_cast<long long>(std::time(nullptr));
        if(now-lastLaunch.load()<15||(!force&&!enabled()))return;
        const auto cache=readCache();if(!force&&cache.checked>0&&now-cache.checked<86400)return;
        lastLaunch.store(now);
        HMODULE module=nullptr;
        if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
            reinterpret_cast<LPCWSTR>(&requestCheck),&module))return;
        wchar_t path[32768]{};if(!GetModuleFileNameW(module,path,32768))return;
        const auto helper=std::filesystem::path(path).parent_path()/L"UpdateCheck.exe";
        if(!std::filesystem::exists(helper))return;
        std::wstring args=L"\""+helper.wstring()+L"\""+(force?L" --force":L"");
        STARTUPINFOW start{};start.cb=sizeof(start);start.dwFlags=STARTF_USESHOWWINDOW;start.wShowWindow=SW_HIDE;
        PROCESS_INFORMATION process{};
        if(CreateProcessW(helper.c_str(),args.data(),nullptr,nullptr,FALSE,CREATE_NO_WINDOW,nullptr,helper.parent_path().c_str(),&start,&process)){
            CloseHandle(process.hThread);CloseHandle(process.hProcess);
        }
    }catch(...){}
}
void openReleasePage() noexcept {ShellExecuteW(nullptr,L"open",TOOLS_RELEASE_URL_W,nullptr,nullptr,SW_SHOWNORMAL);}
}
