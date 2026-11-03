# Changelog

## 0.7.0

- **RANDOM in one click.** The RANDOM button now does again what is written below it (the last choice); its arrow opens the menu to choose another.
- **Dice**: a die next to VOICE / COLOR (new sound), in the sequencer bar (new line) and after the FX tabs (new settings for the knobs of the tab shown: FX, MOTION, REVERB, CHAOS, LFO).

## 0.6.0

- **Same layout as the other Kinetik plug-ins.** One window size (1180 x 800) and a common bottom bar at the same place in every plug-in: REC / CLICK, RANDOM (a menu), FILL (HOLD), UNDO (shows what it undoes), the plug-in's own buttons, PRESETS (with the preset's name) and SAVE. PAT / PLAY / BPM / SKIN keep the same spot in the header.
- REC / CLICK moved from the header to the bottom bar, with RANDOM (menu: SOUND, PATTERN, EVERYTHING), UNDO, ACID GEN, PRESETS and SAVE.
- Sequencer toolbar: pages, LEN, < >, then DENSITY, SCALE, ROOT, SNAP, TRANSPOSE on the right; taller NOTE row.

## 0.5.1

- **SHIMMER reverb rebuilt.** The octave-up sound now lives inside the reverb's own feedback, crossfaded with the normal tail instead of added on top: no more build-up and crackle when MOD goes past the middle, the reverb keeps the colour of what you send it, and MOD really brings in the shimmering octave above (finer, wider, a little detuned between lines).

## 0.5.0

- **New factory presets**: 15 lines of 32 to 64 steps (4-bar phrases with an answer in the last bar) that use per-step
  locks (cutoff, decay, env mod, growl, vowel), chance, repeats, ties, polymetric accent / slide rows, both LFOs, the
  saturator, the low cut, echo, reverb and CHAOS; some have a second pattern in a CHAIN.
- Every preset plays at about the same loudness (-15 LUFS).

## 0.4.0

- Two LFOs (LFO 1 / LFO 2 pages): WAVE (triangle, sine, square, saw up / down, random), RATE free or synced to the tempo,
  DEPTH (centre = off, left = inverted), DEST (cutoff, resonance, env mod, decay, tune, wave, accent, amp decay, drive,
  tone, echo feedback / mix, volume, vowel, growl, sub, saturate, low cut, its resonance, reverb, pump), TRIG (free or
  restart on each note). The knob an LFO moves shows it on its arc with an L1 / L2 badge, as on the other Kinetik plug-ins.
- LOW CUT + LC RESO (COLOR page): a resonant 12 dB high pass after the filter and SUB; drawn on the filter display.
- REVERB page: the KDSP-16 reverb (ROOM, PLATE, HALL, SPRING, SHIMMER) with send, size, damping, pre-delay and the
  model's MOD.
- CHAOS page: the KDSP-16 beat repeat (chance per bar, size, length, pitch, fade, reverse, mix) + a HOLD pad.
- Patterns up to 64 steps (4 pages). LEN - / + in the sequencer (right-click = 4 steps, drag or scroll the number):
  odd lengths turn against the bar for polyrhythms. A click on a dim step past the end (or Alt+click a step) ends the
  pattern there; a bar marks the end. The < > arrows still shift the steps.
- DENSITY next to ACID GEN / RANDOM: how many steps they fill. Moving it adds or removes notes of the line they just
  made, always in the same order (downbeats last); hand edits are kept; one UNDO per move.
- UNDO in red, as the other Kinetik plug-ins. Two presets: Wobble Acid, Shimmer Chaos.

## 0.3.0

- REC: with the pattern playing, the notes played on a MIDI keyboard are written into it (nearest step; velocity 100+ =
  accent; a note played while the previous one is held = slide; a long note = tied steps). One UNDO per take.
  CLICK: a metronome while recording (on / off).
- SATURATE + SAT TYPE (COLOR page): TUBE, TAPE, DIODE, FUZZ and FOLD saturation after the filter, oversampled,
  level matched.

## 0.2.0

- Per-step locks: right-click a step, then turn any sound knob (filter, delay send, drive...) for that step only.
- RANDOM: a new sound, a new pattern (notes, accents, slides, locks) or both; UNDO restores.
- EVOLVE: the line changes a little each loop (in the scale); EVO RESET brings back the written line every 2-16 loops.
- ACCENT and SLIDE rows with their own lengths (polymetric acid); CHANCE / RATCHET per step; TIE (Shift+click).
- COLOR page: VOWEL (formant filter following the cutoff), GROWL (audio-rate filter FM), SUB, HARMONY (second voice in
  the scale). MOTION page: EVOLVE, EVO RESET, PUMP, PUMP RATE. Echo R TIME (own right-hand time).
- CHAIN of patterns; the 17-32 page makes a 16-step pattern 32 steps long.
- The echo tail no longer stops when DELAY MIX goes back to 0.
- 5 new factory presets.

## 0.1.0

- First version: monophonic acid bass (saw / square, resonant 4-pole filter, envelope, accents, slides), drive, tone,
  ping-pong delay, 32-step note sequencer with 16 patterns, swing, rotate, ACID GEN in 8 scales with note snap, MIDI
  play and key transpose, 13 factory presets, user presets, undo, four skins, updates and licence.
