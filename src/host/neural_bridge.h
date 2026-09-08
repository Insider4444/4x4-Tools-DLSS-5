#pragma once
#include <string>
#include <vector>
namespace adobe_dlss5 {
struct Settings {
    int mode = 2, view = 1, strength = 100, preset = 1, mix = 100, encoding = 1;
    int look = 1;
    float tone = 100, structure = 100, autoMask = 0;
    float colorHold = 0, exposureHold = 0, highlights = 0, shadows = 0;
    float texture = 0, detail = 0, radius = 1.5F, artifactGuard = 0;
    float saturation = 100, warmth = 0, tint = 0, exposure = 0;
    float wipe = 50, region = 1, centerX = 50, centerY = 50;
    float regionWidth = 65, regionHeight = 75, feather = 30;
    float renderScale = 1;
};
bool processFrame(const std::vector<float>& input, std::vector<float>& output,
    int width, int height, const Settings& settings, std::string& error);
void shutdownEngine() noexcept;
}
