// Genuine Adobe SDK adapter. NGX starts only when a neural frame is requested.
#include "AEConfig.h"
#include "AE_Effect.h"
#include "AE_EffectCB.h"
#include "AE_EffectCBSuites.h"
#include "AE_Macros.h"
#include "Param_Utils.h"
#include "entry.h"
#include "SPBasic.h"
#include "effect_metadata.h"
#include "neural_bridge.h"
#include "parameters.h"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <memory>
#include <new>
using adobe_dlss5::Settings;
using namespace adobe_dlss5;
static_assert(TOOLS_VERSION == PF_VERSION(1, 3, 0, PF_Stage_RELEASE, 0));
static_assert(TOOLS_FLAGS == PF_OutFlag_DEEP_COLOR_AWARE);
// Frame state is local; one shared GPU session is serialized and reset per request.
static_assert(TOOLS_FLAGS2 == (PF_OutFlag2_SUPPORTS_SMART_RENDER | PF_OutFlag2_FLOAT_COLOR_AWARE |
    PF_OutFlag2_SUPPORTS_THREADED_RENDERING | PF_OutFlag2_PARAM_GROUP_START_COLLAPSED_FLAG));

static PF_Err Report(PF_OutData* out, const char* message) {
    if (out) {
        std::snprintf(out->return_msg, sizeof(out->return_msg), "4x4Tools: %.220s", message);
        out->out_flags |= PF_OutFlag_DISPLAY_ERROR_MESSAGE;
    }
    return PF_Err_INTERNAL_STRUCT_DAMAGED;
}
static PF_Err SetupParams(PF_InData* in_data, PF_OutData* out_data) {
    if (!in_data || !out_data || !in_data->inter.add_param) return PF_Err_BAD_CALLBACK_PARAM;
    for(int i=1;i<ParamCount;++i) {
        const auto& p=specs[i]; PF_ParamDef def{};
        if(p.inLook||i==Look)def.flags=PF_ParamFlag_SUPERVISE;
        if(i==Look)def.flags|=PF_ParamFlag_CANNOT_TIME_VARY;
        if(i==Processing) {def.flags=PF_ParamFlag_CANNOT_TIME_VARY;def.ui_flags=PF_PUI_DISABLED;}
        const PF_ParamFlags parameterFlags=def.flags;
        switch(p.kind) {
            case Kind::Popup: PF_ADD_POPUP(p.name,A_short(p.high),A_short(p.initial),p.choices,i);break;
            case Kind::Integer: PF_ADD_SLIDER(p.name,int(p.low),int(p.high),int(p.low),int(p.high),int(p.initial),i);break;
            case Kind::Float:
                PF_ADD_FLOAT_SLIDERX(p.name,p.low,p.high,p.low,p.high,p.initial,
                    i==Exposure||i==Radius?2:1,0,parameterFlags,i);break;
            case Kind::Checkbox: PF_ADD_CHECKBOX(p.name,"",int(p.initial),0,i);break;
            case Kind::Begin:
                if(i==FinishGroup||i==CompareGroup)def.flags=PF_ParamFlag_START_COLLAPSED;
                PF_ADD_TOPIC(p.name,i);break;
            case Kind::End: PF_END_TOPIC(i);break;
            default:return PF_Err_BAD_CALLBACK_PARAM;
        }
    }
    out_data->num_params = ParamCount;
    return PF_Err_NONE;
}
static void ReadSetting(Settings& s, int index, const PF_ParamDef& p) {
    switch(specs[index].kind) {
        case Kind::Popup:setSetting(s,index,float(p.u.pd.value));break;
        case Kind::Integer:setSetting(s,index,float(p.u.sd.value));break;
        case Kind::Float:setSetting(s,index,float(p.u.fs_d.value));break;
        case Kind::Checkbox:setSetting(s,index,float(p.u.bd.value));break;
        default:break;
    }
}
static void WriteSetting(PF_ParamDef& p,int i,float value) {
    switch(specs[i].kind) {
        case Kind::Popup:p.u.pd.value=A_long(value);break;
        case Kind::Integer:p.u.sd.value=A_long(value);break;
        case Kind::Float:p.u.fs_d.value=value;break;
        case Kind::Checkbox:p.u.bd.value=PF_Boolean(value>=.5F);break;
        default:return;
    }
    p.uu.change_flags|=PF_ChangeFlag_CHANGED_VALUE;
}
static PF_Err UserChanged(PF_OutData* out,PF_ParamDef* params[],PF_UserChangedParamExtra* extra) {
    if(!out||!params||!extra||!params[Look])return PF_Err_BAD_CALLBACK_PARAM;
    const int changed=extra->param_index;
    if(changed==Look && params[Look]->u.pd.value>1) {
        const auto recipe=lookRecipe(params[Look]->u.pd.value);
        for(int i=1;i<ParamCount;++i)if(specs[i].inLook&&!params[i])return PF_Err_BAD_CALLBACK_PARAM;
        for(int i=1;i<ParamCount;++i)if(specs[i].inLook)WriteSetting(*params[i],i,getSetting(recipe,i));
        out->out_flags|=PF_OutFlag_FORCE_RERENDER;
    } else if(changed>0 && changed<ParamCount && specs[changed].inLook) {
        WriteSetting(*params[Look],Look,1); // Editing a recipe becomes an explicit manual look.
    }
    return PF_Err_NONE;
}
static PF_Err ReadSettings(PF_InData* in, PF_ParamDef* params[], Settings& s) {
    if(!in)return PF_Err_BAD_CALLBACK_PARAM;
    for (int i = 1; i < ParamCount; ++i) {
        if(!isValue(i))continue;
        if (params) {
            if (!params[i]) return PF_Err_BAD_CALLBACK_PARAM;
            ReadSetting(s, i, *params[i]);
        } else {
            if (!in->inter.checkout_param || !in->inter.checkin_param) return PF_Err_BAD_CALLBACK_PARAM;
            PF_ParamDef p{};
            PF_Err err = PF_CHECKOUT_PARAM(in, i, in->current_time, in->time_step, in->time_scale, &p);
            if (err) return err;
            ReadSetting(s, i, p);
            err = PF_CHECKIN_PARAM(in, &p);
            if (err) return err;
        }
    }
    resolveLook(s); // Also honor presets set through scripting rather than a UI gesture.
    if(in->downsample_x.den>0 && in->downsample_x.num>0)
        s.renderScale=std::clamp(float(in->downsample_x.num)/in->downsample_x.den,.01F,1.0F);
    return PF_Err_NONE;
}
static bool IsBypass(const Settings& s) { return s.mode != 2 || (s.view!=4 && (s.mix<=0||s.strength<=0)); }
static PF_Err CopyWorld(PF_InData* in, PF_EffectWorld* src, PF_EffectWorld* dst, bool smart) {
    if (!smart) {
        if (!in->utils || !in->utils->copy) return PF_Err_BAD_CALLBACK_PARAM;
        return in->utils->copy(in->effect_ref, src, dst, nullptr, nullptr);
    }
    if (!in->pica_basicP) return PF_Err_BAD_CALLBACK_PARAM;
    const PF_WorldTransformSuite1* suite = nullptr;
    if (in->pica_basicP->AcquireSuite(kPFWorldTransformSuite, kPFWorldTransformSuiteVersion1,
        reinterpret_cast<const void**>(&suite)) || !suite) return PF_Err_BAD_CALLBACK_PARAM;
    const PF_Err err = suite->copy ? suite->copy(in->effect_ref, src, dst, nullptr, nullptr) : PF_Err_BAD_CALLBACK_PARAM;
    in->pica_basicP->ReleaseSuite(kPFWorldTransformSuite, kPFWorldTransformSuiteVersion1);
    return err;
}
template<class Pixel> static PF_Err NeuralPixels(PF_EffectWorld* src, PF_EffectWorld* dst,
    PF_OutData* out, const Settings& settings, float maximum) {
    const int width = src->width, height = src->height;
    if (width != dst->width || height != dst->height)
        return Report(out, "Input/output bounds differ. Render the full layer for this experimental effect.");
    if (width <= 0 || height <= 0) return PF_Err_NONE;
    if (!src->data || !dst->data || width > 8192 || height > 8192 ||
        std::abs(static_cast<long long>(src->rowbytes)) < static_cast<long long>(width) * static_cast<long long>(sizeof(Pixel)) ||
        std::abs(static_cast<long long>(dst->rowbytes)) < static_cast<long long>(width) * static_cast<long long>(sizeof(Pixel)))
        return Report(out, "Unsupported frame size or image buffer.");
    std::vector<float> input(static_cast<size_t>(width) * height * 4), output(input.size());
    for (int y = 0; y < height; ++y) {
        const auto* row = reinterpret_cast<const Pixel*>(reinterpret_cast<const char*>(src->data) +
            static_cast<ptrdiff_t>(y) * src->rowbytes);
        for (int x = 0; x < width; ++x) {
            const auto& p = row[x];
            const size_t i = (static_cast<size_t>(y) * width + x) * 4;
            const float alpha = static_cast<float>(p.alpha) / maximum;
            const float divisor = alpha > 1.0e-6F ? maximum * alpha : maximum;
            input[i] = static_cast<float>(p.red) / divisor;
            input[i + 1] = static_cast<float>(p.green) / divisor;
            input[i + 2] = static_cast<float>(p.blue) / divisor;
            input[i + 3] = alpha;
        }
    }
    std::string error;
    if (!adobe_dlss5::processFrame(input, output, width, height, settings, error))
        return Report(out, error.c_str());
    for (int y = 0; y < height; ++y) {
        const auto* source = reinterpret_cast<const Pixel*>(reinterpret_cast<const char*>(src->data) +
            static_cast<ptrdiff_t>(y) * src->rowbytes);
        auto* dest = reinterpret_cast<Pixel*>(reinterpret_cast<char*>(dst->data) +
            static_cast<ptrdiff_t>(y) * dst->rowbytes);
        for (int x = 0; x < width; ++x) {
            const size_t i = (static_cast<size_t>(y) * width + x) * 4;
            const float alpha = input[i + 3];
            dest[x] = source[x]; // Preserve original alpha and hidden RGB exactly.
            if (alpha <= 1.0e-6F || (settings.view == 2 && x < int(width*settings.wipe*.01F))) continue;
            auto channel = [&](float original, float processed) {
                float value = processed * alpha * maximum;
                if (settings.view == 3) value = std::abs(value - original) * 10.0F;
                if (!std::isfinite(value)) value = original;
                if (maximum > 1.0F) value = std::floor(std::clamp(value, 0.0F, maximum) + 0.5F);
                return value;
            };
            dest[x].red = static_cast<decltype(dest[x].red)>(channel(static_cast<float>(source[x].red), output[i]));
            dest[x].green = static_cast<decltype(dest[x].green)>(channel(static_cast<float>(source[x].green), output[i + 1]));
            dest[x].blue = static_cast<decltype(dest[x].blue)>(channel(static_cast<float>(source[x].blue), output[i + 2]));
        }
    }
    return PF_Err_NONE;
}
static PF_Err Render(PF_InData* in, PF_OutData* out, PF_EffectWorld* src, PF_EffectWorld* dst,
    const Settings& s, int depth, bool smart) {
    if (!src || !dst) return PF_Err_BAD_CALLBACK_PARAM;
    if (IsBypass(s)) return CopyWorld(in, src, dst, smart);
    switch (depth) {
        case 8: return NeuralPixels<PF_Pixel>(src, dst, out, s, 255.0F);
        case 16: return NeuralPixels<PF_Pixel16>(src, dst, out, s, 32768.0F);
        case 32: return NeuralPixels<PF_PixelFloat>(src, dst, out, s, 1.0F);
        default: return Report(out, "Unsupported pixel format.");
    }
}
static void DeletePreRender(void* data) { delete static_cast<Settings*>(data); }
static PF_Err PreRender(PF_InData* in, PF_PreRenderExtra* extra) {
    if (!in || !extra || !extra->input || !extra->output || !extra->cb || !extra->cb->checkout_layer)
        return PF_Err_BAD_CALLBACK_PARAM;
    auto s = std::make_unique<Settings>();
    PF_Err err = ReadSettings(in, nullptr, *s);
    if (err) return err;
    PF_RenderRequest request = extra->input->output_request;
    request.preserve_rgb_of_zero_alpha = 1;
    if (!IsBypass(*s)) {
        request.rect = {0, 0, in->width, in->height};
        request.channel_mask = PF_ChannelMask_ARGB;
        extra->output->flags |= PF_RenderOutputFlag_RETURNS_EXTRA_PIXELS;
    }
    PF_CheckoutResult result{};
    err = extra->cb->checkout_layer(in->effect_ref, 0, 0, &request, in->current_time,
        in->time_step, in->time_scale, &result);
    if (err) return err;
    extra->output->result_rect = result.result_rect;
    extra->output->max_result_rect = result.max_result_rect;
    extra->output->solid = result.solid;
    extra->output->pre_render_data = s.release();
    extra->output->delete_pre_render_data_func = DeletePreRender;
    return PF_Err_NONE;
}
static PF_Err SmartRender(PF_InData* in, PF_OutData* out, PF_SmartRenderExtra* extra) {
    if (!in || !extra || !extra->input || !extra->input->pre_render_data || !extra->cb ||
        !extra->cb->checkout_layer_pixels || !extra->cb->checkout_output || !extra->cb->checkin_layer_pixels)
        return PF_Err_BAD_CALLBACK_PARAM;
    PF_EffectWorld* src = nullptr;
    PF_EffectWorld* dst = nullptr;
    PF_Err err = extra->cb->checkout_layer_pixels(in->effect_ref, 0, &src);
    if (err) return err;
    struct Checkin {
        PF_InData* in; PF_SmartRenderCallbacks* cb;
        ~Checkin() { if (cb) cb->checkin_layer_pixels(in->effect_ref, 0); }
    } cleanup{in, extra->cb};
    err = extra->cb->checkout_output(in->effect_ref, &dst);
    if (!err) err = Render(in, out, src, dst, *static_cast<Settings*>(extra->input->pre_render_data),
        extra->input->bitdepth, true);
    const PF_Err checkinErr = extra->cb->checkin_layer_pixels(in->effect_ref, 0);
    cleanup.cb = nullptr;
    return err ? err : checkinErr;
}
extern "C" DllExport PF_Err EffectMain(PF_Cmd cmd, PF_InData* in_data,
    PF_OutData* out_data, PF_ParamDef* params[], PF_LayerDef* output, void* extra) {
    try {
        switch (cmd) {
            case PF_Cmd_ABOUT:
                if (!out_data) return PF_Err_BAD_CALLBACK_PARAM;
                std::strcpy(out_data->return_msg, "4x4-Tools DLSS 5 v1.0\rExperimental neural video enhancement.\r"
                    "Three neural styles, eight footage recipes, natural restoration and selective blending.\r"
                    "Same resolution; RGBA8 proxy; history resets per frame.");
                break;
            case PF_Cmd_GLOBAL_SETUP:
                if (!out_data) return PF_Err_BAD_CALLBACK_PARAM;
                out_data->my_version = TOOLS_VERSION;
                out_data->out_flags = TOOLS_FLAGS;
                out_data->out_flags2 = TOOLS_FLAGS2;
                break;
            case PF_Cmd_SEQUENCE_SETDOWN:
            case PF_Cmd_GLOBAL_SETDOWN:
                // Release neural resources while the host is still fully operational.
                // Other live instances can lazily recreate this history-free session.
                adobe_dlss5::shutdownEngine();
                break;
            case PF_Cmd_PARAMS_SETUP: return SetupParams(in_data, out_data);
            case PF_Cmd_USER_CHANGED_PARAM:
                return UserChanged(out_data,params,static_cast<PF_UserChangedParamExtra*>(extra));
            case PF_Cmd_RENDER: {
                if (!in_data || !params || !params[0] || !output) return PF_Err_BAD_CALLBACK_PARAM;
                Settings s;
                const PF_Err err = ReadSettings(in_data, params, s);
                if (err) return err;
                return Render(in_data, out_data, &params[0]->u.ld, output, s,
                    PF_WORLD_IS_DEEP(&params[0]->u.ld) ? 16 : 8, false);
            }
            case PF_Cmd_SMART_PRE_RENDER: return PreRender(in_data, static_cast<PF_PreRenderExtra*>(extra));
            case PF_Cmd_SMART_RENDER: return SmartRender(in_data, out_data, static_cast<PF_SmartRenderExtra*>(extra));
            default: break;
        }
        return PF_Err_NONE;
    } catch (const std::bad_alloc&) { return PF_Err_OUT_OF_MEMORY; }
    catch (const std::exception& error) { return Report(out_data, error.what()); }
    catch (...) { return Report(out_data, "Unexpected neural processing failure."); }
}
extern "C" DllExport PF_Err PluginDataEntryFunction2(PF_PluginDataPtr inPtr,
    PF_PluginDataCB2 callback, SPBasicSuite*, const char*, const char*) {
    if (!callback) return PF_Err_BAD_CALLBACK_PARAM;
    PF_Err result = PF_Err_NONE;
    PF_REGISTER_EFFECT_EXT2(inPtr, callback, TOOLS_EFFECT_NAME, TOOLS_MATCH_NAME,
        TOOLS_CATEGORY, AE_RESERVED_INFO, "EffectMain", "");
    return result;
}
