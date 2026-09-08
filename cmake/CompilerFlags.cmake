###############################################################################
# cmake/CompilerFlags.cmake — Production compiler hardening & optimization
###############################################################################

# Function to apply standard compiler flags to a target
function(apply_compiler_flags TARGET_NAME)
    if(MSVC)
        # ── Warning Level ────────────────────────────────────────────────
        target_compile_options(${TARGET_NAME} PRIVATE /W4)

        # Suppress specific warnings from SDK headers
        target_compile_options(${TARGET_NAME} PRIVATE
            /wd4100    # unreferenced formal parameter (common in SDK callbacks)
            /wd4201    # nameless struct/union (used by DirectX)
            /wd4324    # structure was padded
            /wd4458    # declaration hides class member (AE SDK headers)
        )

        # ── Security Hardening ───────────────────────────────────────────
        target_compile_options(${TARGET_NAME} PRIVATE
            /GS        # Buffer security check
            /guard:cf  # Control Flow Guard
            /sdl       # Additional security checks
        )
        target_link_options(${TARGET_NAME} PRIVATE
            /DYNAMICBASE     # ASLR
            /NXCOMPAT        # Data Execution Prevention
            /HIGHENTROPYVA   # High-entropy ASLR
            /GUARD:CF        # CFG in linker
        )

        # ── Optimization ─────────────────────────────────────────────────
        target_compile_options(${TARGET_NAME} PRIVATE
            $<$<CONFIG:Release>:/O2>            # Maximum speed
            $<$<CONFIG:Release>:/GL>            # Whole program optimization
            $<$<CONFIG:Release>:/Oi>            # Intrinsic functions
            $<$<CONFIG:Release>:/Gy>            # Function-level linking
            $<$<CONFIG:RelWithDebInfo>:/O2>
            $<$<CONFIG:RelWithDebInfo>:/Zi>     # Debug info
        )
        target_link_options(${TARGET_NAME} PRIVATE
            $<$<CONFIG:Release>:/LTCG>          # Link-time code gen
            $<$<CONFIG:Release>:/OPT:REF>       # Eliminate unused functions
            $<$<CONFIG:Release>:/OPT:ICF>       # Fold identical COMDATs
        )

        # ── C++ Standard ─────────────────────────────────────────────────
        target_compile_options(${TARGET_NAME} PRIVATE
            /Zc:__cplusplus    # Correct __cplusplus macro
            /Zc:preprocessor   # Standards-conforming preprocessor
            /utf-8             # UTF-8 source files
            /permissive-       # Standards conformance
            /EHsc              # C++ exception handling
        )

        # ── Static CRT ───────────────────────────────────────────────────
        if(DLSS5_STATIC_CRT)
            set_property(TARGET ${TARGET_NAME} PROPERTY
                MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
        endif()

        # ── Debug ────────────────────────────────────────────────────────
        target_compile_options(${TARGET_NAME} PRIVATE
            $<$<CONFIG:Debug>:/Od>       # No optimization
            $<$<CONFIG:Debug>:/Zi>       # Debug info
            $<$<CONFIG:Debug>:/RTC1>     # Runtime checks
        )
        target_compile_definitions(${TARGET_NAME} PRIVATE
            $<$<CONFIG:Debug>:_DEBUG>
            $<$<CONFIG:Release>:NDEBUG>
        )

    endif()

    # Unicode
    target_compile_definitions(${TARGET_NAME} PRIVATE
        UNICODE
        _UNICODE
        WIN32_LEAN_AND_MEAN
        NOMINMAX
        _CRT_SECURE_NO_WARNINGS
    )
endfunction()
