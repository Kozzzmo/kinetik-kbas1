// Kinetik KBAS-1 — what the UI reaches directly in the plug-in (same process)
#pragma once

namespace kn {

struct BassDirect {
    virtual ~BassDirect() {}
    // notes played on a MIDI keyboard while REC is armed and the sequencer plays:
    // kind 0 = note (step, accent index, note, accent), 1 = slide (index), 2 = TIE (step)
    virtual bool popRecorded(int& kind, int& pattern, int& a, int& b, int& note, bool& accent) = 0;
    virtual void setRecording(bool on) = 0;
    virtual void setClick(bool on) = 0;     // metronome while recording
};

} // namespace kn
