#include <windows.h>
#include "AEConfig.h"
#include "AE_Effect.h"
#include "AE_EffectCB.h"
#include "AE_EffectCBSuites.h"
#include "AE_EffectSuites.h"
#include "AE_PluginData.h"
#include "SPBasic.h"
#include "effect_metadata.h"
#include "neural_bridge.h"
#include "parameters.h"
#include "update_storage.h"
#include <algorithm>
using namespace adobe_dlss5;
static PF_ParamDef registered[ParamCount]{};
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <vector>
#include <atomic>
#include <thread>

using MainFn = PF_Err (*)(PF_Cmd, PF_InData*, PF_OutData*, PF_ParamDef*[], PF_LayerDef*, void*);
// Host request state must not itself race when testing the loaded effect.
static thread_local int added = 0, copied = 0, checkedIn = 0, pixelBytes = 4;
static thread_local PF_Err callbackError = 0;
static thread_local PF_LayerDef* sourceWorld = nullptr;
static thread_local PF_LayerDef* destWorld = nullptr;
static std::atomic<int> activeCopies{0}, maximumCopies{0};
static void require(bool ok, const char* text) { if (!ok) throw std::runtime_error(text); }
static A_Err Register(PF_PluginDataPtr, const A_u_char* name, const A_u_char* match,
    const A_u_char* category, const A_u_char* entry, A_long kind,
    A_long major, A_long minor, A_long reserved, const A_u_char*) {
    if (std::strcmp(reinterpret_cast<const char*>(name), TOOLS_EFFECT_NAME) ||
        std::strcmp(reinterpret_cast<const char*>(match), TOOLS_MATCH_NAME) ||
        std::strcmp(reinterpret_cast<const char*>(category), TOOLS_CATEGORY) ||
        std::strcmp(reinterpret_cast<const char*>(entry), "EffectMain") ||
        kind != 'eFKT' || major != PF_AE_PLUG_IN_VERSION || minor != PF_AE_PLUG_IN_SUBVERS || reserved != 8)
        return 999;
    return callbackError;
}
static PF_Err AddParam(PF_ProgPtr, PF_ParamIndex index, PF_ParamDef* def) {
    if (callbackError) return callbackError;
    if (index != -1 || def->uu.id != added+1 || def->uu.id >= ParamCount) return 999;
    registered[def->uu.id]=*def;
    ++added;
    return callbackError;
}
static PF_Err Copy(PF_ProgPtr, PF_EffectWorld* src, PF_EffectWorld* dst, PF_Rect*, PF_Rect*) {
    ++copied;
    if (callbackError) return callbackError;
    const int concurrent = ++activeCopies;
    int previous = maximumCopies.load();
    while (concurrent > previous && !maximumCopies.compare_exchange_weak(previous, concurrent)) {}
    std::this_thread::yield();
    for (int y = 0; y < src->height; ++y)
        std::memcpy(reinterpret_cast<char*>(dst->data) + y * dst->rowbytes,
            reinterpret_cast<char*>(src->data) + y * src->rowbytes, src->width * pixelBytes);
    --activeCopies;
    return PF_Err_NONE;
}
static PF_Err CheckoutInput(PF_ProgPtr, A_long, PF_EffectWorld** result) { *result = sourceWorld; return 0; }
static PF_Err CheckoutOutput(PF_ProgPtr, PF_EffectWorld** result) { *result = destWorld; return callbackError; }
static PF_Err Checkin(PF_ProgPtr, A_long) { ++checkedIn; return 0; }
static PF_WorldTransformSuite1 worldSuite{};
static PF_ParamUtilsSuite3 paramSuite{};
static PF_ParamDef lastStatus{};
static int uiUpdates=0;
static PF_Err UpdateParamUi(PF_ProgPtr, PF_ParamIndex index, const PF_ParamDef* def) {
    if(index!=UpdateStatus)return 999;
    lastStatus=*def;++uiUpdates;return 0;
}
static SPErr Acquire(const char* name, int32 version, const void** result) {
    if(!std::strcmp(name,kPFParamUtilsSuite)&&version==kPFParamUtilsSuiteVersion3){*result=&paramSuite;return 0;}
    if (std::strcmp(name, kPFWorldTransformSuite) || version != kPFWorldTransformSuiteVersion1) return 999;
    *result = &worldSuite;
    return 0;
}
static SPErr Release(const char*, int32) { return 0; }

