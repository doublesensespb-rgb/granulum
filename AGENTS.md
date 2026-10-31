# Granulum — guide for AI agents working on this repo

Read this whole file before changing anything. It describes the product, the code, the build
pipeline and how the owner wants to work.

## 1. Product

Granulum is a commercial VST3/AU instrument plugin ("GRANULIZER") by Andrey Ulyanov (Thematic),
a producer and game sound designer. He uses FL Studio on Windows. Goal: a simple-looking plugin
that gives "wow" results through granular synthesis.

Signal flow:

```
OSC 1 (ADD: 16 drawable harmonic bars, optional chord)  ┐
OSC 2 (SAMPLE: dropped WAV, loops when grains are off)  ┴─> GRANULAR (macros) -> FILTER -> DISTORTION -> REVERB -> OUTPUT
```

Every block and both oscillators have an on/off lamp. 2 LFOs with 4-point drawable shapes can be
dragged onto any knob. The plugin opens in INIT: one sine harmonic, everything else off.

## 2. Owner's rules (important)

- The owner does not code. The agent does everything: write code, build, check, deliver files.
  No step-by-step instructions for him unless he asks.
- When he sends change requests, FIRST agree on the changes in text, THEN implement.
- Always give links (to builds, tools, sources).
- He writes in Russian, often with typos or wrong keyboard layout. Reply in Russian.
- Design: teal "deep petrol" flat palette (`Palette` in `PluginEditor.h`), lighter/darker shades only.
  NO violet, NO pink, NO cyberpunk. Keep it simple and clean.
- GRANULAR shows macros only, never raw grain parameters. The GRAIN FIELD screen must look
  beautiful: crystalline shards with glowing trails, NOT bubbles.
- Changing/removing parameter IDs breaks his saved FL projects — tell him when you do it.

## 3. Code map

| File | What it is |
|---|---|
| `Source/GrainEngine.h` | Header-only DSP: additive renderer (`renderAdditive`, 16 harmonics), voices, chords, portamento, grain spawning from macros, grain events for the UI |
| `Source/PluginProcessor.h/.cpp` | Parameters (`ParamID`), modulation matrix (48 slots, `ModTarget` enum), LFOs (4 points, FREE/RETRIG/ENV, RISE), key-tracked filter, `Distortion` (5 types), reverb, INIT, randomize, state save/load |
| `Source/PluginEditor.h/.cpp` | JUCE UI 1080x800 (resizable, fixed aspect): `GranulumLook`, `Knob` + `ModChip`, `GrainField`, `HarmonicEditor`, `ChordPicker` (CallOutBox), `LfoEditor`, `Lamp`, `GranulumEditor` |
| `tools/Snapshot.cpp` | Console app that renders the editor to PNG (CI previews; `themes` mode renders palette variants) |
| `test/engine_test.cpp` | Pure C++ engine tests: `g++ -std=c++17 -O2 -ISource test/engine_test.cpp && ./a.out` |

Key facts:
- Macros: BLOOM, SMEAR, SCATTER, SHIMMER, DRIFT (big); SIZE, SPEED, POSITION, DENSITY, MIX, EVOLVE (small).
  Mapping to grain length/pitch/position/pan is in `GrainEngine::spawnGrain`.
- Chords (OSC 1 only): OFF, MAJ, MIN, MAJ7, MIN7, MAJ9, MIN9, DARK (+1 st), SPACE (+7 st).
- Filter key-tracks from C3 = MIDI 60 (JUCE naming), readout "1.8 kHz  A5".
- Audio thread is lock-free: sample buffers go through `SourceSlot`, LFO tables are double-buffered.
- Audio is processed in 32-sample chunks so modulation is smooth.

## 4. Build pipeline (no local Windows/Mac needed)

`.github/workflows/build.yml` runs on every push to `main`:
1. `windows` job (windows-latest, MSVC): builds VST3 + Standalone + Snapshot, renders `ui-*.png`, zips `Granulum-Windows.zip`.
2. `mac` job (macos-latest, Xcode): universal arm64+x86_64 VST3 + AU + Standalone, ad-hoc codesign, `Granulum-macOS.zip`.
3. `publish` job: force-pushes zips + PNGs to the `builds` branch.

JUCE 8.0.10 is cloned by CI (not committed). Build takes ~10-20 min.

Workflow loop the agent should follow:
1. Edit code, run `test/engine_test.cpp` locally.
2. Optional syntax check with JUCE cloned locally:
   `g++ -std=c++17 -fsyntax-only -IJUCE/modules -DJUCE_GLOBAL_MODULE_SETTINGS_INCLUDED=1 Source/PluginEditor.cpp`
3. Commit and push to `main` (add `[skip ci]` to the message for docs-only changes).
4. Wait for the run: `gh run list -L 1`.
5. Fetch results: `git clone --depth 1 --branch builds https://github.com/doublesensespb-rgb/granulum`.
6. LOOK at the PNG screenshots and fix anything ugly or broken before giving the build to the owner.
7. Send him the zip with a short summary of what changed.

Install on Windows: replace `Granulum.vst3` in `C:\Program Files\Common Files\VST3`, rescan in FL.
Mac: see `README.txt` inside the mac zip (copy to `/Library/Audio/Plug-Ins/...`, remove quarantine with `xattr`).

## 5. Done so far (v0.3)

Two oscillators, 16 wide bars, chord popup with mini keyboard, macro granular engine, lamps for every
block, INIT, per-block RANDOM, key-tracked filter with note readout, 5 distortion types, reverb
MIX/SIZE, POLY/MONO + PORTA, 2 drawable 4-point LFOs with curve handles, FREE/RETRIG/ENV + RISE,
shard GRAIN FIELD, Windows and macOS builds.

Ideas not started: presets, installer, Apple notarization ($99 developer account), pluginval/auval in CI,
alternative palettes (`Palette::applyTheme`, variants 1-4 based on #355872 #7AAACE #9CD5FF #F7F8F0).
