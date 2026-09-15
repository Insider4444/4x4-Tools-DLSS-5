#include "neural_bridge.h"
#include "Feature18Runtime.h"
#include "finishing.h"
#include <algorithm>
#include <chrono>
#include <mutex>

namespace adobe_dlss5 {
namespace {
std::mutex engineMutex;
// Exactly one session per loaded Adobe module. Never initialize NGX during scan.
// Explicit GLOBAL_SETDOWN cleanup avoids running GPU code under DllMain's loader lock.
resolve_dlss5::Feature18Runtime* engine = nullptr;
std::string lastFailure;
std::chrono::steady_clock::time_point retryAfter{};
}
bool processFrame(const std::vector<float>& input, std::vector<float>& output,
    int width, int height, const Settings& settings, std::string& error,
    const FrameGeometry& geometry) {
    std::scoped_lock lock(engineMutex);
    if (settings.strength <= 0 || settings.mix <= 0 || settings.mode != 2) {
        output = input;
        finishFrame(input, output, width, height, settings, geometry);
        return true;
    }
    if (!lastFailure.empty() && std::chrono::steady_clock::now() < retryAfter) {
        error = lastFailure;
        return false;
    }
    if (!engine) engine = new resolve_dlss5::Feature18Runtime();
    resolve_dlss5::Feature18Settings native;
    // The shipped runtime ignores render preset hints. Style actually changes its model/look.
    // Keep one creation preset, so switching looks does not rebuild the GPU session.
    native.preset = resolve_dlss5::NrPreset::Preset1;
    native.style = std::clamp(settings.preset - 1, 0, 2);
    // Native intensity saturates at 1. Above 100%, finishFrame scales the neural residual.
    native.intensity = std::clamp(settings.strength / 100.0F, 0.0F, 1.0F);
    native.localToneStrength = std::clamp(settings.tone / 100.0F, 0.0F, 2.0F);
    native.localStructureStrength = std::clamp(settings.structure / 100.0F, 0.0F, 2.0F);
    native.useAutoMask = settings.autoMask >= 0.5F;
    native.preserveInputPrecision = settings.preserveInputPrecision;
    native.inputEncoding = settings.encoding == 2 ? resolve_dlss5::InputEncoding::LinearScRgb :
        resolve_dlss5::InputEncoding::SdrSrgb;
    // Reset every request: AE MFR and Premiere can seek/reorder/duplicate frames.
    // This build deliberately has no temporal history shared between clips.
    const bool ok = engine->process(input.data(), width * 4 * sizeof(float), output.data(),
        width * 4 * sizeof(float), width, height, native, true);
    if (!ok) {
        error = lastFailure = engine->lastError();
        retryAfter = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    } else {
        lastFailure.clear();
        finishFrame(input, output, width, height, settings, geometry);
    }
    return ok;
}
void shutdownEngine() noexcept {
    std::scoped_lock lock(engineMutex);
    if (engine) resolve_dlss5::writeDiagnosticLog("Adobe requested shared-engine teardown");
    delete engine;
    engine = nullptr;
    lastFailure.clear();
}
}
