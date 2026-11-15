# Phase 3 outlook: AI upscaling of art, cutscenes and sound

An outlook written on 2026-11-15, during Phase 1b. Nothing here is built yet. Model names are a snapshot; the field moves fast, so the bake-off in section 5 decides, not this list.

## 1. What there is to upscale
| Asset | Facts (from the installed game) | What makes it hard |
|---|---|---|
| Cutscenes | 77 Smacker clips, 61 minutes; 75 at 320×200 and 2 at 320×240, 15 frames a second; 8-bit palette video | very low resolution, palette banding and dithering, pre-rendered 3D with faces and text |
| Sprites | `DOMINION.000` (54 MB): pre-rendered 3D units and structures, 8-bit, eight facings (three of them mirrored at run time), many animation cells | tiny (infantry are about 30 pixels), must stay consistent across cells and facings, hard alpha edges |
| Tiles | four tilesets (`GRASS`, `DESERT`, `ARCTIC`, `MOLTEN`, about 5 MB each), isometric | must still tile without seams |
| Interface, fonts | panels, buttons, bitmap fonts | sharp edges and text: upscalers smear these |
| Sound | effects and voices in `audio.rdf` (45 MB); music is CD audio | low sample rates; music needs nothing |

Two properties of the engine shape everything for sprites and tiles:
* **Team colours are a palette trick.** A unit is drawn in its player's colours by remapping a range of palette indices (`colormap.dat`, `sprite_recolour`). A true-colour sprite has no index range, so each upscaled sprite needs a **team-colour mask** next to it. The mask can be taken exactly from the original: we know which indices are the team range.
* **Shading is a lookup table too** (fog, the darkened "being built" look, damage states through `SpriteSetBlitFX`). In true colour these become shader effects on the renderer's side.

## 2. What has to exist first (code, not AI)
1. **Export and repack tools** for sprites, tiles and videos (Phase 0 item 3, never done). Our own modules already decode all of it bit-exactly (`tests/smk_compare.sh`, `tests/blit_compare.sh`), so the tools are thin wrappers around existing code.
2. **A way to load replacements**: an "HD pack" directory beside the original data, looked up by resource id, with the original as fallback and a classic/HD switch.
3. **A renderer that can draw true colour at 2× or 4×** (the Phase 3 renderer). Until then only cutscenes can be shown upscaled, since our Smacker module can play other files.
4. **Legal shape.** Upscaled originals are derived from the original art, so they cannot be published any more than the originals. The safe model is to publish the **pipeline** (scripts and settings) and let each player build the HD pack from their own discs, or to get the rights holder's permission. New art made from scratch is ours to publish.

Cutscenes are therefore the natural first target: no renderer needed, self-contained, and the result is easy to judge.

## 3. Cutscenes
Pipeline: decode with our decoder → remove dithering and banding → upscale 4× (320×200 → 1280×800) → optionally interpolate 15 → 30 frames a second → encode → subtitles.
* **Restoration models for video** (2026): SeedVR2 (one-step diffusion restoration; open weights) is the usual pick for heavily degraded footage; FlashVSR is the faster streaming option but reported to shimmer on camera moves. Both invent detail, which suits 320×200 sources but must be watched on faces and text.
* **Conservative baseline**: a GAN upscaler of the Real-ESRGAN family run per frame with a temporal filter. Less invented detail, less risk; always include it in a comparison.
* **Frame interpolation** (RIFE-type) for smoother motion is optional and a matter of taste; 15 frames a second is part of the 1998 feel. Keep it switchable.
* **Aspect**: 320×200 was shown on 4:3 screens (non-square pixels). Decide once whether cutscenes are shown 4:3 or 16:10, before upscaling.
* **Subtitles** (in the plan): speech-to-text (Whisper-type) over the audio tracks gives a first draft to correct by hand.
* Scale: 61 minutes at 15 frames a second is about 55,000 frames, a job for one GPU over hours to days depending on the model; fine as an offline step each player runs once.

## 4. Sprites and tiles: three routes, cheapest first
**A. Upscale each cell 2× with a faithful (non-generative) model.**
* Process whole animation strips, not single cells, so neighbouring cells get the same treatment; or run a video model over the animation.
* Upscale colour on a neutral background and the alpha mask separately; do the same for the team-colour mask.
* Community ESRGAN-family models trained on pre-rendered game art are the starting point; pre-rendered 3D upscales better than hand-drawn pixel art.
* Cheap and automatic; limited by how little a 30-pixel soldier contains.

**B. Rebuild the 3D models and render again** (the Age of Empires DE route).
* Image-to-3D models (TRELLIS.2, Hunyuan3D; check their licences) can take the facings of a unit as views and give a first mesh; an artist cleans it up; then render every facing and cell at any resolution, with more facings and smoother animation than the original if wanted.
* The cutscenes show many units and structures at far higher detail than the sprites: they are reference material for this route.
* Best result, most hand work. Sensible for structures and vehicles first (large, rigid), infantry last.

**C. Re-draw with an image model guided by the original** (image-to-image with strong structure control).
* Gives detail that A cannot, without modelling; the risk is drifting from the original look and inconsistency between cells. Needs heavy curation; a fallback for assets where A is too soft and B too expensive.

**Tiles**: upscale tileset sheets with their neighbours attached (or in a model's tiling mode) so edges still match; check seams automatically by rendering test maps in the game.
**Interface and fonts**: redraw by hand or as vectors; replace bitmap fonts with a matching outline font. Not an AI job.
**Sound effects and voices**: audio super-resolution (AudioSR-type) can widen the frequency range of low-rate samples; judge by ear, keep the originals switchable.

## 5. Proposed first experiment (a bake-off, a few days)
1. Write the export tools (needed for everything else).
2. Pick three cutscenes (faces, a fast camera move, on-screen text), five sprites (a soldier, a vehicle, two structures, an effect) and one tileset.
3. Run each candidate per section 3 and 4A on them; put original and results side by side in one viewer page.
4. Judge: faithful to the original? stable in motion? acceptable team colours? Then decide how far routes B and C are worth going.

## 6. How it fits the phases
* Does not block Phase 1b or 2. The export tools can be written any time and also serve modding.
* Cutscene upscaling can ship as soon as the tools exist (our Smacker module plays the result).
* Sprites and tiles need the Phase 3 renderer.
