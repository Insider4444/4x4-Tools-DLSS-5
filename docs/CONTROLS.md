# 4x4-Tools DLSS 5 v1.0 — controls and starting points

Apply **4x4Tools > 4x4Tools-DLSS5**, select **Neural (experimental)** and choose a **Footage preset**. Start with **Natural balance** for general footage. Use **Output → Original / neural wipe** to compare, then return to **Processed** for export.

## Eight editable footage presets

| Preset | Starting point |
| --- | --- |
| Natural balance | Moderate enhancement while retaining original color and lighting. |
| Portrait / skin | Gentler structure, automatic neural masking and stronger restoration of original texture. |
| Sports / action | More structure and restrained detail enhancement, with some original texture retained. |
| Product / fabric | Strong color preservation, texture recovery and highlight protection. |
| Landscape / daylight | Broader neural tone changes with protection for bright areas. |
| Low light / gentle | Reduced neural strength and strong shadow/artifact protection. |
| Animation / game | More stylized neural structure with partial color preservation. |
| Strong enhancement | Stronger tone, structure and neural residual. Use the wipe to check faces and fine edges. |

These are starting recipes, not automatic footage classification or separately trained models. Choose the result that suits the shot. Changing a recipe-controlled slider switches **Footage preset** to **Manual**, retaining the other recipe values so you can fine-tune them. Recipe selection leaves the input color interpretation, output view and selective region unchanged. For scripts or expressions, use Manual when overriding or animating individual controls.

## Neural controls

| Control | What it changes |
| --- | --- |
| Neural style | Three actual runtime looks: Default, Natural and Cinematic. These replace the ineffective numbered preset hints. |
| Neural intensity, 0–200 | 0 bypasses; 1–100 uses native neural intensity. Above 100, the plug-in amplifies the difference between the original and neural image because native intensity saturates at 100. |
| Mix, 0–100 | Final blend with the original. 0 preserves original pixels. |
| Neural tone, 0–200 | The runtime's broader lighting and tone transformation. |
| Neural structure, 0–200 | The runtime's structure/detail transformation. Higher is more aggressive. |
| Automatic neural mask | Lets the runtime choose where to apply its transformation. This is not a user-editable face or object matte. |

## Natural restoration

| Control | What it changes |
| --- | --- |
| Preserve original color | Blends original chroma back into the neural result. Raise it when the neural style shifts colors too far. |
| Preserve original lighting | Restores broad original brightness while retaining finer changes. |
| Protect highlights / shadows | Reduces the final effect in bright or dark source areas, including color finishing. |
| Recover original texture | Restores source luminance texture that neural processing softened or changed. It can also restore source noise. |
| Detail: soft / crisp | Negative values soften fine luminance detail; positive values sharpen with limits to reduce ringing and noise amplification. |
| Detail radius (pixels) | Size of the detail filter, adjusted for AE preview resolution. It matters when detail, texture recovery or artifact protection is active. |
| Artifact protection | Limits large neural changes in locally flat areas by blending back the source. It is a contrast-based guard, not an AI artifact detector. |

Color finishing adds **Saturation**, **Warmth**, **Tint** and **Exposure in stops**. These affect the processed side of the comparison; they leave the original side untouched.

## Compare and selective blend

Move **Wipe position** to inspect different areas. At 0 the entire image is processed; at 100 the entire image is original. **Difference x10** makes small changes easier to see.

Choose **Inside ellipse** or **Outside ellipse** to restrict the final enhancement. Position and resize it with the region controls; increase **Region feather** for a softer boundary. **Effect matte** shows the plug-in's final blend weight: white receives the effect, black preserves the original. It includes Mix, geometric selection and restoration protection; it does not display the runtime's internal automatic mask. The GPU still evaluates the full image before selective blending.

## Practical limits and performance

Use SDR for ordinary display-referred footage. Linear HDR is experimental. The neural stage uses an RGBA8 proxy even in AE 16-/32-bit projects; it cannot recover all precision or missing scene information. The plug-in performs same-resolution enhancement, not upscaling or frame generation. It does not have real depth or motion vectors, and it resets neural history for every frame. Check moving footage for flicker, changes in faces, text, fabric patterns and edges before a final export.

The original fast path remains active when restoration/finishing controls are neutral. The additional restoration filters cost CPU time: the Natural balance finishing stage measured approximately **85 ms at 1920 × 1080** in the local test, excluding neural evaluation and Adobe overhead. Quarter-resolution preview substantially reduces that work. Large simultaneous Adobe projects can still compete for GPU memory.

Saved 1.1.x effects keep their parameter IDs and types. The old numbered Preset control now selects the working Neural style, and intensity above 100 now has an effect; those settings can therefore change older projects' appearance. **Default style, intensity 100, Manual preset and neutral restoration controls** retain the former baseline behavior.
