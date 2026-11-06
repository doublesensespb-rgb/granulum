# GRAIN FORM (repo: granulum) — guide for AI agents and developers

Read this whole file before changing anything. It describes the product, the code, the build
pipeline and how the owner wants to work. Current version: **0.98** (`project(... VERSION 0.9.8)` in CMakeLists.txt).
The UI shows the version on a small plate next to the title (taken from CMake `GRAINFORM_VERSION`, "0.9.8" -> "v0.98").

## 1. Product

GRAIN FORM, subtitle "SELECTIVE GRANULATION" (formerly Granulum), is a commercial VST3/AU/Standalone
instrument by Andrey Ulyanov (artist name Thematic, manufacturer "Thematic"), a producer and game sound designer.
He uses FL Studio on Windows. Goal: a simple-looking synth that gives "wow" results through granular synthesis,
where you choose WHICH harmonics get granulated (the bass can stay clean and mono).

Signal flow:

```
OSC 1 ADDITIVE (16 drawable harmonics, chords, per-harmonic SELECT dots) ─┐
OSC 2 SAMPLE   (factory sounds via SOUNDS menu / < >, or your own WAV)    ├─ each OSC: GRAIN button -> GRANULAR macros -> own FX chain ─> OUTPUT
OSC 3 NOISE    (10 built-in noises, PITCH knob, no key tracking)          ┘
OSC 4 SUB      (3 drawable sticks: root / octave / octave+fifth, VOL, OCT) ── DRY: no grains, no filter, no FX, added before OUTPUT
Every OSC 1-3 has PAN (equal-power balance after the engine). OSC 4 has no PAN (sub stays centred).
FX chain per OSC = FILTER 1 -> FILTER 2 -> DISTORTION -> DELAY -> REVERB
"1 2 3" digits on every block choose which OSCs it processes (like Serum's routing).
Amp envelope: ADSR tab (with on/off lamp; off = plain gate) in the modulation strip.
```

- Every OSC, filter and FX block has an on/off lamp. The GRANULAR block has NO lamp (removed in 0.91):
  each OSC's GRAIN button decides. Its knobs dim when no active OSC has GRAIN on.
- 3 LFOs (default triangle; 2..16 points: double-click empty = add, double-click/right-click a point = delete) are dragged
  onto any knob. Each LFO tab's lamp switches that LFO off everywhere (routings kept). REMOVE LINKS deletes all routings.
  One RATE knob: note values when SYNC is on, Hz when off (like Serum). INTO GRAINS (per LFO): the grains cut up the
  LFO rhythm on an OSC volume (each grain carries its own slice of the LFO, time-scattered by SCATTER).
- Filter view: small window under CUTOFF/RESO showing the response curve over a live input spectrum;
  drag left/right = CUTOFF, up/down = RESO (or GAIN for PEAK).
- LOOP (GRAIN FIELD title row): OFF / LOOP / PING-PONG / ONE-SHOT, two copper flags on the screen set the region (OSC 2/3).
- OSC 2 factory sounds: Atmo 1-5 are embedded (Resources/samples) and installed once to Documents/GRAIN FORM/Samples/Atmos;
  the SOUNDS button opens a menu by folder + "your own file". Drop or click the sample box for your own WAV.
- UNDO button + Ctrl+Z / Ctrl+Shift+Z (APVTS UndoManager; harmonics and LFO points are not undoable yet).
- Knobs feel like Serum: `FineSlider::baseSensitivity = 300` px for full range on every knob, Shift/Ctrl = 10x finer, double-click = default.
- OSC 1 header has a waveform picker (SINE / SAW / SQUARE / TRIANGLE -> sets the 16 harmonics).
- OSC 2 sample box: x in the top-right corner removes the loaded sample (`clearSample`).
- CLIP box next to the meter is a button: toggles the output clipper (`clipOn`). OUTPUT default 0 dB.
- Division lists (LFO RATE, DELAY) are ordered like Serum: dotted / straight / triplet next to each other; states < 0.9.8 are migrated by name.
- The plugin opens in INIT: one sine harmonic, OSC 2 empty, all GRAIN buttons off, effects off.

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
- Design (NEXT STEP, owner still deciding): a 'real hardware' look based on `docs/design/hardware-reference-gemini.png`
  (take ONLY the style, ignore its layout mistakes): worn black metal faceplate, metal frame, screws, chrome knurled knobs
  with a dark pointer, light-grey plastic key caps, copper lit buttons, LEDs inside DIST/DELAY/REVERB/FILTER tabs, displays
  like an old oscilloscope (thick rounded bezel, curved glass, vignette). Open questions to ask him: screen traces copper
  (current) or teal phosphor like the reference? Analog VU meter with a red CLIP lamp instead of the bar meter?
  A first try exists in code: `Palette::hardware` (off by default; Snapshot mode `hw`), picture `docs/design/hardware-preview-claude-0.98.png`.
  Do not ship it until he approves.