static void ConcurrentFrames(MainFn effect, int worker) {
    PF_InData in{};
    PF_OutData out{};
    PF_UtilCallbacks utils{};
    utils.copy = Copy;
    in.utils = &utils;
    SPBasicSuite basic{};
    basic.AcquireSuite = Acquire;
    basic.ReleaseSuite = Release;
    in.pica_basicP = &basic;
    PF_SmartRenderCallbacks callbacks{};
    callbacks.checkout_layer_pixels = CheckoutInput;
    callbacks.checkout_output = CheckoutOutput;
    callbacks.checkin_layer_pixels = Checkin;
    PF_SmartRenderExtra extra{};
    extra.cb = &callbacks;
    adobe_dlss5::Settings bypass; bypass.mode = 1;
    PF_SmartRenderInput smartInput{}; smartInput.bitdepth = 8; smartInput.pre_render_data = &bypass; extra.input = &smartInput;
    for (int frame = 0; frame < 40; ++frame) for (int size : {4, 8, 16}) {
        pixelBytes = size;
        const int width = 37 + worker, height = 19 + frame % 3;
        const int srcStride = width * size + 12, dstStride = width * size + 28;
        std::vector<unsigned char> input(srcStride * height), output(dstStride * height, 0xcd);
        for (size_t i = 0; i < input.size(); ++i)
            input[i] = static_cast<unsigned char>(i * 17 + worker * 29 + frame * 7);
        PF_ParamDef source{};
        source.u.ld.width = width; source.u.ld.height = height; source.u.ld.rowbytes = srcStride;
        source.u.ld.data = reinterpret_cast<PF_Pixel*>(input.data());
        PF_ParamDef controls[ParamCount]{};
        std::copy(std::begin(registered),std::end(registered),controls);
        controls[Mode].u.pd.value=1;
        PF_ParamDef* params[ParamCount]{};
        for(int i=0;i<ParamCount;++i)params[i]=&controls[i];
        params[0]=&source;
        PF_LayerDef destination = source.u.ld;
        destination.rowbytes = dstStride;
        destination.data = reinterpret_cast<PF_Pixel*>(output.data());
        sourceWorld = &source.u.ld; destWorld = &destination;
        for (PF_Cmd cmd : {PF_Cmd_RENDER, PF_Cmd_SMART_RENDER}) {
            std::memset(output.data(), 0xcd, output.size());
            const int beforeCopy = copied, beforeCheckin = checkedIn;
            callbackError = 0;
            require(effect(cmd, &in, &out, params, &destination, &extra) == 0, "Concurrent render failed");
            require(copied == beforeCopy + 1, "Concurrent copy count wrong");
            require(checkedIn == beforeCheckin + (cmd == PF_Cmd_SMART_RENDER), "Concurrent checkout leak");
            for (int y = 0; y < height; ++y) {
                require(!std::memcmp(input.data() + y * srcStride, output.data() + y * dstStride, width * size),
                    "Concurrent requests mixed pixels");
                for (int x = width * size; x < dstStride; ++x)
                    require(output[y * dstStride + x] == 0xcd, "Concurrent output padding overwritten");
            }
        }
        callbackError = 123;
        const int beforeCheckin = checkedIn;
        require(effect(PF_Cmd_SMART_RENDER, &in, &out, params, &destination, &extra) == 123 &&
            checkedIn == beforeCheckin + 1, "Concurrent error cleanup failed");
        callbackError = 0;
    }
}

