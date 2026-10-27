// Kinetik KBAS-1 — DPF plugin glue
#include "DistrhoPlugin.hpp"
#include "Engine.hpp"
#include "License.hpp"
#include <atomic>
#include <cmath>
#include <mutex>
#include <memory>

START_NAMESPACE_DISTRHO

using namespace kn;

static constexpr uint32_t kGlobBit = 1u << 16;

class KbasPlugin : public Plugin
{
public:
    KbasPlugin()
        : Plugin(kTotalParams, 0, 3)
    {
        engine.reset(new Engine());
        master.reset(new Bank());
        master->initDefault();
        engine->bank = *master;
        engine->init(getSampleRate());
        for (int i = 0; i < kTotalParams; ++i) { uiVal[i].store(0.f); uiHold[i].store(0); }
        notes.reserve(256);
        licensed.store(lic::findInstalled(kProductCode).valid);
    }

protected:
    const char* getLabel() const override { return "KinetikKBAS1"; }
    const char* getDescription() const override { return "Monophonic acid bass with step sequencer: slides, accents, resonant 4-pole filter"; }
    const char* getMaker() const override { return "Kinetik"; }
    const char* getLicense() const override { return "Proprietary"; }
    uint32_t getVersion() const override { return d_version(KN_VER_MAJOR, KN_VER_MINOR, KN_VER_MICRO); }

    void initAudioPort(bool input, uint32_t index, AudioPort& port) override { port.groupId = kPortGroupStereo; Plugin::initAudioPort(input, index, port); }

    void initParameter(uint32_t index, Parameter& p) override
    {
        static const char* syms[kTotalParams] = {"tune", "wave", "cutoff", "reso", "envmod", "decay", "accent", "slide",
                                                 "ampdec", "drive", "tone", "dly_time", "dly_fb", "dly_mix", "volume",
                                                 "tempo", "play", "pattern", "out_step", "out_gate", "out_pattern", "out_note"};
        static const char* names[kTotalParams] = {"Tune", "Wave", "Cutoff", "Resonance", "Env Mod", "Decay", "Accent", "Slide",
                                                  "Amp Decay", "Drive", "Tone", "Delay Time", "Delay Feedback", "Delay Mix", "Volume",
                                                  "Internal Tempo", "Internal Play", "Pattern", "Play Step", "Gate", "Playing Pattern", "Note"};
        static_assert(sizeof(syms) / sizeof(syms[0]) == kTotalParams && sizeof(names) / sizeof(names[0]) == kTotalParams, "tables");
        p.hints = kParameterIsAutomatable;
        p.name = names[index]; p.shortName = names[index]; p.symbol = syms[index];
        p.ranges.min = 0; p.ranges.max = 127; p.ranges.def = kParamDefaults[index];
        switch (index) {
        case pTEMPO: p.ranges.min = 40; p.ranges.max = 240; p.unit = "BPM"; break;
        case pPLAY: p.ranges.max = 1; p.hints |= kParameterIsBoolean | kParameterIsInteger; break;
        case pPATTERN: p.ranges.max = kPatterns - 1; p.hints |= kParameterIsInteger; break;
        case pOUT_STEP: p.hints = kParameterIsOutput | kParameterIsInteger; p.ranges.min = -1; p.ranges.max = 32768 * Engine::kFine; break;
        case pOUT_GATE: p.hints = kParameterIsOutput | kParameterIsInteger; p.ranges.max = 1; break;
        case pOUT_PATTERN: p.hints = kParameterIsOutput | kParameterIsInteger; p.ranges.max = kPatterns - 1; break;
        case pOUT_NOTE: p.hints = kParameterIsOutput | kParameterIsInteger; break;
        default: break;
        }
    }

    void initState(uint32_t index, State& state) override
    {
        if (index == 0) { state.key = "bank"; state.defaultValue = String(serializeBank(*master).c_str()); state.label = "Patterns"; }
        else if (index == 1) { state.key = "cmd"; state.defaultValue = ""; state.label = "Edit command"; }
        else { state.key = "skin"; state.defaultValue = ""; state.label = "Interface skin"; }
    }

    float getParameterValue(uint32_t index) const override
    {
        switch (index) {
        case pOUT_STEP: return (float)engine->outStep;
        case pOUT_GATE: return (float)engine->outGate;
        case pOUT_PATTERN: return (float)engine->outPattern;
        case pOUT_NOTE: return (float)engine->outNote;
        default: return index < (uint32_t)kTotalParams ? engine->param[index] : 0.f;
        }
    }
    std::atomic<float> uiVal[kTotalParams];
    std::atomic<int32_t> uiHold[kTotalParams];
    void uiEdit(uint32_t index, float value)
    {
        if (index >= (uint32_t)kSavedParams) return;
        uiVal[index].store(value); uiHold[index].store((int32_t)(getSampleRate() * 1.5));
        engine->param[index] = value;
    }
    void setParameterValue(uint32_t index, float value) override
    {
        if (index >= (uint32_t)kSavedParams) return;
        if (uiHold[index].load(std::memory_order_relaxed) > 0) {
            if (std::fabs(value - uiVal[index].load(std::memory_order_relaxed)) > 0.01f) return;
            uiHold[index].store(0);
        }
        engine->param[index] = value;
    }

