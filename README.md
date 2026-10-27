# Kinetik KBAS-1

Monophonic acid bass plug-in (VST3 / AU / CLAP, Windows and macOS): a saw / square oscillator into a resonant 4-pole
low pass filter with its own envelope, accents and slides, driven by a 32-step note sequencer.

- Voice: TUNE, WAVE (saw to square), CUTOFF, RESO, ENV MOD, DECAY, ACCENT, SLIDE.
- Colour: VOWEL, GROWL, SUB, HARMONY, SATURATE (tube, tape, diode, fuzz, fold).
- After the filter: AMP DECAY, DRIVE, TONE, a tempo-synced ping-pong delay (TIME, FEEDBACK, MIX) and VOLUME.
- Sequencer: per step note, gate, accent and slide; accents in a row build up; a slide glides into the next note
  without a new attack. Pattern length 1-32 steps, swing, rotate, 16 patterns with copy / paste / clear.
- ACID GEN writes a new random line in the chosen scale and root (8 scales); SNAP keeps dragged notes in the scale.
- MIDI: play it from a keyboard (last note priority, legato notes slide), or KEY TRANSPOSE: keys transpose the playing
  pattern (C3 = as written), or REC: the notes you play are written into the playing pattern (with a click).
- Per-step locks, chance, ratchets, ties, polymetric accent / slide rows, EVOLVE, PUMP, RANDOM, CHAIN.
- Follows the host transport (or plays on its own when the host is stopped); 18 factory presets and your own; undo;
  four skins; resizable window.

Build: `make setup` once (fetches DPF), then `make` (or `make universal` on macOS, or
`make WINDOWS=true CC=x86_64-w64-mingw32-gcc CXX=x86_64-w64-mingw32-g++` from Linux).

Proprietary software — see `LICENSE` and `EULA.txt`. Third-party code: `THIRD_PARTY_NOTICES.txt`.
