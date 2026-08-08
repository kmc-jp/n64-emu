#include "rcp/vu_profile.h"
#include "utils/log.h"
#include <algorithm>
#include <array>
#include <cstdlib>
#include <utility>
#include <vector>

namespace N64 {
namespace Rsp {

namespace {

constexpr const char *kFunctNames[64] = {
    "VMULF", "VMULU", "VRNDP", "VMULQ", "VMUDL", "VMUDM", "VMUDN", "VMUDH",
    "VMACF", "VMACU", "VRNDN", "VMACQ", "VMADL", "VMADM", "VMADN", "VMADH",
    "VADD",  "VSUB",  "VSUT",  "VABS",  "VADDC", "VSUBC", "VADDB", "VSUBB",
    "VACCB", "VSUCB", "VSAD",  "VSAC",  "VSUM",  "VSAR",  "?",     "?",
    "VLT",   "VEQ",   "VNE",   "VGE",   "VCL",   "VCH",   "VCR",   "VMRG",
    "VAND",  "VNAND", "VOR",   "VNOR",  "VXOR",  "VNXOR", "?",     "?",
    "VRCP",  "VRCPL", "VRCPH", "VMOV",  "VRSQ",  "VRSQL", "VRSQH", "VNOP",
    "VEXTT", "VEXTQ", "VEXTN", "?",     "VINST", "VINSQ", "VINSN", "?",
};

constexpr const char *kLoadNames[16] = {
    "LBV", "LSV", "LLV", "LDV", "LQV", "LRV", "LPV", "LUV",
    "LHV", "LFV", "?",   "LTV", "?",   "?",   "?",   "?",
};

constexpr const char *kStoreNames[16] = {
    "SBV", "SSV", "SLV", "SDV", "SQV", "SRV", "SPV", "SUV",
    "SHV", "SFV", "SWV", "STV", "?",   "?",   "?",   "?",
};

struct State {
    bool enabled{false};
    bool inited{false};
    std::array<uint64_t, 64> compute{};
    std::array<uint64_t, 64> compute_scalar{};
    std::array<uint64_t, 16> lwc2{};
    std::array<uint64_t, 16> swc2{};
    std::array<uint64_t, 8> cop2_move{}; // by rs sub
    uint64_t total_compute{0};
};

State &state() {
    static State s;
    if (!s.inited) {
        s.inited = true;
        const char *e = std::getenv("N64_PROFILE_VU");
        s.enabled = e && e[0] != '\0' && e[0] != '0';
    }
    return s;
}

} // namespace

bool vu_profile_enabled() { return state().enabled; }

void vu_profile_compute(uint32_t inst, bool used_simd) {
    auto &s = state();
    if (!s.enabled)
        return;
    const int funct = inst & 0x3F;
    s.compute[static_cast<size_t>(funct)]++;
    s.total_compute++;
    if (!used_simd)
        s.compute_scalar[static_cast<size_t>(funct)]++;
}

void vu_profile_lwc2(uint32_t inst) {
    auto &s = state();
    if (!s.enabled)
        return;
    const int opcode = (inst >> 11) & 0x1F;
    if (opcode < 16)
        s.lwc2[static_cast<size_t>(opcode)]++;
}

void vu_profile_swc2(uint32_t inst) {
    auto &s = state();
    if (!s.enabled)
        return;
    const int opcode = (inst >> 11) & 0x1F;
    if (opcode < 16)
        s.swc2[static_cast<size_t>(opcode)]++;
}

void vu_profile_cop2_move(uint8_t sub) {
    auto &s = state();
    if (!s.enabled)
        return;
    if (sub < 8)
        s.cop2_move[sub]++;
}

void vu_profile_dump() {
    auto &s = state();
    if (!s.enabled)
        return;

    std::vector<std::pair<uint64_t, int>> ranked;
    ranked.reserve(64);
    for (int i = 0; i < 64; i++)
        if (s.compute[static_cast<size_t>(i)])
            ranked.emplace_back(s.compute[static_cast<size_t>(i)], i);
    std::sort(ranked.begin(), ranked.end(),
              [](auto &a, auto &b) { return a.first > b.first; });

    Utils::info("VU profile: compute ops total={}", s.total_compute);
    uint64_t shown = 0;
    for (size_t n = 0; n < ranked.size() && n < 20; n++) {
        const int f = ranked[n].second;
        const uint64_t c = ranked[n].first;
        const uint64_t sc = s.compute_scalar[static_cast<size_t>(f)];
        const double pct =
            s.total_compute ? 100.0 * static_cast<double>(c) /
                                  static_cast<double>(s.total_compute)
                            : 0.0;
        Utils::info("  [{:>2}] {:<6} count={} ({:.1f}%) scalar_fallback={}", n,
                    kFunctNames[f], c, pct, sc);
        shown += c;
    }

    uint64_t lwc_total = 0;
    for (auto c : s.lwc2)
        lwc_total += c;
    if (lwc_total) {
        Utils::info("VU profile: LWC2 total={}", lwc_total);
        std::vector<std::pair<uint64_t, int>> lr;
        for (int i = 0; i < 16; i++)
            if (s.lwc2[static_cast<size_t>(i)])
                lr.emplace_back(s.lwc2[static_cast<size_t>(i)], i);
        std::sort(lr.begin(), lr.end(),
                  [](auto &a, auto &b) { return a.first > b.first; });
        for (size_t n = 0; n < lr.size() && n < 10; n++) {
            const int o = lr[n].second;
            Utils::info("  LWC2 {:<4} count={}", kLoadNames[o], lr[n].first);
        }
    }

    uint64_t swc_total = 0;
    for (auto c : s.swc2)
        swc_total += c;
    if (swc_total) {
        Utils::info("VU profile: SWC2 total={}", swc_total);
        std::vector<std::pair<uint64_t, int>> sr;
        for (int i = 0; i < 16; i++)
            if (s.swc2[static_cast<size_t>(i)])
                sr.emplace_back(s.swc2[static_cast<size_t>(i)], i);
        std::sort(sr.begin(), sr.end(),
                  [](auto &a, auto &b) { return a.first > b.first; });
        for (size_t n = 0; n < sr.size() && n < 10; n++) {
            const int o = sr[n].second;
            Utils::info("  SWC2 {:<4} count={}", kStoreNames[o], sr[n].first);
        }
    }

    static const char *move_names[] = {"MFC2", "?", "CFC2", "?",
                                       "MTC2", "?", "CTC2", "?"};
    for (int i = 0; i < 8; i++) {
        if (s.cop2_move[static_cast<size_t>(i)])
            Utils::info("  COP2 move {} count={}", move_names[i],
                        s.cop2_move[static_cast<size_t>(i)]);
    }
}

} // namespace Rsp
} // namespace N64
