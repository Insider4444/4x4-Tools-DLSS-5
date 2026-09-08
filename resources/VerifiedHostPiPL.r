#include "AEConfig.h"
#include "AE_EffectVers.h"
#include "effect_metadata.h"
#ifndef AE_OS_WIN
#include "AE_General.r"
#endif

resource 'PiPL' (16000) {
    {
        Kind { AEEffect },
        Name { TOOLS_EFFECT_NAME },
        Category { TOOLS_CATEGORY },
        CodeWin64X86 { "EffectMain" },
        AE_PiPL_Version { 2, 0 },
        AE_Effect_Spec_Version { PF_PLUG_IN_VERSION, PF_PLUG_IN_SUBVERS },
        AE_Effect_Version { TOOLS_VERSION },
        AE_Effect_Info_Flags { 0 },
        AE_Effect_Global_OutFlags { TOOLS_FLAGS },
        AE_Effect_Global_OutFlags_2 { TOOLS_FLAGS2 },
        AE_Effect_Match_Name { TOOLS_MATCH_NAME },
        AE_Reserved_Info { 8 },
        AE_Effect_Support_URL { "" }
    }
};
