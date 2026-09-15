#include "filter.h"
#include "parameters.h"
#include "update_notice.h"
#include <commctrl.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <stdexcept>
#include <vector>

using namespace adobe_dlss5;
namespace photoshop_dlss5 {
namespace {
struct Dialog {
    FilterSettings* settings;
    ImageSize size;int depth;
    const Preview* preview;
    std::vector<HWND> controls;
    std::vector<int> indices;
    std::vector<float> original,enhanced;
    std::vector<unsigned char> bitmap;
    int width=0,height=0,page=0;
    bool populating=false;
};
const std::vector<std::vector<int>> pages={
    {Mode,Intensity,Style,Mix,Tone,Structure,AutoMask},
    {ColorHold,ExposureHold,Highlights,Shadows,Texture,Detail,Radius,ArtifactGuard},
    {Saturation,Warmth,Tint,Exposure},
    {View,Wipe,Region,CenterX,CenterY,RegionWidth,RegionHeight,Feather}
};
void choices(HWND box,const char* options) {
    std::string text(options);std::size_t start=0;
    do {const auto end=text.find('|',start);const auto part=text.substr(start,end-start);
        SendMessageA(box,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(part.c_str()));
        if(end==std::string::npos)break;start=end+1;
    }while(true);
}
HWND add(HWND parent,const char* klass,const char* text,DWORD style,int id,int x,int y,int w,int h) {
    RECT rect{x,y,x+w,y+h};MapDialogRect(parent,&rect);
    auto control=CreateWindowExA(0,klass,text,WS_CHILD|WS_VISIBLE|style,rect.left,rect.top,
        rect.right-rect.left,rect.bottom-rect.top,parent,reinterpret_cast<HMENU>(std::intptr_t(id)),GetModuleHandleW(nullptr),nullptr);
    SendMessageW(control,WM_SETFONT,SendMessageW(parent,WM_GETFONT,0,0),TRUE);return control;
}
void populate(HWND hwnd,Dialog& d) {
    d.populating=true;
    for(auto item:d.controls)DestroyWindow(item);d.controls.clear();d.indices.clear();
    int row=0;
    for(const int index:pages[d.page]) {
        const auto& spec=specs[index];const int y=76+row*25;
        const std::string name=index==Look?"Image preset":spec.name;
        d.controls.push_back(add(hwnd,"STATIC",name.c_str(),0,0,370,y+3,169,14));
        HWND control=nullptr;
        if(spec.kind==Kind::Popup) {
            control=add(hwnd,"COMBOBOX","",CBS_DROPDOWNLIST|WS_VSCROLL|WS_TABSTOP,1000+index,537,y,128,160);
            choices(control,spec.choices);SendMessageW(control,CB_SETCURSEL,int(getSetting(d.settings->image,index))-1,0);
        }else if(spec.kind==Kind::Checkbox) {
            control=add(hwnd,"BUTTON","Enabled",BS_AUTOCHECKBOX|WS_TABSTOP,1000+index,537,y,124,17);
            SendMessageW(control,BM_SETCHECK,getSetting(d.settings->image,index)>=.5F?BST_CHECKED:BST_UNCHECKED,0);
        }else {
            char value[32]{};std::snprintf(value,sizeof(value),"%.2f",getSetting(d.settings->image,index));
            control=add(hwnd,"EDIT",value,WS_BORDER|WS_TABSTOP|ES_AUTOHSCROLL,1000+index,537,y,58,18);
            char range[48]{};std::snprintf(range,sizeof(range),"%.0f to %.0f",spec.low,spec.high);
            d.controls.push_back(add(hwnd,"STATIC",range,0,0,600,y+3,67,14));
        }
        d.controls.push_back(control);d.indices.push_back(index);++row;
    }
    SendDlgItemMessageW(hwnd,118,CB_SETCURSEL,d.settings->image.look-1,0);
    d.populating=false;
}
bool readControls(HWND hwnd,Dialog& d) {
    for(int index:d.indices) {
        const auto& spec=specs[index];float value=0;
        if(spec.kind==Kind::Popup)value=float(SendDlgItemMessageW(hwnd,1000+index,CB_GETCURSEL,0,0)+1);
        else if(spec.kind==Kind::Checkbox)value=SendDlgItemMessageW(hwnd,1000+index,BM_GETCHECK,0,0)==BST_CHECKED?1.0F:0.0F;
        else {
            char text[80]{};GetDlgItemTextA(hwnd,1000+index,text,sizeof(text));char* end=nullptr;value=std::strtof(text,&end);
            while(end&&*end==' ')++end;
            if(end==text||(end&&*end)||!std::isfinite(value)||value<spec.low||value>spec.high) {
                const std::string message=std::string(spec.name)+" must be between "+std::to_string(spec.low)+" and "+std::to_string(spec.high)+".";
                MessageBoxA(hwnd,message.c_str(),"4x4Tools DLSS5",MB_OK|MB_ICONINFORMATION);SetFocus(GetDlgItem(hwnd,1000+index));return false;
            }
        }
        setSetting(d.settings->image,index,value);
    }
    const auto quality=SendDlgItemMessageW(hwnd,119,CB_GETCURSEL,0,0);
    d.settings->tileCore=quality==0?512:quality==2?1536:1024;
    return true;
}
void draw(HWND hwnd,Dialog& d,HDC dc,const RECT& r) {
    FillRect(dc,&r,static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH)));
    if(d.enhanced.empty())return;
    const auto& pixels=SendDlgItemMessageW(hwnd,116,BM_GETCHECK,0,0)==BST_CHECKED?d.original:d.enhanced;
    d.bitmap.resize(std::size_t(d.width)*d.height*4);
    for(std::size_t i=0;i<pixels.size();i+=4)for(int c=0;c<3;++c) {
        float value=pixels[i+c];
        if(d.depth==32) {value=std::max(0.0F,value);value=value/(1+value);value=value<=.0031308F?12.92F*value:1.055F*std::pow(value,1/2.4F)-.055F;}
        d.bitmap[i+2-c]=static_cast<unsigned char>(std::lround(std::clamp(value,0.0F,1.0F)*255));
    }
    BITMAPINFO info{};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);info.bmiHeader.biWidth=d.width;
    info.bmiHeader.biHeight=-d.height;info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;info.bmiHeader.biCompression=BI_RGB;
    // Display one image pixel per screen pixel whenever the pane can fit it.
    const int side=std::min({int(r.right),int(r.bottom),d.width});
    SetStretchBltMode(dc,HALFTONE);
    StretchDIBits(dc,(r.right-side)/2,(r.bottom-side)/2,side,side,0,0,d.width,d.height,d.bitmap.data(),&info,DIB_RGB_COLORS,SRCCOPY);
}
void refreshPreview(HWND hwnd) {
    const auto pane=GetDlgItem(hwnd,111);InvalidateRect(pane,nullptr,FALSE);UpdateWindow(pane);
}
INT_PTR CALLBACK dialogProc(HWND hwnd,UINT message,WPARAM wp,LPARAM lp) {
    auto* d=reinterpret_cast<Dialog*>(GetWindowLongPtrW(hwnd,DWLP_USER));
    if(message==WM_INITDIALOG) {
        d=reinterpret_cast<Dialog*>(lp);SetWindowLongPtrW(hwnd,DWLP_USER,lp);
        const wchar_t* names[]={L"Neural",L"Restore",L"Color",L"Compare"};
        for(int i=0;i<4;++i) {TCITEMW item{};item.mask=TCIF_TEXT;item.pszText=const_cast<wchar_t*>(names[i]);
            SendDlgItemMessageW(hwnd,117,TCM_INSERTITEMW,i,reinterpret_cast<LPARAM>(&item));}
        choices(GetDlgItem(hwnd,118),"Manual|Natural balance|Portrait / skin|Sports / action|Product / fabric|Landscape / daylight|Low light / gentle|Animation / game|Strong enhancement");
        choices(GetDlgItem(hwnd,119),"Low VRAM|Balanced|Larger context");
        SendDlgItemMessageW(hwnd,119,CB_SETCURSEL,d->settings->tileCore<=512?0:d->settings->tileCore>=1536?2:1,0);
        SetDlgItemTextW(hwnd,113,L"50");SetDlgItemTextW(hwnd,114,L"50");
        char info[160]{};std::snprintf(info,sizeof(info),"%s   |   %d x %d   |   RGB %d-bit","4x4Tools DLSS5",d->size.width,d->size.height,d->depth);
        SetDlgItemTextA(hwnd,110,info);populate(hwnd,*d);
        updates::requestCheck();SetTimer(hwnd,1,2000,nullptr);
        const auto status=updates::statusText()+" | Same dimensions and transparency.";
        SetDlgItemTextA(hwnd,120,status.c_str());return TRUE;
    }
    if(!d)return FALSE;
    if(message==WM_TIMER) {
        const auto status=updates::statusText()+" | Same dimensions and transparency.";
        SetDlgItemTextA(hwnd,120,status.c_str());return TRUE;
    }
    if(message==WM_DESTROY) {KillTimer(hwnd,1);return TRUE;}
    if(message==WM_NOTIFY&&reinterpret_cast<NMHDR*>(lp)->idFrom==117) {
        if(reinterpret_cast<NMHDR*>(lp)->code==TCN_SELCHANGING) {
            const bool valid=readControls(hwnd,*d);SetWindowLongPtrW(hwnd,DWLP_MSGRESULT,valid?FALSE:TRUE);return TRUE;
        }
        if(reinterpret_cast<NMHDR*>(lp)->code==TCN_SELCHANGE) {d->page=int(SendDlgItemMessageW(hwnd,117,TCM_GETCURSEL,0,0));populate(hwnd,*d);return TRUE;}
    }
    if(message==WM_DRAWITEM&&wp==111) {
        const auto* item=reinterpret_cast<DRAWITEMSTRUCT*>(lp);
        draw(hwnd,*d,item->hDC,item->rcItem);return TRUE;
    }
    if(message==WM_COMMAND) {
        const int id=LOWORD(wp),notification=HIWORD(wp);
        if(id==IDCANCEL) {EndDialog(hwnd,IDCANCEL);return TRUE;}
        if(id==IDOK) {if(readControls(hwnd,*d))EndDialog(hwnd,IDOK);return TRUE;}
        if(id==116) {refreshPreview(hwnd);return TRUE;}
        if(id==121) {
            auto menu=CreatePopupMenu();
            AppendMenuW(menu,MF_STRING,1,L"Check for updates");
            AppendMenuW(menu,MF_STRING,2,L"Open release downloads");
            AppendMenuW(menu,MF_STRING|(updates::enabled()?MF_CHECKED:0),3,L"Automatic update checks");
            RECT rect{};GetWindowRect(GetDlgItem(hwnd,121),&rect);
            const auto action=TrackPopupMenu(menu,TPM_RETURNCMD|TPM_NONOTIFY,rect.left,rect.bottom,0,hwnd,nullptr);
            DestroyMenu(menu);
            if(action==1)updates::requestCheck(true);
            else if(action==2)updates::openReleasePage();
            else if(action==3)updates::setEnabled(!updates::enabled());
            return TRUE;
        }
        if(id==118&&notification==CBN_SELCHANGE&&!d->populating) {
            const int look=int(SendDlgItemMessageW(hwnd,118,CB_GETCURSEL,0,0))+1;
            if(look>1) {const int encoding=d->settings->image.encoding;d->settings->image=lookRecipe(look);d->settings->image.encoding=encoding;}
            d->settings->image.look=look;populate(hwnd,*d);SetDlgItemTextW(hwnd,112,L"Settings changed. Click Preview to refresh the crop.");return TRUE;
        }
        if(id>=1000&&!d->populating&&(notification==EN_CHANGE||notification==CBN_SELCHANGE||notification==BN_CLICKED)) {
            if(specs[id-1000].inLook) {d->settings->image.look=1;SendDlgItemMessageW(hwnd,118,CB_SETCURSEL,0,0);}
            SetDlgItemTextW(hwnd,112,L"Settings changed. Click Preview to refresh the crop.");
        }
        if(id==115) {
            if(!readControls(hwnd,*d))return TRUE;
            float centers[2]{};
            for(int i=0;i<2;++i) {char text[64]{};GetDlgItemTextA(hwnd,113+i,text,sizeof(text));char* end=nullptr;centers[i]=std::strtof(text,&end);
                if(end==text||*end||!std::isfinite(centers[i])||centers[i]<0||centers[i]>100) {MessageBoxW(hwnd,L"Crop center must be between 0 and 100 percent.",L"4x4Tools DLSS5",MB_OK);return TRUE;}}
            SetDlgItemTextW(hwnd,112,L"Rendering preview...");UpdateWindow(hwnd);std::string error;
            try {
                if((*d->preview)(*d->settings,centers[0],centers[1],d->original,d->enhanced,d->width,d->height,error)) {
                    SetDlgItemTextW(hwnd,112,L"512 x 512 source-pixel crop. Toggle Original to compare.");refreshPreview(hwnd);
                }else {SetDlgItemTextW(hwnd,112,L"Preview failed. Adjust settings or check GPU support.");MessageBoxA(hwnd,error.c_str(),"4x4Tools DLSS5",MB_OK|MB_ICONERROR);}
            }catch(const std::exception& e) {MessageBoxA(hwnd,e.what(),"4x4Tools DLSS5",MB_OK|MB_ICONERROR);}
            return TRUE;
        }
    }
    if(message==WM_CLOSE) {EndDialog(hwnd,IDCANCEL);return TRUE;}
    return FALSE;
}
}
bool showDialog(HWND owner,FilterSettings& settings,ImageSize size,int depth,const Preview& preview) {
    INITCOMMONCONTROLSEX common{sizeof(common),ICC_TAB_CLASSES};InitCommonControlsEx(&common);
    HMODULE module=nullptr;
    GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        reinterpret_cast<LPCWSTR>(&showDialog),&module);
    Dialog data{&settings,size,depth,&preview};
    const auto result=DialogBoxParamW(module,MAKEINTRESOURCEW(101),owner,dialogProc,reinterpret_cast<LPARAM>(&data));
    if(result==-1)throw std::runtime_error("Could not open the filter controls.");
    return result==IDOK;
}
}
