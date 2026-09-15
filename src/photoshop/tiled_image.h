#pragma once
#include "neural_bridge.h"
#include <cstddef>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace photoshop_dlss5 {
struct ImageRect { int x=0, y=0, width=0, height=0; };
struct ImageSize { int width=0, height=0; };
struct TileOptions {
    int core=1024;
    int context=256;
    int overlap=64;
    std::uint64_t memoryBudget=512ULL*1024*1024;
};
struct TilePlan {
    int columns=0, rows=0, extent=0, step=0;
    std::uint64_t tiles=0, workingBytes=0;
};
enum class RenderResult { Success, Cancelled, Failed };

// Float, straight-alpha RGBA, top-left origin. Callbacks use sample strides,
// not byte strides. Reads must always see the immutable original document.
struct Callbacks {
    std::function<bool(ImageRect, float*, std::size_t, std::string&)> read;
    // Writes are disjoint rectangles, in row-major order. The host adapter
    // MUST stage these in a transaction/scratch store and commit only on Success.
    std::function<bool(ImageRect, const float*, std::size_t, std::string&)> stage;
    std::function<bool(std::uint64_t, std::uint64_t)> progress;
};
using Kernel = std::function<bool(const std::vector<float>&, std::vector<float>&,
    int, int, const adobe_dlss5::FrameGeometry&, std::string&)>;

bool planTiles(ImageSize image, const TileOptions& options, TilePlan& plan, std::string& error);
RenderResult renderTiles(ImageSize image, const TileOptions& options,
    const Callbacks& callbacks, const Kernel& kernel, std::string& error);
}
