# 4x4Tools DLSS5 for Photoshop

Included in the v1.2 suite with After Effects and Premiere Pro. Windows x64,
Photoshop 2026 or newer, and a compatible NVIDIA RTX GPU are required.

Open **Filter > 4x4Tools > DLSS5 - Image Enhancement** on an RGB layer.
Use **Natural balance** first. Select a crop center, click **Preview**, and
toggle **Original** to compare. **Apply** processes the full image at its original
dimensions. The preview is a 512 x 512 source-pixel crop fitted to its panel.
Changing controls requires another Preview. Apply always uses the current values.

## High-resolution images

- RGB 8-, 16- and 32-bit host pixels, including Photoshop's 0–32768 internal
  16-bit range. The image's dimensions, profile and transparency are retained.
- Large-document coordinates, with dimensions up to Photoshop's 300,000-pixel
  limit. This is a coordinate limit, not a claim that a 90-gigapixel image has
  been rendered. Actual capacity depends on memory, free scratch space and time.
- Overlapping tiles retain context around each region. Smooth overlap blending
  includes four-tile corners; lighting restoration and masks use document coordinates.
- The CPU pipeline keeps tiles and overlap strips, rather than two full RGBA
  images. Default working memory is budgeted at 512 MiB; Photoshop, color
  management and GPU runtime allocations are additional.
- The temporary drive needs roughly 12 bytes per selected pixel plus 256 MiB
  free space. Enhanced pixels are staged in a private delete-on-close file before
  committing to Photoshop. Cancellation and processing failures discard staging.
- **Low VRAM** uses smaller tiles. **Balanced** is the default. **Larger context**
  provides the model with a bigger surrounding region and uses more VRAM.
- No upscaling: an 8192 x 4320 input remains 8192 x 4320. This does not invent a
  higher-resolution source or supply frame generation.

The neural model itself uses an 8-bit proxy. Float processing and residual
reconstruction preserve original sub-8-bit information; they do not turn the
model into native 16- or 32-bit inference. HDR remains experimental.

## Color, layers and controls

ICC conversion uses Little CMS, converting document RGB to sRGB (linear sRGB
for 32-bit images) for processing and back to the original profile. Untagged
images assume sRGB. 32-bit processing requires a matrix RGB profile. CMYK,
indexed color, individual channels and layer-mask-only filtering are not supported.

Photoshop owns selection masking and undo. Only RGB channels are written;
transparency and extra channels are preserved. Use a duplicate layer for comparisons.
All image controls are recorded in the Photoshop action descriptor for repeat use.

The Neural, Restore, Color and Compare tabs share the suite's presets, styles,
intensity, mix, color/lighting preservation, texture recovery and selective blend.
See [CONTROLS.md](CONTROLS.md) for the shared controls. Input encoding follows
the document bit depth automatically.

## Scripting

The filter event is `stringIDToTypeID("ca8c8e71-f8ea-433b-a977-3857309e371f")`.
Photoshop registers the scope UUID as the callable event; calling the raw
four-character `T4P5` terminology ID returns "command not available".
Numeric descriptor keys are
documented in `resources/photoshop-pipl.json`; for example `x003` is intensity,
`x005` is mix, `x008` selects a preset, and `x099` sets the tile core size.
Use `DialogModes.NO` for unattended processing. Invalid image modes and GPU
failures return a Photoshop error instead of silently producing an unchanged image.

The small version line in the dialog shows a newer stable release when available.
The Updates button provides manual checking, downloads and an automatic-check
toggle. No image data is uploaded and no update is installed automatically.

The integration is independent of Adobe and NVIDIA. See the suite's
[compatibility notes](COMPATIBILITY.md) for verified hardware and runtime limits.
