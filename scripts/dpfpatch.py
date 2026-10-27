#!/usr/bin/env python3
# Patches applied to DPF (DISTRHO Plugin Framework) for Kinetik DR-16.
# usage: python3 dpfpatch.py <path-to-DPF>
import sys
p = sys.argv[1] + '/distrho/src/DistrhoPluginVST3.cpp'
s = open(p).read()

def rep(a, b):
    global s
    if b in s: return
    assert a in s, a[:60]
    s = s.replace(a, b, 1)

# --- patch 1: ignore stale host values right after a UI edit (Ableton Live) ---
rep("""            fCachedParameterValues = new float[extraParameterCount];
""", """            fCachedParameterValues = new float[extraParameterCount];
            fUiGuardValue = new float[fParameterCount + 1]();
            fUiGuardBlocks = new int32_t[fParameterCount + 1]();
""")
rep("""            delete[] fCachedParameterValues;
            fCachedParameterValues = nullptr;
        }
""", """            delete[] fCachedParameterValues;
            fCachedParameterValues = nullptr;
        }
        delete[] fUiGuardValue; fUiGuardValue = nullptr;
        delete[] fUiGuardBlocks; fUiGuardBlocks = nullptr;
""")
rep("""    float* fCachedParameterValues; // basic offset + real
""", """    float* fCachedParameterValues; // basic offset + real
    // Kinetik patch: some hosts (e.g. Ableton Live) send the processor the value a parameter had *before*
    // an edit made in the plugin UI. Remember UI edits for a while and ignore host values that disagree.
    float* fUiGuardValue = nullptr;
    int32_t* fUiGuardBlocks = nullptr;

    bool _isStaleHostValue(const uint32_t index, const double normalized)
    {
        if (fUiGuardBlocks == nullptr || index >= fParameterCount || fUiGuardBlocks[index] <= 0)
            return false;
        const ParameterRanges& r(fPlugin.getParameterRanges(index));
        const double plain = r.getUnnormalizedValue(normalized);
        const bool stepped = (fPlugin.getParameterHints(index) & (kParameterIsInteger | kParameterIsBoolean)) != 0;
        const double tol = stepped ? 0.5 : (r.max - r.min) * 0.0005 + 1e-6;
        if (std::abs(plain - fUiGuardValue[index]) <= tol) { fUiGuardBlocks[index] = 0; return false; }
        return true;
    }
""")
rep("""            if (! fPlugin.isParameterOutputOrTrigger(index))
                fPlugin.setParameterValue(index, value);

            return v3_cpp_obj(fComponentHandler)->perform_edit(fComponentHandler, rindex, normalized);""",
"""            if (! fPlugin.isParameterOutputOrTrigger(index))
                fPlugin.setParameterValue(index, value);

            if (fUiGuardValue != nullptr)
            {
                fUiGuardValue[index] = value;
                fUiGuardBlocks[index] = 64;
            }

            return v3_cpp_obj(fComponentHandler)->perform_edit(fComponentHandler, rindex, normalized);""")
rep("""                if (offset != 0)
                    continue;

                const uint32_t index = rindex - kVst3InternalParameterCount;
                _setNormalizedPluginParameterValue(index, normalized);""",
"""                if (offset != 0)
                    continue;

                const uint32_t index = rindex - kVst3InternalParameterCount;
                if (_isStaleHostValue(index, normalized))
                    continue;
                _setNormalizedPluginParameterValue(index, normalized);""")
rep("""                if (offset == 0)
                    continue;

                const uint32_t index = rindex - kVst3InternalParameterCount;
                _setNormalizedPluginParameterValue(index, normalized);""",
"""                if (offset == 0)
                    continue;

                const uint32_t index = rindex - kVst3InternalParameterCount;
                if (_isStaleHostValue(index, normalized))
                    continue;
                _setNormalizedPluginParameterValue(index, normalized);""")
rep("""    v3_result process(v3_process_data* const data)
    {
""", """    v3_result process(v3_process_data* const data)
    {
        // Kinetik patch: age the UI-edit guards (see _isStaleHostValue)
        if (fUiGuardBlocks != nullptr)
            for (uint32_t i = 0; i < fParameterCount; ++i)
                if (fUiGuardBlocks[i] > 0) --fUiGuardBlocks[i];
""")
# --- patch 2: extra output groups are VST3 aux buses ---
rep("""                    else
                    {
                        busType = V3_MAIN;
                        flags = busInfo.audio == 0 ? V3_DEFAULT_ACTIVE : 0;
                    }
                    break;""", """                    else
                    {
                        // Kinetik patch: first group = main bus, further groups = aux buses (still active by default)
                        busType = busId == 0 ? V3_MAIN : V3_AUX;
                        flags = busInfo.audio == 0 ? V3_DEFAULT_ACTIVE : 0;
                    }
                    break;""")
open(p, 'w').write(s)
print('DPF patched')

# --- patch 3: host-driven resizing (Ableton Live could shrink but not enlarge the window) ---
# The stock code treated rect->right/bottom as width/height (wrong when the rect has an origin) and,
# with a fixed aspect ratio, always snapped back the dimension the user was dragging.
p2 = sys.argv[1] + '/distrho/src/DistrhoUIVST3.cpp'
u = open(p2).read()
a = """        applyGeometryConstraints(minimumWidth, minimumHeight, keepAspectRatio, rect);
        return V3_TRUE;
    }"""
b = """        // Kinetik patch: work on the size (not the corner coordinates) and follow the edge being dragged
        const int32_t reqW = rect->right - rect->left, reqH = rect->bottom - rect->top;
        int32_t nw = std::max<int32_t>(reqW, static_cast<int32_t>(minimumWidth));
        int32_t nh = std::max<int32_t>(reqH, static_cast<int32_t>(minimumHeight));
        if (keepAspectRatio && minimumWidth != 0 && minimumHeight != 0)
        {
            const double ratio = static_cast<double>(minimumWidth) / static_cast<double>(minimumHeight);
            const double cw = std::max(1.0, static_cast<double>(fUI.getWidth()));
            const double ch = std::max(1.0, static_cast<double>(fUI.getHeight()));
            const double dw = std::abs(reqW - cw) / cw, dh = std::abs(reqH - ch) / ch;
            if (dw >= dh) nh = d_roundToIntPositive(nw / ratio);
            else          nw = d_roundToIntPositive(nh * ratio);
            if (nw < static_cast<int32_t>(minimumWidth) || nh < static_cast<int32_t>(minimumHeight))
            {
                nw = static_cast<int32_t>(minimumWidth);
                nh = static_cast<int32_t>(minimumHeight);
            }
        }
        rect->right = rect->left + nw;
        rect->bottom = rect->top + nh;
        return V3_TRUE;
        (void)applyGeometryConstraints;
    }"""
if b not in u:
    assert a in u, 'checkSizeConstraint not found'
    u = u.replace(a, b, 1)
    open(p2, 'w').write(u)
print('DPF UI patched')
