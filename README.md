# Red Sun — multiband saturation / distortion VST3 with "Sidechain Savior"

A JUCE 8 / CMake project that builds a **64-bit Windows VST3**.

## Build (Windows)

Requirements: Visual Studio 2022 (Desktop C++ workload), CMake 3.22+, Git, internet (JUCE is fetched automatically).

```bat
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
```

The plugin appears at `build\RedSun_artefacts\Release\VST3\Red Sun.vst3`.
Copy that folder to `C:\Program Files\Common Files\VST3\` (needs admin) and rescan in your DAW.

## Features

* 4 bands, Linkwitz-Riley 4th-order crossovers (phase-aligned, sums flat). Drag the crossover handles in the display.
* Per band: Style (11 distortion types), Drive, Level, Mix, Tone (tilt), Dynamics (-compress / +expand the signal feeding the saturator), Mute, Solo.
* Master Input / Drive / Output / Mix, 1x / 2x / 4x oversampling (linear-phase FIR, latency reported to host, dry paths compensated).
* Real-time spectrum analyser (in = grey line, out = red fill), per-band "heat bar", and a transfer-curve inset for the selected band.
* **Drive knobs turn into a red sun**: corona, flaring rays and plasma surface intensify as drive increases.
* **Sidechain Savior** button: the plugin's sidechain input is band-limited to a range you choose (LOW / HIGH, draggable in the display). Whenever
  that range is loud on the sidechain track, the *same range* is ducked on this track (DEPTH, THRESHOLD, ATTACK, RELEASE).
  Everything outside the range is untouched (flat-summing split).
  Route the other track to this plugin's sidechain input in your DAW.
* Procedural space backdrop: nebula, stars, faint detailed red sun, exploded planets. No image assets required.

## Source map

| File | Role |
|---|---|
| `Source/PluginProcessor.*` | Audio engine: crossovers, saturators, oversampling, Sidechain Savior |
| `Source/Dsp.h`, `Shapers.h` | Biquad / LR4 filters, 11 waveshapers |
| `Source/Params.h` | Parameter layout |
| `Source/SunLookAndFeel.h` | Sun knobs, Sidechain Savior button, gray metal widgets |
| `Source/SpaceArt.h` | Procedural background |
| `Source/DisplayComponent.h` | Analyser, bands, sidechain range, transfer curve |
| `Source/PluginEditor.*` | Layout and panels |

## Build without installing anything (GitHub Actions)

1. Create a new GitHub repository and push this folder to it (the `.github/workflows/build.yml` file is included).
2. Open the repo's **Actions** tab. The "Build Red Sun VST3 (Windows)" run starts automatically (or click *Run workflow*).
3. When it finishes (about 10 minutes), open the run and download the **RedSun-VST3-Windows** artifact.
4. Unzip it and copy `Red Sun.vst3` into `C:\Program Files\Common Files\VST3\`.
