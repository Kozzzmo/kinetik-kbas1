# Kinetik KBAS-1 — project memory for Claude

Kinetik KBAS-1 is a monophonic acid bass plug-in (VST3 / AU / CLAP, built with DPF and NanoVG): saw / square
oscillator, resonant 4-pole filter, accents, slides, 32-step note sequencer. All DSP is original code. Family:
KDSP-16 (`Kozzzmo/kinetik-kdsp16`), KMSP-6, KFX-8, KJNG-2, KDYN-10; next product KGRN-4 (granular).
Repository: `Kozzzmo/kinetik-kbas1` (public, created 2026-10-27; first built locally in `/home/claude/kinetik-kbas1`, with `dpf` symlinked to the KDSP-16 checkout).

## Source map (`src/`)

| File | Role |
|---|---|
| `Params.hpp` | 32 steps, 16 patterns; host params `pTUNE..pVOLUME` (voice + amp/fx), `pTEMPO pPLAY pPATTERN`, outputs `pOUT_STEP` (1/24 16ths, -1 stopped) `pOUT_GATE pOUT_PATTERN pOUT_NOTE`; `kSavedParams = pOUT_STEP`; mappings `cutoffHz decaySec slideSec ampDecaySec volumeGain`, `noteName` (60 = C3), `paramText`; 8 scales (`kScaleNames`, `kScaleMask`, `inScale`) |
| `Bank.hpp` | `Pattern` (length, swing, per step `on note accent slide`, default note 36 = C1), `Global` (scale, root, transpose mode); text header `KBAS1` (`G`, `P`, `Q` = 32 tokens `<note><x|.><A|-><S|->`); commands `P s o n a l Q G cl cp rot` (`applyCommand` returns the pattern or `kGlobTouched`) |
| `Engine.hpp` | ZDF ladder (tanh feedback), per-sample sequencing with swing, slide = no retrigger + glide, gate 55 % unless sliding, accent (short decay, charge that builds up over accents in a row), live MIDI last-note priority with legato glide, `midiTranspose`, drive (`DriveAA`), tone LP, DC blocker, ping-pong delay synced to the tempo, pattern change at the end of the pattern |
| `KbasPlugin.cpp` | DPF glue (synth, stereo out, MIDI in, unique id `KBA1`, AU `aumu`): params, states `bank` / `cmd` / `skin`, `pv` UI edits (stale host values ignored 1.5 s, as KDSP-16), cmd `key <note> <vel>` = on-screen audition; demo gate = silent 3 s every 45 s |
| `KbasUI.cpp` | 1180x720: header (logo, LENGTH, SWING, PAT, PLAY, BPM, SKIN), voice panel (LCD with the filter curve + 8 knobs), FX panel (7 knobs), patterns A01-A16 + copy / paste / clear, sequencer (pages 1-16 / 17-32, rotate, ACID GEN, scale, root, SNAP, KEY TRANSPOSE, UNDO, PRESETS, SAVE; rows STEP / NOTE (drag = semitones, + / - ends = octave, double-click = root, audition when stopped) / ACCENT / SLIDE; dot = note outside the scale), presets window (factory + user `.kbas`), skins, about/update, licence, tooltips. Generic blocks copied from `KfxUI.cpp` |
| `Presets.hpp` | `acidGen` (ACID GEN) and 13 factory presets (`PB`: `key`, `set`, `line` = "A1!~" tokens, `!` accent, `~` slide) |
| shared (identical in the family, `scripts/check-shared.sh`) | `Skins.hpp`, `Fonts.hpp`, `Dsp.hpp`, `FsUtf8.hpp`, `Updater.*`, `License.hpp`, `LogoMark.hpp`, monocypher, `fonts/OFL-Unbounded.txt` |

User presets: `%APPDATA%\Kinetik\KBAS-1\Presets\*.kbas` (`KBAS1-PRESET 1`, `NAME`, `V <param> <value>`, `BANK` + bank).
View file `KinetikKBAS1-view.txt` ("skin zoom").

## Working with the owner (read first)

- The owner (GitHub **Kozzzmo**) writes in **French**: answer in French. UI, code, comments and README in **English**.
- Git: commit as `Kozzzmo <Kozzzmo@users.noreply.github.com>`. **No `Co-Authored-By` or any Claude attribution**.
- Proprietary code (`LICENSE`); permissive third-party code only, listed in `THIRD_PARTY_NOTICES.txt`.
- No other makers' names, graphics or trade dress in product text (no "303", Roland, Elektron, Torso...), no
  artist-named presets.
- Family look: Orbit logo + product name in Unbounded, skins NEON (default) / MIDNIGHT / RISO / CLASSIC, large text,
  NEON lit steps red, activity LEDs green.
- Licence product string `KBAS-1` (or BUNDLE).

## Build, test

- `make` (dpf in `dpf/`; `./scripts/setup.sh` clones it at the family's pinned revision), Windows:
  `make WINDOWS=true CC=x86_64-w64-mingw32-gcc CXX=x86_64-w64-mingw32-g++`.
- Tests: `tests/engine.cpp` (pitch, gate, slide, accent, resonance, live legato, transpose, pattern change),
  `tests/bank.cpp` (text round trip, commands, presets in their key, ACID GEN in the scale), `tests/license.cpp`,
  `tests/update.cpp`. VST3 validator: 47/47.
- CI workflow (`.github/workflows/build.yml`) adapted from KFX-8 (auval `aumu KBA1 Kntk`); every push to `main` builds and
  publishes the release `v$(cat VERSION)`.
