# GRAIN FORM (repo: granulum) — guide for AI agents and developers

Read this whole file before changing anything. It describes the product, the code, the build
pipeline and how the owner wants to work. Current version: **0.99.6** (test build 0.99.7-test on branch `test`) (`project(... VERSION 0.9.9)` + display label `set(GRAINFORM_LABEL "0.99.6")` in CMakeLists.txt).
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
- **After EVERY build, without being asked:** send the versioned zips, the links (builds branch, README-RU.md, AGENTS.md,
  commit history) and a fresh offline archive `GRAIN-FORM-<version>-offline.zip` (single-branch clone of main with history,
  JUCE 8.0.10, both builds, the handbook). The owner insists on this so he can continue without the agent.
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
v0.99 / 0.99.1: OSC order ADDITIVE / SUB / SAMPLE / NOISE (display only; internal osc2 = sample, osc3 = noise, "sub" separate),
SUB routable to blocks via digit 2 (params `<block>R4`, default off, own FxChain `subChain`), SUB 5 sticks (subH1..5, always
layered), OCT -3..+1, SEMI; GRAIN switches row (`HarmonicEditor::rowStyle = 11`), quiet first-run hint; LFO on sticks:
virtual mod targets `harm1..16` (kHarmTarget0 = 1000) and mtSubH1..5, one LFO per stick, wheel = depth, look = style B
(`lfoStickStyle = 1`); a stick with an LFO is rendered as its own layer (`SampleData::layers`, gain ramped per chunk in the
engine); PEAK Q (peakQ/peakQ2) + gain ±36 dB, filter view 0 dB centred; no hand cursors anywhere; UI idle optimisations.
v0.99.7-test (branch `test`, CI zips named GRAIN-FORM-TEST-*): anti-click - computeModulated slews every normalised mod value
(~4 ms, `normSm`, reset on soundReset); Distortion smooths drive/mix per oversampled sample, AGC glides (`agcSm`), type change
crossfades 15 ms; DIST on/off crossfades 20 ms (`distGate`, sources split between own path and the DIST bus; MIX 0 no longer
re-routes); grain cloud norm glides (`Voice::normSm`); CHIME strike low-passed (~2.5 kHz); perc grains 4 ms smoothstep attack.
New: `osc1Air` (mod target mtOsc1Air; `renderAirMips` = harmonics 17..512, read as Params::overlay2 inside engineAdd),
`gshape` (mtShape; Grain fadeR/tailCurve, window2), `freeze` (travel stops), `shimmerMode` FREE/5TH/OCT, grain colour tilt from
DRIFT (Grain::tilt, ~1.2 kHz one-pole). HarmonicEditor pills: pillRect(0) MIX WAVE, pillRect(1) AIR, both accept LFO drops.
GrainSwitch (GRANULAR title row): FREEZE toggle, SHIMMER intervals. chordGain n^-0.6.
Later on `test`: user INIT (`userInitFile`, InitButton right-click menu), preset browser DELETE / RENAME;
master EQ `ParamEq` (5 Simper-SVF bands, params eq{1..5}{On,Type,Freq,Gain,Q,Slope}, mod targets mtEq1Freq.. (Freq/Gain/Q
per band), processed in processChunk before OUTPUT/CLIP; `eqRing` tap only while EqView is showing); EqView + ViewSwitch
(GRAINS | EQ) replace the GRAIN FIELD title; filter types reordered LP12 LP24 LP48 HP12 HP24 HP48 BP PEAK COMB VOWEL
(`filtOrder` 2, older states remapped), BP = HP+LP pair with `bpWidth`/`bpWidth2` (mtBpWidth*), `subTune` (mtSubTune).
Final TEST UI: typed random (`randType` 0..5 ANY/BASS/PAD/PLUCK/CHORD/TEXTURE, `randomizeTyped`, voiced for E4 = the owner's
preset note; saved in Documents/GRAIN FORM/settings.txt, not in presets); `RandomPanel` overlay replaces the PopupMenu of the ▾
button (chips without captions, 2-column ticks = randMask, RANDOM <TYPE> button; main button label follows the type);
`TourOverlay` (9 steps, spotlight + card, `setupTour`) opened by `GuidePill`; `TitleBadge` draws the name (copper sheen every 8 s,
click -> `AboutOverlay`: "Made by producers Thematic & Oddkut" + version); the header version plate is gone.
Snapshot mode `final` renders header / panel / tour / About.
Output: the 0.99.6 tanh soft clipper (knee at -3 dB, 23-35 % THD on bass when pushed) is replaced by `OutLimiter`
(PluginProcessor.h): look-ahead 1.5 ms (min-window + moving average, so gain is down before the peak), 10 ms hold,
150 ms release, ceiling 0.9886 (-0.1 dB) + hard safety clamp; the signal is always delayed (constant latency, reported via
setLatencySamples). Param id stays `clipOn` (name "Output Limiter"), button reads LIMIT and glows with `limitGr`.
Pills MIX WAVE / AIR: LFO chip (`pillChip`, `pillLfo`, `pillChipDrag`) = dot + depth, drag up/down / wheel = depth, right-click removes.
CPU: `Params::maxOverlap` (14 tones, 6 noise, set in voiceParams) caps grains per voice; spawnGrain stretches `spacing`
(by reference) to length/cap, and to 35-100 % of that in RELEASE by env; v.norm follows, so the level holds. Owner's heavy pad
(size 600 ms, density 33/s, bloom .5, smear .4, AIR, pink noise grains, 6 notes): 53 % -> 13 % of a core (rdemo Bench3.cpp).
Snapshot mode `pill`; rdemo Loud.cpp (`loud <note> <outDb>`, `loud sine <inDb> <out.raw>`) measures level and limiter THD.
v0.99.6: `osc1Mode` = ADDITIVE|WAVE (2 choices; states without `modeOrder` map old MIX -> ADD keeping osc1WaveMix, old WAVE -> 1).
MIX WAVE (osc1WaveMix, default 0) only in ADD: drawn as a pill in HarmonicEditor::paintOverChildren (drag, double click = 0); the wave
is read INSIDE engineAdd as `Params::overlay` (same grains, phase mapped by ovScale = wave sr / add sr, one 1024-sample cycle per
mip via readCycle, per-grain MipCache). engineWave only runs in WAVE mode. Grain budget `kGrainBudget = 200` live grains per engine
stretches spacing (anti-crackle). Output CLIP = 4x oversampled soft knee clipper (t 0.708, ceil 0.985), no latch indicator.
OUTPUT -36..+24 dB. randMask bits: filter2 64, lfo1..3 128/256/512 (`randOrder` 2). VOWEL norm 2.4/fq, FilterUnit::resoComp,
chordGain n^-0.8. Noises: 12 files (01_WHITE..12_BUBBLES), `noiseOrder` 3; states with noiseOrder 2 are remapped by name via
noiseIndexFor (removed: BROWN, AC HUM, BUZZ, TRANSFORMER, AIR, LEAVES); noiseOffsetSemis: PINK +5 st. osc3Color range -1..2
(= -12..+24 st). GrainField reads events from engineAdd, engineWave, engineSmp, engineNoise (Shard::src 0/1/2).
v0.99.5: OSC 1 `osc1Mode` ADD|MIX|WAVE + `osc1Wave` + `osc1WaveMix` (mod target mtOsc1WaveMix); WAVE = `engineWave` playing
`renderWaveMips` (period 1024, 512 harmonics, octave mips, per-read mip pick by step -> no aliasing); idle OSC 1 parts are not
rendered. Distortion 4x oversampled (dsp::Oversampling IIR) with auto level (AGC -24..+6 dB) and gentler drive curves. RANDOM ALL
menu (`randMask`, state). Noise list rebuilt: WHITE PINK BROWN BLUE first (tools/make_noises.py --classic / --new), STATIC WIND
BREATH removed; old states (no `noiseOrder` property) are remapped by name. Noises +3 st. INIT / preset without file -> clearSample.
LFO'd sticks draw at their live level (harmLive / modNorm). DIST is ONE shared bus (`distBusChain`, `distBuf`): sources routed
to DIST are summed and distorted together (noise + tone intermodulate like Serum); after it the mix goes to DELAY/REVERB if lit for
any member. Shaper is symmetric, full band (matched to the owner's Serum reference: odd harmonics, noise as bright air), TUBE almost
symmetric, TONE knob removed (fixed open), BITCRUSH replaced by SQUARE (soft comparator); states without `distOrder` map 3 -> HARD CLIP.
Tonal noises AC HUM / BUZZ / TRANSFORMER (make_noises.py --hum). INIT = MONO (param default).
v0.99.4: click fixes (owner's Bass_Synth_03): min attack 4 ms + smoothstep for attacks < 15 ms; a retriggered active
voice crossfades its last output sample out over 5 ms (`Voice::tail*`); OSC/SUB gains go through two one-pole stages
(~1.5 ms each, per chunk, `volSmooth`/`subSmooth`) and snap when the engine has no active voice (`GrainEngine::anyActive`).
v0.99.3: grain field 60 px narrower / OSC column 560 (GRAIN switches had shrunk to dots in 0.99.2); OSC 2/3/4 fold into a
strip (`proc.uiFold[3]`, state property `oscFold`, `FoldButton`, `Knob::setInline`), OSC 1 never folds; preset list = `PresetSaveWindow`
in browse mode (click loads, double-click loads + closes); SPREAD base width 0.85 (works with SCATTER 0, owner re-tunes his presets);
master PITCH +-24 st continuous.
v0.99.2: fix: state save wrote `kModTargetIds[t]` for t >= 1000 (LFO on sticks) -> out-of-bounds read, now `modTargetId(t)`;
TUNE on OSC 1/2 (`osc1Tune`/`osc2Tune`, -48..+48 st, continuous, mod targets mtOsc1Tune/mtOsc2Tune), master PITCH (`pitch`,
-12..+12 st, mtPitch, added to every osc + SUB transpose) next to OUTPUT in the header; whole CLIP box is the button (lit when
on); LFO depth readout in % (`DepthReadout`: knobs show a small
tag next to the LFO chip (the knob value must stay visible - owner), sticks show a pill above the range; wheel 5 % / Shift 1 %,
drag snaps to 1 %); harmonic numbers row under the OSC 1 sticks (`HarmonicEditor::numStyle = 1`, owner's choice A); preset
folders: `PresetSaveWindow` (in-plugin SAVE: folders, + FOLDER, rename/delete-empty, preset list, REPLACE), menu submenus per
folder, `presetFolder` state property, `listPresets()` = root then folders; CHIME hammer fix: the strike scales with the real
grain source level (`SampleData::level/layerLevel`), so all grain sticks down = silence (was phantom bells). RETRIG/ENV LFOs restart on every note-on (was: only when no key held -> dead
LFO on legato MONO notes). Preset change / INIT sets `soundReset`: the audio
thread resets engines + FX chains and glides the last sample to 0 in 128 samples (no tails of the old preset, no click).
Owner rejected the first factory bass presets: none ship.
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
