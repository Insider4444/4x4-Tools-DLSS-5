// Network I/O lives in this short-lived process, never on an Adobe render thread.
#include <windows.h>
#include <winhttp.h>
#include <fstream>
#include <iostream>
#include "version.h"
#include "update_storage.h"
using namespace adobe_dlss5::updates;
struct Internet {HINTERNET value=nullptr;~Internet(){if(value)WinHttpCloseHandle(value);}};
static std::string fetch(){
    const auto deadline=GetTickCount64()+12000;
    Internet session{WinHttpOpen(L"4x4Tools-DLSS5-win/" L"" TOOLS_PRODUCT_VERSION,WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,WINHTTP_NO_PROXY_NAME,WINHTTP_NO_PROXY_BYPASS,0)};
    if(!session.value)return {};
    WinHttpSetTimeouts(session.value,2500,2500,2500,2500);
    Internet connection{WinHttpConnect(session.value,L"api.github.com",INTERNET_DEFAULT_HTTPS_PORT,0)};
    if(!connection.value)return {};
    const std::wstring path=L"/repos/" TOOLS_REPOSITORY_W L"/releases/latest";
    Internet request{WinHttpOpenRequest(connection.value,L"GET",path.c_str(),nullptr,WINHTTP_NO_REFERER,WINHTTP_DEFAULT_ACCEPT_TYPES,WINHTTP_FLAG_SECURE)};
    if(!request.value)return {};
    DWORD redirects=WINHTTP_OPTION_REDIRECT_POLICY_NEVER,auth=WINHTTP_AUTOLOGON_SECURITY_LEVEL_HIGH;
    WinHttpSetOption(request.value,WINHTTP_OPTION_REDIRECT_POLICY,&redirects,sizeof(redirects));
    WinHttpSetOption(request.value,WINHTTP_OPTION_AUTOLOGON_POLICY,&auth,sizeof(auth));
    const wchar_t* headers=L"Accept: application/vnd.github+json\r\nX-GitHub-Api-Version: 2022-11-28\r\n";
    if(!WinHttpSendRequest(request.value,headers,DWORD(-1),WINHTTP_NO_REQUEST_DATA,0,0,0)||!WinHttpReceiveResponse(request.value,nullptr))return {};
    DWORD status=0,size=sizeof(status);
    if(!WinHttpQueryHeaders(request.value,WINHTTP_QUERY_STATUS_CODE|WINHTTP_QUERY_FLAG_NUMBER,WINHTTP_HEADER_NAME_BY_INDEX,&status,&size,WINHTTP_NO_HEADER_INDEX)||status!=200)return {};
    std::string body;char buffer[4096];DWORD received=0;
    while(GetTickCount64()<deadline){if(!WinHttpReadData(request.value,buffer,sizeof(buffer),&received))return {};
        if(!received)return body;if(body.size()+received>131072)return {};body.append(buffer,received);}
    return {};
}
int wmain(int argc,wchar_t** argv){
    SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX);
    try{
        const bool force=argc==2&&std::wstring(argv[1])==L"--force";
        const bool fixture=argc==3&&std::wstring(argv[1])==L"--fixture";
        if(argc>1&&!force&&!fixture)return 2;
        HANDLE mutex=CreateMutexW(nullptr,FALSE,L"Local\\4x4Tools.UpdateCheck");if(!mutex)return 1;
        const auto lock=WaitForSingleObject(mutex,0);
        if(lock!=WAIT_OBJECT_0&&lock!=WAIT_ABANDONED){CloseHandle(mutex);return 0;}
        struct Unlock {HANDLE h;~Unlock(){ReleaseMutex(h);CloseHandle(h);}} unlock{mutex};
        auto cache=readCache();const auto now=static_cast<long long>(std::time(nullptr));
        if(!force&&!fixture&&(!automaticEnabled()||(cache.checked>0&&now-cache.checked<86400)))return 0;
        std::string json;
        if(fixture){const std::filesystem::path file(argv[2]);if(std::filesystem::file_size(file)>131072)return 2;
            std::ifstream stream(file,std::ios::binary);json.assign(std::istreambuf_iterator<char>(stream),{});}
        else json=fetch();
        Version latest;const bool valid=ReleaseJson(json).stableVersion(latest);
        if(valid)cache.version=latest.text();
        cache.checked=now;writeCache(cache);
        if(fixture){std::cout<<(valid?"accepted ":"ignored ")<<cache.version<<'\n';return valid?0:3;}
        return 0; // Offline, rate-limited and failed requests stay silent.
    }catch(...){return 1;}
}
