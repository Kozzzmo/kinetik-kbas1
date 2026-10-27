// Update manifest: signature, product, version order, tampering, SHA-512 (no network)
#include "../src/Updater.hpp"
#include <cstdio>
extern "C" {
#include "../src/third_party/monocypher/monocypher-ed25519.h"
}
using namespace kn;
static int fails = 0;
#define CHECK(c, m) do { if (!(c)) { std::printf("FAIL %s\n", m); ++fails; } else std::printf("ok   %s\n", m); } while (0)

static std::string sign(const std::string& body, const uint8_t* sk64)
{
    uint8_t sig[64];
    crypto_ed25519_sign(sig, sk64, (const uint8_t*)body.data(), body.size());
    static const char* hx = "0123456789abcdef";
    std::string s;
    for (uint8_t b : sig) { s += hx[b >> 4]; s += hx[b & 15]; }
    return body + "signature: " + s + "\n";
}

int main()
{
    uint8_t seed[32], sk[64], pk[32];
    for (int i = 0; i < 32; ++i) seed[i] = (uint8_t)(100 + i);
    crypto_ed25519_key_pair(sk, pk, seed);
    const std::string body = "KINETIK-UPDATE 1\nproduct: KDSP-16\nversion: 0.9\nnotes: new things\n"
                             "page: https://github.com/Kozzzmo/kinetik-kdsp16/releases/latest\n"
                             "mac: https://github.com/Kozzzmo/kinetik-kdsp16/releases/download/v0.9/a.pkg\nmac-sha512: aa\n"
                             "win: https://github.com/Kozzzmo/kinetik-kdsp16/releases/download/v0.9/a.exe\nwin-sha512: bb\n";
    const std::string good = sign(body, sk);
    upd::Manifest m = upd::parse(good, "KDSP-16", pk);
    CHECK(m.valid && m.version == "0.9" && m.notes == "new things", "signed manifest accepted");
    CHECK(!upd::parse(good, "KMSP-6", pk).valid, "other product refused");
    std::string bad = good; bad.replace(bad.find("0.9"), 3, "9.9");
    CHECK(!upd::parse(bad, "KDSP-16", pk).valid, "tampered manifest refused");
    CHECK(!upd::parse(good, "KDSP-16").valid, "manifest signed by another key refused");
    std::string evil = sign("KINETIK-UPDATE 1\nproduct: KDSP-16\nversion: 1.0\nwin: http://evil.example/x.exe\nwin-sha512: cc\nmac: http://evil.example/x.pkg\nmac-sha512: cc\n", sk);
    upd::Manifest e = upd::parse(evil, "KDSP-16", pk, "win");
    CHECK(e.valid && e.url.empty(), "installer outside github.com/Kozzzmo ignored");
    CHECK(upd::parse(good, "KDSP-16", pk, "win").url.find("a.exe") != std::string::npos
          && upd::parse(good, "KDSP-16", pk, "mac").sha512 == "aa", "installer picked per system");
    CHECK(upd::newer("0.10", "0.9") && upd::newer("0.9", "0.8.1") && !upd::newer("0.8", "0.8") && !upd::newer("0.7.9", "0.8"), "version order");
    // SHA-512("abc")
    CHECK(upd::sha512Hex("abc").rfind("ddaf35a193617aba", 0) == 0, "SHA-512");
    std::printf(fails ? "%d FAILED\n" : "all passed\n", fails);
    return fails;
}
