#pragma once
#include "neural_bridge.h"
#include <algorithm>
#include <cmath>
namespace adobe_dlss5 {
// Indices and disk IDs 1..7 retain the 1.1.x parameter types and meaning,
// except the formerly ineffective preset hint now selects the working Style.
enum Param { Input, Mode, View, Intensity, Style, Mix, Encoding, Processing, Look,
    NeuralGroup, Tone, Structure, AutoMask, NeuralEnd,
    RestoreGroup, ColorHold, ExposureHold, Highlights, Shadows, Texture, Detail,
    Radius, ArtifactGuard, RestoreEnd, FinishGroup, Saturation, Warmth, Tint,
    Exposure, FinishEnd, CompareGroup, Wipe, Region, CenterX, CenterY,
    RegionWidth, RegionHeight, Feather, CompareEnd, ParamCount };
enum class Kind { Input, Popup, Integer, Float, Checkbox, Begin, End };
struct ParamSpec {
    Kind kind; const char* name; float low=0, high=0, initial=0;
    const char* choices=""; float Settings::* field=nullptr; bool inLook=false;
};
inline constexpr ParamSpec specs[ParamCount] = {
    {Kind::Input,"Input"},
    {Kind::Popup,"DLSS enhancement",1,2,2,"Bypass|Neural (experimental)"},
    {Kind::Popup,"Output",1,4,1,"Processed|Original / neural wipe|Difference x10|Effect matte"},
    {Kind::Integer,"Neural intensity",0,200,100,"",nullptr,true},
    {Kind::Popup,"Neural style",1,3,1,"Default|Natural|Cinematic",nullptr,true},
    {Kind::Integer,"Mix",0,100,100,"",nullptr,true},
    {Kind::Popup,"Input color",1,2,1,"SDR|Linear HDR (experimental)"},
    {Kind::Popup,"Processing",1,1,1,"Same size; no frame generation"},
    {Kind::Popup,"Footage preset",1,9,1,"Manual|Natural balance|Portrait / skin|Sports / action|Product / fabric|Landscape / daylight|Low light / gentle|Animation / game|Strong enhancement"},
    {Kind::Begin,"Neural rendering"},
    {Kind::Float,"Neural tone",0,200,100,"",&Settings::tone,true},
    {Kind::Float,"Neural structure",0,200,100,"",&Settings::structure,true},
    {Kind::Checkbox,"Automatic neural mask",0,1,0,"",&Settings::autoMask,true},
    {Kind::End,""},
    {Kind::Begin,"Natural restoration"},
    {Kind::Float,"Preserve original color",0,100,0,"",&Settings::colorHold,true},
    {Kind::Float,"Preserve original lighting",0,100,0,"",&Settings::exposureHold,true},
    {Kind::Float,"Protect highlights",0,100,0,"",&Settings::highlights,true},
    {Kind::Float,"Protect shadows",0,100,0,"",&Settings::shadows,true},
    {Kind::Float,"Recover original texture",0,100,0,"",&Settings::texture,true},
    {Kind::Float,"Detail: soft / crisp",-100,100,0,"",&Settings::detail,true},
    {Kind::Float,"Detail radius (pixels)",0.5F,5,1.5F,"",&Settings::radius,true},
    {Kind::Float,"Artifact protection",0,100,0,"",&Settings::artifactGuard,true},
    {Kind::End,""},
    {Kind::Begin,"Color finishing"},
    {Kind::Float,"Saturation",0,200,100,"",&Settings::saturation,true},
    {Kind::Float,"Warmth",-100,100,0,"",&Settings::warmth,true},
    {Kind::Float,"Tint",-100,100,0,"",&Settings::tint,true},
    {Kind::Float,"Exposure (stops)",-4,4,0,"",&Settings::exposure,true},
    {Kind::End,""},
    {Kind::Begin,"Compare and selective blend"},
    {Kind::Float,"Wipe position",0,100,50,"",&Settings::wipe},
    {Kind::Popup,"Apply enhancement",1,3,1,"Whole frame|Inside ellipse|Outside ellipse",&Settings::region},
    {Kind::Float,"Region center X",0,100,50,"",&Settings::centerX},
    {Kind::Float,"Region center Y",0,100,50,"",&Settings::centerY},
    {Kind::Float,"Region width",1,200,65,"",&Settings::regionWidth},
    {Kind::Float,"Region height",1,200,75,"",&Settings::regionHeight},
    {Kind::Float,"Region feather",0,100,30,"",&Settings::feather},
    {Kind::End,""}
};
inline bool isValue(int i) {
    return i>0 && i<ParamCount && i!=Processing &&
        specs[i].kind!=Kind::Begin && specs[i].kind!=Kind::End;
}
inline constexpr int ValueCount = 29;
inline void setSetting(Settings& s,int i,float v) {
    const auto& p=specs[i];
    v=std::isfinite(v)?std::clamp(v,p.low,p.high):p.initial;
    if(p.field) {s.*(p.field)=v;return;}
    switch(i) {
        case Mode:s.mode=int(v);break; case View:s.view=int(v);break;
        case Intensity:s.strength=int(v);break; case Style:s.preset=int(v);break;
        case Mix:s.mix=int(v);break; case Encoding:s.encoding=int(v);break;
        case Look:s.look=int(v);break; default:break;
    }
}
inline float getSetting(const Settings& s,int i) {
    if(specs[i].field)return s.*(specs[i].field);
    switch(i) {
        case Mode:return float(s.mode); case View:return float(s.view);
        case Intensity:return float(s.strength); case Style:return float(s.preset);
        case Mix:return float(s.mix); case Encoding:return float(s.encoding);
        case Look:return float(s.look); default:return specs[i].initial;
    }
}
inline Settings lookRecipe(int look) {
    Settings s;
    switch(look) {
        case 2: s.strength=75;s.tone=65;s.structure=100;s.colorHold=70;s.exposureHold=55;
            s.highlights=65;s.shadows=25;s.texture=20;s.artifactGuard=25;break;
        case 3: s.preset=2;s.strength=60;s.tone=45;s.structure=75;s.autoMask=1;
            s.colorHold=85;s.exposureHold=70;s.highlights=80;s.shadows=35;s.texture=40;s.artifactGuard=50;break;
        case 4: s.strength=85;s.tone=70;s.structure=125;s.colorHold=65;s.exposureHold=45;
            s.highlights=65;s.shadows=20;s.texture=30;s.detail=15;s.artifactGuard=35;break;
        case 5: s.strength=80;s.tone=55;s.structure=120;s.colorHold=90;s.exposureHold=55;
            s.highlights=80;s.texture=40;s.detail=10;s.artifactGuard=30;break;
        case 6: s.preset=2;s.strength=85;s.tone=85;s.structure=115;s.colorHold=65;
            s.exposureHold=40;s.highlights=80;s.shadows=20;s.texture=15;s.detail=8;s.artifactGuard=20;break;
        case 7: s.strength=55;s.tone=50;s.structure=65;s.autoMask=1;s.colorHold=80;
            s.exposureHold=70;s.highlights=65;s.shadows=85;s.artifactGuard=75;break;
        case 8: s.preset=3;s.strength=90;s.tone=95;s.structure=120;s.colorHold=55;
            s.exposureHold=25;s.highlights=50;s.texture=10;s.artifactGuard=30;break;
        case 9: s.preset=3;s.strength=125;s.tone=120;s.structure=140;s.colorHold=40;
            s.exposureHold=20;s.highlights=50;s.shadows=15;s.texture=10;s.artifactGuard=25;break;
        default:break;
    }
    return s;
}
inline void resolveLook(Settings& s) {
    if(s.look<=1)return;
    const auto recipe=lookRecipe(s.look);
    for(int i=1;i<ParamCount;++i)if(specs[i].inLook)setSetting(s,i,getSetting(recipe,i));
}
}
