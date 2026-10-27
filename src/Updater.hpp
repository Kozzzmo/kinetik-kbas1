// Kinetik — update check (shared by every Kinetik DPF product; keep identical in all repositories)
//
// Once a day at most, a background thread downloads a small text manifest from the product's latest GitHub release:
//
//   KINETIK-UPDATE 1
//   product: KDSP-16
//   version: 0.9
//   notes: one line of what is new
//   page: https://github.com/Kozzzmo/kinetik-kdsp16/releases/latest
//   mac: <installer URL>
//   mac-sha512: <128 hex>
//   win: <installer URL>
//   win-sha512: <128 hex>
//   signature: <128 hex>        Ed25519 over every line before it (lines trimmed, ended by '\n')
//
// The manifest is refused unless its signature matches the Kinetik update key (not the licence key) and the product
// matches. An installer is opened only if its SHA-512 matches the signed manifest, so a compromised download host
// cannot make anyone install something else. Older or equal versions are never offered (no downgrade).
// Downloads use the system's curl (macOS: /usr/bin/curl, Windows 10+: System32\curl.exe); no data is sent.
//
// Automatic install (setting "autoinstall", on by default): when a check finds a new version, the verified installer
// runs silently (Windows: NSIS /S, one UAC prompt; macOS: /usr/sbin/installer through one administrator prompt).
// The plug-in already loaded keeps running; the new version is used the next time the host loads it. A lock file
// stops several open windows from installing at once, and "installed-<product>" remembers what was installed.
#pragma once
#include "License.hpp"
#include <atomic>
#include <thread>
#include <mutex>
#include <string>
#include <cstdlib>
#include <memory>

extern "C" {
#include "third_party/monocypher/monocypher.h"
}

