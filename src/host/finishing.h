#pragma once
#include "neural_bridge.h"
namespace adobe_dlss5 {
// Float straight-alpha RGBA, in-place neural output. No global frame/history state.
void finishFrame(const std::vector<float>& source, std::vector<float>& neural,
    int width, int height, const Settings& settings);
}
