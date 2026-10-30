# Changelog

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
