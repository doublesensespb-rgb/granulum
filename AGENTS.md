# GRAIN FORM (repo: granulum) — guide for AI agents and developers

Read this whole file before changing anything. It describes the product, the code, the build
pipeline and how the owner wants to work. Current version: **0.91** (`project(... VERSION 0.9.1)` in CMakeLists.txt).

## 1. Product

GRAIN FORM, subtitle "SELECTIVE GRANULATION" (formerly Granulum), is a commercial VST3/AU/Standalone
instrument by Andrey Ulyanov (artist name Thematic, manufacturer "Thematic"), a producer and game sound designer.
He uses FL Studio on Windows. Goal: a simple-looking synth that gives "wow" results through granular synthesis,
where you choose WHICH harmonics get granulated (the bass can stay clean and mono).

Signal flow:

```
OSC 1 ADDITIVE (16 drawable harmonics, chords, per-harmonic SELECT dots) ─┐
OSC 2 SAMPLE   (dropped/loaded WAV, loops when its GRAIN is off)          ├─ each OSC: GRAIN button -> GRANULAR macros -> own FX chain ─> OUTPUT
OSC 3 NOISE    (10 built-in noises, no key tracking)                      ┘
FX chain per OSC = FILTER 1 -> FILTER 2 -> DISTORTION -> DELAY -> REVERB
"1 2 3" digits on every block choose which OSCs it processes (like Serum's routing).
Amp envelope: ADSR tab (with on/off lamp; off = plain gate) in the modulation strip.
```

- Every OSC, filter and FX block has an on/off lamp. The GRANULAR block has NO lamp (removed in 0.91):
  each OSC's GRAIN button decides. Its knobs dim when no active OSC has GRAIN on.
- 3 LFOs (default triangle, 4-point drawable shapes) are dragged onto any knob. Each LFO tab's lamp switches
  that LFO off everywhere (routings kept). REMOVE LINKS deletes all routings of that LFO.
- Filter view: small window under CUTOFF/RESO showing the response curve over a live input spectrum;
  drag left/right = CUTOFF, up/down = RESO.
- The plugin opens in INIT: one sine harmonic, all GRAIN buttons off, effects off.

## 2. Owner's rules (important)

- The owner does not code. The agent does everything: write code, build, check, deliver files.
  No step-by-step instructions for him unless he asks.
- When he sends change requests, FIRST agree on the changes in text, THEN implement.
  For visual changes, render a preview picture locally (see section 5) and show it BEFORE building.
- **NEVER start a CI build without his explicit command** ("собирай", "делай сборку").
- Version naming: 0.9 was delivered; then 0.91, 0.92 ... Deliver files with the version in the name:
  `GRAIN-FORM-0.92-Windows.zip`, `GRAIN-FORM-0.92-macOS.zip`. Bump `VERSION` in CMakeLists.txt (0.92 -> 0.9.2).
- Always give links (to builds, tools, sources).
- He writes in Russian, often with typos or wrong keyboard layout. Reply in Russian.
- Design: teal "deep petrol" flat palette (`Palette` in `PluginEditor.h`), lighter/darker shades only.
  NO violet, NO pink, NO yellow, NO cyberpunk. Signature colour of the granulated-harmonic feature: sand/pearl (`Palette::hit`).
  Flat knobs everywhere. Soft segmented selectors. Keep it simple and clean.
- GRANULAR shows macros only, never raw grain parameters.
- **Never change** `PLUGIN_MANUFACTURER_CODE Thmt`, `PLUGIN_CODE GrFm`, `BUNDLE_ID`, or `PRODUCT_NAME` — the DAW would see
  a different plugin and saved projects would break.
- **Never rename or remove parameter IDs** (`ParamID` in PluginProcessor.h) — saved FL projects and presets break.
  If a feature is removed, keep the parameter and ignore it (see `grainOn`, kept as "Granular On (legacy)"),
  and migrate old states in `setStateInformation`.

## 3. Code map

| File | What it is |
|---|---|
| `Source/GrainEngine.h` | Header-only DSP: additive renderer (16 harmonics, `renderAdditiveSplit` grain/clean parts), voices, chords, portamento, ADSR, grain spawning from macros, CHIME resonator bank, viz atomics for the UI |
| `Source/PluginProcessor.h/.cpp` | Parameters (`ParamID`), mod matrix (48 slots, `ModTarget`), 3 LFOs, `FilterUnit` (LP12, LP24, HP, BP, NOTCH, COMB, VOWEL + `response()` for the filter view), `Distortion`, `Delay`, per-OSC `FxChain`, routing, noises (BinaryData FLAC), presets, INIT/randomize, state save/load + migration |
| `Source/PluginEditor.h/.cpp` | JUCE UI 1080x800 (resizable, fixed aspect): `GranulumLook`, `Knob`, `GrainField`, `HarmonicEditor`, `PeakMeter`, `ChordPicker`, `LfoTab`, `LfoEditor`, `AmpTab`, `AmpEditor`, `FilterScope`, `RouteButtons`, `FxTab`, `PresetBar`, `NoisePicker`, `Lamp`, `GranulumEditor` |
| `tools/Snapshot.cpp` | Console app that renders the editor to PNG (CI previews + design preview modes: themes, grainrow, strings, select, toggles, digits, fxtabs, amp, remove, scope) |
| `tools/make_noises.py` | Generates the OSC 3 noises (numpy/scipy). Output WAV -> convert to FLAC into `Resources/noises/` |
| `test/engine_test.cpp` | Pure C++ engine tests: `g++ -std=c++17 -O2 -ISource test/engine_test.cpp && ./a.out` |
| `.github/workflows/build.yml` | CI: Windows + macOS builds, publishes zips to the `builds` branch |
| `tools/build-windows.bat` | Local one-click Windows build (Visual Studio 2022 + CMake) |