namespace kn {
namespace upd {

// public key of the Kinetik update signer (its private key is the KINETIK_UPDATE_KEY secret of the CI)
static const uint8_t kUpdateKey[32] = {
    0x59, 0x1d, 0xdc, 0xfe, 0xd6, 0x59, 0xcf, 0xeb, 0x44, 0x1e, 0x69, 0x1f, 0x8b, 0xa6, 0xc6, 0xdc,
    0x93, 0xe7, 0xef, 0x76, 0x13, 0x32, 0x25, 0x6f, 0xe1, 0xcf, 0xf1, 0x9d, 0x96, 0x0b, 0x7d, 0xfa};

struct Manifest {
    bool valid = false;
    std::string product, version, notes, page, url, sha512;   // url / sha512: installer for this system
};

// "0.10.2" > "0.9" ; missing parts count as 0
inline bool newer(const std::string& a, const std::string& b)
{
    const char* pa = a.c_str(); const char* pb = b.c_str();
    while (*pa || *pb) {
        const long x = std::strtol(pa, (char**)&pa, 10), y = std::strtol(pb, (char**)&pb, 10);
        if (x != y) return x > y;
        if (*pa == '.') ++pa;
        if (*pb == '.') ++pb;
        if ((*pa && !(*pa >= '0' && *pa <= '9')) || (*pb && !(*pb >= '0' && *pb <= '9'))) break;
    }
    return false;
}

#if defined(_WIN32)
static const char* const kThisOs = "win";
#elif defined(__APPLE__)
static const char* const kThisOs = "mac";
#else
static const char* const kThisOs = "";
#endif

inline Manifest parse(const std::string& text, const char* product, const uint8_t* key = kUpdateKey,
                      const char* os = kThisOs)
{
    Manifest m;
    std::string signedPart, sigHex, mac, macSha, win, winSha;
    bool header = false;
    size_t pos = 0;
    while (pos <= text.size()) {
        size_t e = text.find('\n', pos);
        if (e == std::string::npos) e = text.size();
        const std::string line = lic::trim(text.substr(pos, e - pos));
        pos = e + 1;
        if (line.empty()) continue;
        if (!header) {
            if (line != "KINETIK-UPDATE 1") continue;
            header = true;
            signedPart = line + "\n";
            continue;
        }
        const size_t c = line.find(':');
        if (c == std::string::npos) continue;
        const std::string k = lic::trim(line.substr(0, c)), v = lic::trim(line.substr(c + 1));
        if (k == "signature") { sigHex = v; break; }
        signedPart += k + ": " + v + "\n";
        if (k == "product") m.product = v;
        else if (k == "version") m.version = v;
        else if (k == "notes") m.notes = v;
        else if (k == "page") m.page = v;
        else if (k == "mac") mac = v;
        else if (k == "mac-sha512") macSha = v;
        else if (k == "win") win = v;
        else if (k == "win-sha512") winSha = v;
    }
    uint8_t sig[64];
    if (!header || !lic::fromHex(sigHex, sig, 64)) return m;
    if (crypto_ed25519_check(sig, key, (const uint8_t*)signedPart.data(), signedPart.size()) != 0) return m;
    if (m.product != product || m.version.empty()) return m;
    if (std::string(os) == "win") { m.url = win; m.sha512 = winSha; }
    else if (std::string(os) == "mac") { m.url = mac; m.sha512 = macSha; }
    // only https downloads from the expected host
    if (!m.url.empty() && m.url.rfind("https://github.com/Kozzzmo/", 0) != 0) { m.url.clear(); m.sha512.clear(); }
    if (!m.page.empty() && m.page.rfind("https://", 0) != 0) m.page.clear();
    m.valid = true;
    return m;
}

inline std::string sha512Hex(const std::string& data)
{
    uint8_t h[64];
    crypto_sha512_ctx ctx;
    crypto_sha512_init(&ctx);
    crypto_sha512_update(&ctx, (const uint8_t*)data.data(), data.size());
    crypto_sha512_final(&ctx, h);
    static const char* hx = "0123456789abcdef";
    std::string s;
    for (uint8_t b : h) { s += hx[b >> 4]; s += hx[b & 15]; }
    return s;
}

// ---- system side (Updater.cpp)
bool download(const std::string& url, const std::string& path, int timeoutSec);
bool openPage(const std::string& url);
bool openFolder(const std::string& path);   // shows a local folder in Explorer / Finder / the file manager
int runInstallerSilently(const std::string& path);   // 0 = installed; waits for the end (up to 15 min)
// an older copy of this plug-in in the per-user plug-in folder, which hosts may load instead of the installed one
// ("" if none). bin = file name without extension, e.g. "KinetikKMSP6".
std::string strayCopy(const char* bin);

// ---- the checker: owned by the UI, all network / file work on its own thread. The thread only touches a shared
// state block, so closing the window during a download or an install never waits for it (the thread is detached).
class Checker {
public:
    enum State { stIdle, stChecking, stAvailable, stUpToDate, stDownloading, stOpened, stFailed, stInstalling, stInstalled };

    Checker(const char* product, const char* currentVersion, const char* manifestUrl) : sh(std::make_shared<Shared>())
    { sh->prod = product; sh->current = currentVersion; sh->url = manifestUrl; }
    ~Checker();

    void check(bool force);          // automatic check (at most every 20 h, if enabled) or forced
    void install();                  // download the installer, verify it, install it silently (or open it / the page)
    void skipThisVersion();
    bool autoCheck() const;
    void setAutoCheck(bool on);
    bool autoInstall() const;
    void setAutoInstall(bool on);

    State getState() const { return (State)sh->state.load(); }
    Manifest manifest() const { return sh->manifest(); }
    std::string message() const { std::lock_guard<std::mutex> lk(sh->mx); return sh->msg; }
    const char* currentVersion() const { return sh->current; }

    struct Shared {
        const char* prod = ""; const char* current = ""; const char* url = "";   // string literals
        std::atomic<int> state{stIdle};
        std::atomic<bool> busy{false};
        mutable std::mutex mx;
        Manifest man;
        std::string msg;
        Manifest manifest() const { std::lock_guard<std::mutex> lk(mx); return man; }
        void setMsg(const char* m) { std::lock_guard<std::mutex> lk(mx); msg = m; }
    };

private:
    void startWork(void (*fn)(Shared*, bool), bool arg);
    static void doCheck(Shared* c, bool force);
    static void doInstall(Shared* c, bool automatic);

    std::shared_ptr<Shared> sh;
    std::thread worker;
};

} // namespace upd
} // namespace kn