- Design: since 0.92 the shipping skin is **GRAPHITE** (theme 12 in `Palette::applyTheme`): dark graphite, warm white ink,
  copper accent, embedded Inter font (Resources/fonts, OFL). LFO colours: steel blue / sage / ivory (never the copper accent).
  SOUNDS and CHORD buttons are copper with a light glint. Themes 0-14 exist (teal original + previews); the owner picked 12.
  NO violet, NO pink, NO yellow, NO cyberpunk. Flat knobs everywhere. Soft segmented selectors. Keep it simple and clean.
  Every text must stay readable (contrast checked).
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
| `Source/PluginProcessor.h/.cpp` | Parameters (`ParamID`), mod matrix (48 slots, `ModTarget`), 3 LFOs, `FilterUnit` (LP12, LP24, HP, BP, PEAK (index 4, was NOTCH), COMB, VOWEL + `response()` for the filter view), `Distortion`, `Delay`, per-OSC `FxChain`, routing, noises (BinaryData FLAC), presets, INIT/randomize, state save/load + migration |
| `Source/PluginEditor.h/.cpp` | JUCE UI 1080x860 (resizable, fixed aspect): `GranulumLook`, `Knob`, `GrainField`, `HarmonicEditor`, `PeakMeter`, `ChordPicker`, `LfoTab`, `LfoEditor`, `AmpTab`, `AmpEditor`, `FilterScope`, `LoopModeBar`, `SampleMenu`, `WavePicker`, `SubBars`, `FineSlider`, `RouteButtons`, `FxTab`, `PresetBar`, `NoisePicker`, `Lamp`, `GranulumEditor` |
| `tools/Snapshot.cpp` | Console app that renders the editor to PNG (CI previews + design preview modes: v098, hw, themes, grainrow, strings, select, toggles, digits, fxtabs, amp, remove, scope, sampler <wav>, loop, lfo8) |
| `tools/make_noises.py` | Generates the OSC 3 noises (numpy/scipy). Output WAV -> convert to FLAC into `Resources/noises/` |
| `test/engine_test.cpp` | Pure C++ engine tests: `g++ -std=c++17 -O2 -ISource test/engine_test.cpp && ./a.out` |
| `.github/workflows/build.yml` | CI: Windows + macOS builds, publishes zips to the `builds` branch |
| `tools/build-windows.bat` | Local one-click Windows build (Visual Studio 2022 + CMake) |

Key facts:
- Macros: BLOOM, SMEAR, SCATTER, SHIMMER, DRIFT, CHIME (big); SIZE, SPEED, POSITION, DENSITY, SPREAD, MIX (small).
  SPREAD = stereo width of the grain cloud; grains alternate left/right so the cloud is centred on average
  (before 0.98 DRIFT could push the whole cloud into one ear).
  EVOLVE was removed by the owner. Mapping to grain length/pitch/position/pan is in `GrainEngine::spawnGrain`.
- CHIME = per-voice resonator bank, lifted by octaves above ~700 Hz so low chords don't get muddy; stereo detune/pan.
- Chords (OSC 1 only): OFF, MAJ, MIN, MAJ7, MIN7, MAJ9, MIN9, DARK, SPACE.
- OSC 1 SELECT dots (`grainMask`, saved in state): off = that harmonic stays clean & mono (`engineAddClean`).
- OSC 3 noises: AIR, TAPE HISS, VINYL, STATIC, WIND, BREATH, PAPER, DUST, FROST, METAL RAIN (FLAC, embedded via
  `juce_add_binary_data`). The owner does NOT want users to drop their own noises into OSC 3. Noise does not track the note.