int main(int argc, char** argv) {
    try {
        require(argc == 2, "Expected plugin path");
        HMODULE module = LoadLibraryExA(argv[1], nullptr, LOAD_WITH_ALTERED_SEARCH_PATH);
        require(module != nullptr, "Plugin failed to load");
        auto effect = reinterpret_cast<MainFn>(GetProcAddress(module, "EffectMain"));
        auto registerEffect = reinterpret_cast<PluginDataEntryFunction2Ptr>(GetProcAddress(module, "PluginDataEntryFunction2"));
        require(effect && registerEffect, "SDK entry points missing");
        require(FindResourceA(module, MAKEINTRESOURCEA(16000), "PiPL") != nullptr, "PiPL 16000 missing");
        require(registerEffect(nullptr, Register, nullptr, "After Effects", "26.3") == 0, "Registration metadata incorrect");
        callbackError = 123;
        require(registerEffect(nullptr, Register, nullptr, "Premiere Pro", "26.3") == 123, "Registration error swallowed");
        callbackError = 0;
        PF_InData in{};
        PF_OutData out{};
        PF_UtilCallbacks utils{};
        utils.copy = Copy;
        in.utils = &utils;
        in.inter.add_param = AddParam;
        require(effect(PF_Cmd_GLOBAL_SETUP, &in, &out, nullptr, nullptr, nullptr) == 0, "Global setup failed");
        require(out.my_version == TOOLS_VERSION && out.out_flags == TOOLS_FLAGS && out.out_flags2 == TOOLS_FLAGS2,
            "Global flags differ from resource source");
        require((out.out_flags2 & PF_OutFlag2_SUPPORTS_THREADED_RENDERING) != 0, "MFR support flag missing");
        require(effect(PF_Cmd_PARAMS_SETUP, &in, &out, nullptr, nullptr, nullptr) == 0 && added == ParamCount-1 && out.num_params == ParamCount,
            "Parameter registration count incorrect");
        require(registered[Intensity].param_type==PF_Param_SLIDER && registered[Style].param_type==PF_Param_POPUP &&
            registered[Mix].param_type==PF_Param_SLIDER && registered[Encoding].param_type==PF_Param_POPUP,
            "Saved-project parameter types changed");
        require(registered[Look].u.pd.num_choices==9 && registered[Style].u.pd.num_choices==3,
            "Missing preset or style choices");
        int groupBalance=0, valueCount=0;
        for(int i=1;i<ParamCount;++i) {
            if(registered[i].param_type==PF_Param_GROUP_START)++groupBalance;
            if(registered[i].param_type==PF_Param_GROUP_END)--groupBalance;
            require(groupBalance>=0,"Unbalanced parameter group");
            if(isValue(i))++valueCount;
            if(specs[i].inLook) require((registered[i].flags&PF_ParamFlag_SUPERVISE)!=0,"Editable preset parameter is not supervised");
        }
        require(groupBalance==0&&valueCount==ValueCount,"Parameter group or checkout count wrong");
        PF_ParamDef presetControls[ParamCount];std::copy(std::begin(registered),std::end(registered),presetControls);
        PF_ParamDef* presetParams[ParamCount];for(int i=0;i<ParamCount;++i)presetParams[i]=&presetControls[i];
        presetControls[Look].u.pd.value=3;presetControls[Encoding].u.pd.value=2;presetControls[View].u.pd.value=2;
        PF_UserChangedParamExtra changed{};changed.param_index=Look;
        require(effect(PF_Cmd_USER_CHANGED_PARAM,&in,&out,presetParams,nullptr,&changed)==0,"Preset application failed");
        require(presetControls[Style].u.pd.value==2&&presetControls[Intensity].u.sd.value==60&&
            presetControls[Tone].u.fs_d.value==45&&presetControls[ColorHold].u.fs_d.value==85,"Portrait recipe values not applied");
        require(presetControls[Encoding].u.pd.value==2&&presetControls[View].u.pd.value==2,"Recipe changed color interpretation or comparison");
        require((presetControls[Tone].uu.change_flags&PF_ChangeFlag_CHANGED_VALUE)!=0,"Preset changes not undoable/visible");
        presetControls[Tone].u.fs_d.value=111;changed.param_index=Tone;
        require(effect(PF_Cmd_USER_CHANGED_PARAM,&in,&out,presetParams,nullptr,&changed)==0&&
            presetControls[Look].u.pd.value==1&&presetControls[Tone].u.fs_d.value==111,"Manual preset adjustment lost");
        callbackError = 123;
        require(effect(PF_Cmd_PARAMS_SETUP, &in, &out, nullptr, nullptr, nullptr) == 123, "Parameter error swallowed");
        callbackError = 0;
        require(effect(PF_Cmd_ABOUT, &in, &out, nullptr, nullptr, nullptr) == 0 &&
            std::strstr(out.return_msg, "Experimental neural"), "Missing honest capability status");
        SPBasicSuite basic{};
        basic.AcquireSuite = Acquire;
        basic.ReleaseSuite = Release;
        in.pica_basicP = &basic;
        worldSuite.copy = Copy;
        paramSuite.PF_UpdateParamUI=UpdateParamUi;
        updates::writeCache({static_cast<long long>(std::time(nullptr)),"65000.0.0"});
        const auto originalParams=std::vector<PF_ParamDef>(std::begin(presetControls),std::end(presetControls));
        out.out_flags=0;
        require(effect(PF_Cmd_UPDATE_PARAMS_UI,&in,&out,presetParams,nullptr,nullptr)==0&&uiUpdates==1,
            "Update status UI callback failed");
        require(std::strstr(lastStatus.PF_DEF_NAME,"Update 65000.0.0 available")!=nullptr,"Newer stable version was not shown");
        require(!std::memcmp(originalParams.data(),presetControls,sizeof(presetControls))&&!(out.out_flags&PF_OutFlag_FORCE_RERENDER),
            "Cosmetic update check modified parameters or requested rendering");
        changed.param_index=UpdateAction;presetControls[UpdateAction].u.pd.value=3;
        require(effect(PF_Cmd_USER_CHANGED_PARAM,&in,&out,presetParams,nullptr,&changed)==0&&!updates::automaticEnabled(),"Automatic update opt-out failed");
        require(presetControls[UpdateAction].u.pd.value==1&&!(out.out_flags&PF_OutFlag_FORCE_RERENDER),"Update action changed render state");
        updates::setAutomatic(true);updates::writeCache({static_cast<long long>(std::time(nullptr)),"1.0.0"});
        require(effect(PF_Cmd_UPDATE_PARAMS_UI,&in,&out,presetParams,nullptr,nullptr)==0&&
            !std::strcmp(lastStatus.PF_DEF_NAME,"v" TOOLS_PRODUCT_VERSION),"Older release produced a false update notice");
        PF_SmartRenderCallbacks callbacks{};
        callbacks.checkout_layer_pixels = CheckoutInput;
        callbacks.checkout_output = CheckoutOutput;
        callbacks.checkin_layer_pixels = Checkin;
        PF_SmartRenderExtra extra{};
        extra.cb = &callbacks;
    adobe_dlss5::Settings bypass; bypass.mode = 1;
    PF_SmartRenderInput smartInput{}; smartInput.bitdepth = 8; smartInput.pre_render_data = &bypass; extra.input = &smartInput;
        for (int size : {4, 8, 16}) {
            pixelBytes = size;
            const int width = 5, height = 3, inputStride = width * size + 12, outputStride = width * size + 28;
            std::vector<unsigned char> input(inputStride * height, 0x5a), output(outputStride * height, 0xcd);
            for (size_t i = 0; i < input.size(); ++i) input[i] = static_cast<unsigned char>(i * 17);
            PF_ParamDef source{};
            source.u.ld.width = width; source.u.ld.height = height; source.u.ld.rowbytes = inputStride;
            source.u.ld.data = reinterpret_cast<PF_Pixel*>(input.data());
            PF_ParamDef controls[ParamCount]{};
        std::copy(std::begin(registered),std::end(registered),controls);
        controls[Mode].u.pd.value=1;
        PF_ParamDef* params[ParamCount]{};
        for(int i=0;i<ParamCount;++i)params[i]=&controls[i];
        params[0]=&source;
            PF_LayerDef destination = source.u.ld;
            destination.rowbytes = outputStride;
            destination.data = reinterpret_cast<PF_Pixel*>(output.data());
            sourceWorld = &source.u.ld; destWorld = &destination;
            for (PF_Cmd command : {PF_Cmd_RENDER, PF_Cmd_SMART_RENDER}) {
                std::memset(output.data(), 0xcd, output.size());
                const int beforeCopy = copied, beforeCheckin = checkedIn;
                require(effect(command, &in, &out, params, &destination, &extra) == 0, "Passthrough render failed");
                require(copied == beforeCopy + 1, "Host copy not invoked");
                if (command == PF_Cmd_SMART_RENDER) require(checkedIn == beforeCheckin + 1, "Input not checked in");
                for (int y = 0; y < height; ++y) {
                    require(!std::memcmp(input.data() + y * inputStride, output.data() + y * outputStride, width * size),
                        "Pixels changed in passthrough");
                    for (int x = width * size; x < outputStride; ++x)
                        require(output[y * outputStride + x] == 0xcd, "Output padding overwritten");
                }
            }
        }
        callbackError = 123;
        const int beforeCheckin = checkedIn;
        require(effect(PF_Cmd_SMART_RENDER, &in, &out, nullptr, nullptr, &extra) == 123 && checkedIn == beforeCheckin + 1,
            "Smart-render failure leaked checkout or hid error");
        callbackError = 0;
        std::atomic<int> ready{0}, failures{0};
        std::atomic<bool> start{false};
        std::vector<std::thread> workers;
        for (int worker = 0; worker < 8; ++worker) workers.emplace_back([&, worker] {
            ++ready;
            while (!start.load()) std::this_thread::yield();
            try { ConcurrentFrames(effect, worker); }
            catch (const std::exception& error) { ++failures; std::fprintf(stderr, "%s\n", error.what()); }
        });
        while (ready.load() != 8) std::this_thread::yield();
        start = true;
        for (auto& worker : workers) worker.join();
        require(failures.load() == 0, "Concurrent host contract failed");
        require(maximumCopies.load() > 1, "Test did not exercise overlapping render callbacks");
        require(effect(PF_Cmd_GLOBAL_SETDOWN, &in, &out, nullptr, nullptr, nullptr) == 0, "Global cleanup failed");
        FreeLibrary(module);
        std::printf("PASS: registration, PiPL, parameters, errors, 8/16/32-bit host copy; 1,920 concurrent renders plus 960 error cleanups across 8 workers (peak overlap %d).\n", maximumCopies.load());
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "FAIL: %s\n", error.what());
        return 1;
    }
}