    String getState(const char* key) const override
    {
        if (std::strcmp(key, "bank") == 0) { std::lock_guard<std::mutex> lk(masterMutex); return String(serializeBank(*master).c_str()); }
        if (std::strcmp(key, "skin") == 0) return skinState;
        return String("");
    }
    void setState(const char* key, const char* value) override
    {
        if (std::strcmp(key, "skin") == 0) { skinState = value ? value : ""; return; }
        if (std::strcmp(key, "bank") == 0) {
            std::lock_guard<std::mutex> lk(masterMutex);
            if (deserializeBank(*master, value)) dirty.store(0xFFFFu | kGlobBit);
            return;
        }
        if (std::strcmp(key, "cmd") == 0 && value && *value) {
            std::lock_guard<std::mutex> lk(masterMutex);
            uint32_t d = 0;
            const char* s = value; std::string line;
            while (*s) {
                const char* e = std::strchr(s, '\n');
                const size_t len = e ? (size_t)(e - s) : std::strlen(s);
                line.assign(s, len);
                if (line.rfind("pv ", 0) == 0) { unsigned i = 0; float v = 0.f; if (std::sscanf(line.c_str() + 3, "%u %f", &i, &v) == 2) uiEdit(i, v); }
                else if (line == "lic") licensed.store(lic::findInstalled(kProductCode).valid);
                else if (line.rfind("key ", 0) == 0) {          // on-screen keyboard: key <note> <velocity> (0 = off)
                    int n = 60, v = 0; std::sscanf(line.c_str() + 4, "%d %d", &n, &v);
                    const uint32_t packed = ((uint32_t)clampi(n, 0, 127) << 8) | (uint32_t)clampi(v, 0, 127);
                    const int w = keyW.load(); if ((w + 1) % 32 != keyR.load()) { keyQ[w] = packed; keyW.store((w + 1) % 32); }
                } else {
                    const int p = applyCommand(*master, line.c_str());
                    if (p == kGlobTouched) d |= kGlobBit; else if (p >= 0) d |= 1u << p;
                }
                s += len; if (*s == '\n') ++s;
            }
            if (d) dirty.fetch_or(d);
        }
    }

    void activate() override { engine->reset(); }
    void sampleRateChanged(double sr) override { engine->init(sr); }

    void run(const float**, float** outputs, uint32_t frames, const MidiEvent* midi, uint32_t midiCount) override
    {
        for (int i = 0; i < kSavedParams; ++i) {
            const int32_t h = uiHold[i].load(std::memory_order_relaxed);
            if (h > 0) uiHold[i].store(h > (int32_t)frames ? h - (int32_t)frames : 0, std::memory_order_relaxed);
        }
        if (dirty.load(std::memory_order_acquire)) {
            std::unique_lock<std::mutex> lk(masterMutex, std::try_to_lock);
            if (lk.owns_lock()) {
                const uint32_t d = dirty.exchange(0);
                for (int p = 0; p < kPatterns; ++p) if (d & (1u << p)) engine->bank.patterns[p] = master->patterns[p];
                if (d & kGlobBit) engine->bank.glob = master->glob;
            }
        }
        notes.clear();
        while (keyR.load() != keyW.load() && notes.size() < 256) {
            const int r = keyR.load(); const uint32_t k = keyQ[r]; keyR.store((r + 1) % 32);
            notes.push_back({0, (int)(k >> 8), (int)(k & 127), (k & 127) > 0});
        }
        for (uint32_t i = 0; i < midiCount && notes.size() < 256; ++i) {
            const MidiEvent& ev = midi[i];
            if (ev.size < 3) continue;
            const uint8_t st = ev.data[0] & 0xF0;
            if (st == 0x90 && ev.data[2] > 0) notes.push_back({ev.frame, ev.data[1], ev.data[2], true});
            else if (st == 0x80 || (st == 0x90 && ev.data[2] == 0)) notes.push_back({ev.frame, ev.data[1], 0, false});
        }
        Transport tp;
        const TimePosition& pos = getTimePosition();
        if (pos.bbt.valid) {
            tp.hostValid = true; tp.hostPlaying = pos.playing;
            const double bt = pos.bbt.beatType > 0 ? pos.bbt.beatType : 4.0;
            const double beats = (pos.bbt.bar - 1) * (double)pos.bbt.beatsPerBar + (pos.bbt.beat - 1) + (pos.bbt.ticksPerBeat > 0 ? pos.bbt.tick / pos.bbt.ticksPerBeat : 0.0);
            tp.beat = beats * (4.0 / bt); tp.bpm = pos.bbt.beatsPerMinute > 0 ? pos.bbt.beatsPerMinute : 120.0;
        }
        engine->process(outputs[0], outputs[1], frames, tp, notes.data(), (uint32_t)notes.size());
        if (lic::kDemoModeEnabled && !licensed.load(std::memory_order_relaxed)) demoGate(outputs, frames);
    }
    void demoGate(float** outputs, uint32_t frames)
    {
        const double sr = getSampleRate();
        const uint64_t period = (uint64_t)(45.0 * sr), mute = (uint64_t)(3.0 * sr);
        const float coef = 1.f - std::exp(-1.f / (0.01f * (float)sr));
        for (uint32_t i = 0; i < frames; ++i) {
            const float target = (demoPos % period) >= period - mute ? 0.f : 1.f;
            demoGain += (target - demoGain) * coef; ++demoPos;
            if (demoGain < 0.9999f) { outputs[0][i] *= demoGain; outputs[1][i] *= demoGain; }
        }
    }

private:
    std::unique_ptr<Engine> engine;
    std::unique_ptr<Bank> master;
    mutable std::mutex masterMutex;
    std::atomic<uint32_t> dirty{0};
    std::atomic<bool> licensed{false};
    uint32_t keyQ[32] = {}; std::atomic<int> keyW{0}, keyR{0};
    std::vector<NoteIn> notes;
    uint64_t demoPos = 0; float demoGain = 1.f;
    static constexpr const char* kProductCode = "KBAS-1";
    String skinState;
    DISTRHO_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(KbasPlugin)
};

Plugin* createPlugin() { return new KbasPlugin(); }

END_NAMESPACE_DISTRHO