- Filters key-track from C3 = MIDI 60; readout "1.8 kHz  A5" (PEAK does not key-track). REVERB has a LOW CUT on the wet signal only.
- Distortion menu: FOLD, TUBE, HARD CLIP, BITCRUSH (menu index -> shape via `menuToShape`); RECTIFY was removed in 0.94.
- State carries `pluginVersion`; `setStateInformation` migrates older states (GRANULAR lamp, OSC 3 COLOR->PITCH, distortion order).
- Presets: XML `.gpreset` files in `Documents/GRAIN FORM/Presets`. UI settings (first-run hint) in userAppData/GRAIN FORM/settings.xml.
- Output: slim L/R peak meter with red zone + CLIP button (LED filled = clipper on).
- INTO GRAINS is gated on the OSC on/off switch, not on the modulated volume (that switch caused clicks at fast LFO rates, fixed 0.98).
- Audio thread is lock-free: sample buffers go through `SourceSlot`, LFO tables are double-buffered. Audio is processed in
  32-sample chunks so modulation is smooth.
- Static design flags (owner-approved, keep): `GranulumLook::flatKnobs=true`, `toggleStyle=5`, `HarmonicEditor::barStyle=5`
  (strings), `rowStyle=2` (dots), `FxTab::style=2`, `RouteButtons::style=3`.

## 4. Build pipeline

`.github/workflows/build.yml` runs ONLY when started manually (`gh workflow run build.yml --ref main`, or GitHub -> Actions ->
Build -> Run workflow). Never start it before the owner has approved all pending changes and said to build.
Inputs: `target` = all | windows | mac, `mac_runner` (default `macos-15`; macos-14 stopped working in Nov 2026; if a mac job is cancelled as 'not acquired', run again with `-f target=mac`).
Example: `gh workflow run build.yml --ref main -f target=all`.
1. `windows` job (windows-latest, MSVC): VST3 + Standalone + Snapshot, renders `ui-*.png`, zips `GRAIN-FORM-Windows.zip`.
2. `mac` job (Xcode): universal arm64+x86_64 VST3 + AU + Standalone, ad-hoc codesign, `GRAIN-FORM-macOS.zip`.
3. `publish` job: updates the `builds` branch, keeping the other platform's zip when only one was built.

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
v0.92: GRAPHITE skin + Inter, bolder granular (SCATTER bursts/holes, SHIMMER pings, CHIME hammer), RANDOM characters,
factory Atmo 1-5 + SOUNDS menu, OSC 3 PITCH, version stamp.
v0.94: LOOP modes, PEAK filter, FOLD first / RECTIFY gone, INTO GRAINS, one RATE knob, UNDO, heavy knobs, copper CHORD.
v0.95: ~2x faster GRAIN FIELD drawing, ~1.7x faster audio (bit-identical), free LFO points (2..16), INIT with empty OSC 2.
v0.96: fixed crash (INIT / preset load with the ADSR tab open read a 4th LFO); ADSR curve follows its knobs at once.
v0.97: FL lag with several instances: Windows uses JUCE's software renderer (not Direct2D), one 30 fps UI tick for all views.
v0.98: OSC 4 SUB (dry), PAN on OSC 1-3, SPREAD, waveform picker, sample clear x, live filter view, Serum-like knob feel,
CLIP toggle, OUTPUT 0 dB, Serum-like division order, INTO GRAINS click fix, LFO rings outside small knobs, version plate.

## 7. Ideas not started / release to-do

Big ideas agreed in principle: RENDER VARIATIONS (batch WAV export for game audio), LOCK (grains/bells in key),
MUTATE + MORPH XY, analysing any sample into harmonic layers (selective granulation for any sound), effect version,
SPLIT handle for the selective feature (testers do not notice the SELECT dots). ECO quality switch if CPU is still an issue.
LOCK (Chroma-like harmonic snapping, owner undecided), TUNE (auto root detection for OSC 2), Windows installer (Inno Setup),
Apple notarization (needs the owner's Apple Developer account, $99/yr), pluginval/auval in CI, copy protection / licence keys
(owner undecided), EULA, factory presets, alternative palettes (`Palette::applyTheme`).

## 8. How this plugin was made (links)

- Built from 0.1 to 0.98 by the owner talking to Claude (Anthropic) in Claude Code / claude.ai sessions; the owner described,
  Claude wrote all code, built in GitHub Actions and delivered zips. Main session:
  https://claude.ai/code/session_01Hux4b6x3M7Dyo9ivmJYTnv (only the owner's account can open it).
- Full change history with explanations: https://github.com/doublesensespb-rgb/granulum/commits/main
  (each version was developed on a branch v09x and merged; old branches stay on GitHub).
- Builds and CI logs: https://github.com/doublesensespb-rgb/granulum/actions
- Design pictures: `docs/design/` (current GRAPHITE 0.98, hardware reference and first hardware try).
- JUCE 8.0.10: https://github.com/juce-framework/JUCE/tree/8.0.10 · docs https://docs.juce.com