Key facts:
- Macros: BLOOM, SMEAR, SCATTER, SHIMMER, DRIFT, CHIME (big); SIZE, SPEED, POSITION, DENSITY, MIX (small).
  EVOLVE was removed by the owner. Mapping to grain length/pitch/position/pan is in `GrainEngine::spawnGrain`.
- CHIME = per-voice resonator bank, lifted by octaves above ~700 Hz so low chords don't get muddy; stereo detune/pan.
- Chords (OSC 1 only): OFF, MAJ, MIN, MAJ7, MIN7, MAJ9, MIN9, DARK, SPACE.
- OSC 1 SELECT dots (`grainMask`, saved in state): off = that harmonic stays clean & mono (`engineAddClean`).
- OSC 3 noises: AIR, TAPE HISS, VINYL, STATIC, WIND, BREATH, PAPER, DUST, FROST, METAL RAIN (FLAC, embedded via
  `juce_add_binary_data`). The owner does NOT want users to drop their own noises into OSC 3. Noise does not track the note.
- Filters key-track from C3 = MIDI 60; readout "1.8 kHz  A5". REVERB has a LOW CUT on the wet signal only.
- Presets: XML `.gpreset` files in `Documents/GRAIN FORM/Presets`. UI settings (first-run hint) in userAppData/GRAIN FORM/settings.xml.
- Output: slim L/R peak meter with red zone + CLIP.
- Audio thread is lock-free: sample buffers go through `SourceSlot`, LFO tables are double-buffered. Audio is processed in
  32-sample chunks so modulation is smooth.
- Static design flags (owner-approved, keep): `GranulumLook::flatKnobs=true`, `toggleStyle=5`, `HarmonicEditor::barStyle=5`
  (strings), `rowStyle=2` (dots), `FxTab::style=2`, `RouteButtons::style=3`.

## 4. Build pipeline

`.github/workflows/build.yml` runs ONLY when started manually (`gh workflow run build.yml --ref main`, or GitHub -> Actions ->
Build -> Run workflow). Never start it before the owner has approved all pending changes and said to build.
1. `windows` job (windows-latest, MSVC): VST3 + Standalone + Snapshot, renders `ui-*.png`, zips `GRAIN-FORM-Windows.zip`.
2. `mac` job (macos-latest, Xcode): universal arm64+x86_64 VST3 + AU + Standalone, ad-hoc codesign, `GRAIN-FORM-macOS.zip`.
3. `publish` job: force-pushes zips + PNGs to the `builds` branch.

JUCE 8.0.10 is cloned by CI (not committed). A build takes ~15 min.

Workflow loop:
1. Agree on changes with the owner. Work on a branch if the changes are not approved yet.
2. Edit code, run `test/engine_test.cpp`.
3. Visual change -> render a preview with Snapshot locally (section 5), show it, get approval.
4. On his command: merge to `main`, bump VERSION, push, start the workflow, wait (`gh run list -L 1`).
5. Fetch results: `git clone --depth 1 --branch builds https://github.com/doublesensespb-rgb/granulum`.
6. LOOK at the PNG screenshots, fix anything broken.
7. Rename zips with the version and send them with a short summary of what changed.

Install on Windows: replace `GRAIN FORM.vst3` in `C:\Program Files\Common Files\VST3`, rescan in FL.
Mac: see `README.txt` inside the mac zip (copy to `/Library/Audio/Plug-Ins/...`, remove quarantine with `xattr`).

## 5. Fast local UI previews on Linux (no CI)

Compile the JUCE modules once (`g++ -std=c++17 -O1 -I JUCE/modules -c JUCE/modules/<m>/<m>.cpp` for juce_core, juce_events,
juce_data_structures, juce_graphics, juce_gui_basics, juce_gui_extra, juce_audio_basics, juce_audio_formats,
juce_audio_processors, juce_audio_devices, juce_audio_utils, juce_dsp with
`-DJUCE_GLOBAL_MODULE_SETTINGS_INCLUDED=1 -DJUCE_STANDALONE_APPLICATION=1 -DJUCE_MODAL_LOOPS_PERMITTED=1 -DJUCE_WEB_BROWSER=0 -DJUCE_USE_CURL=0 -DJUCE_ALSA=0 -DJUCE_JACK=0`
and freetype flags), generate BinaryData with JUCE's juceaide or a stand-in, then compile PluginProcessor.cpp,
PluginEditor.cpp, tools/Snapshot.cpp and link (plus freetype, fontconfig, pthread, dl, rt). Run under `Xvfb :99`:
`DISPLAY=:99 ./snap outdir [mode]`. Recompile ALL three sources when a header changes (stale objects crash).

## 6. History

v0.4: presets, CHIME, DELAY, OSC 3 NOISE, per-OSC FX routing, FX tabs.
v0.9: rename to GRAIN FORM, per-harmonic SELECT dots, strings design, flat knobs, FILTER 2, filter types, 3rd LFO + LFO lamps,
reverb LOW CUT, peak meter, EVOLVE removed.
v0.91: ADSR amp tab, LFO REMOVE LINKS, filter view, GRANULAR lamp removed (GRAIN per OSC), FX lamp click fix.

## 7. Ideas not started / release to-do

LOCK (Chroma-like harmonic snapping, owner undecided), TUNE (auto root detection for OSC 2), Windows installer (Inno Setup),
Apple notarization (needs the owner's Apple Developer account, $99/yr), pluginval/auval in CI, copy protection / licence keys
(owner undecided), EULA, factory presets, alternative palettes (`Palette::applyTheme`).
