#include "rcp/jit/jit.h"
#include "rcp/rsp.h"
#include "utils/byte_array.h"
#include <array>
#include <chrono>
#include <cstdio>
#include <cstdint>
#include <random>
#include <string>
#include <vector>

#if !defined(N64_RSP_JIT)
#error "rsp_jit_vu_test requires N64_RSP_JIT"
#endif

namespace {

using N64::Rsp::Rsp;
using N64::Rsp::VuReg;
using clock = std::chrono::steady_clock;

struct VuSnap {
    std::array<VuReg, 32> vpr{};
    std::array<int64_t, 8> acc{};
};

uint32_t encode_cop2(int funct, int vd, int vs, int vt, int element) {
    return (0x12u << 26) | (1u << 25) |
           (static_cast<uint32_t>(element & 0xF) << 21) |
           (static_cast<uint32_t>(vt & 0x1F) << 16) |
           (static_cast<uint32_t>(vs & 0x1F) << 11) |
           (static_cast<uint32_t>(vd & 0x1F) << 6) |
           (static_cast<uint32_t>(funct & 0x3F));
}

void fill_regs(Rsp &rsp, std::mt19937_64 &rng) {
    std::uniform_int_distribution<uint16_t> u16;
    for (int i = 0; i < 32; i++)
        for (int l = 0; l < 8; l++)
            rsp.vreg(i).set_lane(l, u16(rng));
    for (int i = 0; i < 8; i++)
        rsp.acc_set(i, static_cast<int64_t>(rng() & 0xFFFFFFFFFFFFULL));
}

void save_vu(const Rsp &rsp, VuSnap &s) {
    for (int i = 0; i < 32; i++)
        s.vpr[static_cast<size_t>(i)] = rsp.vreg(i);
    for (int i = 0; i < 8; i++)
        s.acc[static_cast<size_t>(i)] = rsp.acc_get(i);
}

void load_vu(Rsp &rsp, const VuSnap &s) {
    for (int i = 0; i < 32; i++)
        rsp.vreg(i) = s.vpr[static_cast<size_t>(i)];
    for (int i = 0; i < 8; i++)
        rsp.acc_set(i, s.acc[static_cast<size_t>(i)]);
}

bool same_vu(const VuSnap &a, const VuSnap &b, std::string &why) {
    for (int i = 0; i < 32; i++)
        for (int l = 0; l < 8; l++)
            if (a.vpr[static_cast<size_t>(i)].lane(l) !=
                b.vpr[static_cast<size_t>(i)].lane(l)) {
                why = "v" + std::to_string(i) + "[" + std::to_string(l) + "]";
                return false;
            }
    for (int i = 0; i < 8; i++)
        if (a.acc[static_cast<size_t>(i)] != b.acc[static_cast<size_t>(i)]) {
            why = "acc" + std::to_string(i);
            return false;
        }
    return true;
}

void poke_imem(Rsp &rsp, uint16_t pc, uint32_t inst) {
    Utils::write_to_byte_array32(rsp.get_sp_imem(), pc & 0xFFC, inst);
}

void prepare_run(Rsp &rsp, uint16_t pc) {
    rsp.set_pc(pc);
    *rsp.status_raw_ptr() &= ~1u; // clear halt without kicking RSP thread
}

int correctness() {
    static const int kFuncts[] = {0x00, 0x04, 0x05, 0x06, 0x07, 0x08,
                                  0x0C, 0x0D, 0x0E, 0x0F};
    std::mt19937_64 rng(12345);
    int fails = 0;
    constexpr int kCases = 50;
    Rsp &rsp = N64::g_rsp();

    for (int funct : kFuncts) {
        for (int element = 0; element < 16; ++element) {
            for (int c = 0; c < kCases; ++c) {
                rsp.reset();
                fill_regs(rsp, rng);
                VuSnap initial{};
                save_vu(rsp, initial);

                constexpr int kLen = 8;
                for (int i = 0; i < kLen; ++i) {
                    const int vd = 3 + (i % 5);
                    const int vs = 1 + (i % 4);
                    const int vt = 2 + (i % 3);
                    poke_imem(rsp, static_cast<uint16_t>(i * 4),
                              encode_cop2(funct, vd, vs, vt, element));
                }
                poke_imem(rsp, kLen * 4, 0x0000000D); // BREAK

                prepare_run(rsp, 0);
                for (int i = 0; i < kLen; ++i)
                    rsp.step();
                VuSnap ref{};
                save_vu(rsp, ref);

                load_vu(rsp, initial);
                N64::Rsp::Jit::g_dynarec().reset();
                prepare_run(rsp, 0);
                const int taken = N64::Rsp::Jit::g_dynarec().run(kLen);
                if (taken < kLen) {
                    std::printf("FAIL funct=%02x e=%d: jit ran %d\n", funct,
                                element, taken);
                    if (++fails > 20)
                        return fails;
                    continue;
                }
                VuSnap got{};
                save_vu(rsp, got);

                std::string why;
                if (!same_vu(ref, got, why)) {
                    std::printf("FAIL funct=%02x e=%d case=%d at %s\n", funct,
                                element, c, why.c_str());
                    if (++fails > 20)
                        return fails;
                }
            }
        }
    }
    return fails;
}

template <typename Fn>
double bench_ns(Fn &&fn, int iters) {
    for (int i = 0; i < 200; ++i)
        fn();
    const auto t0 = clock::now();
    for (int i = 0; i < iters; ++i)
        fn();
    const auto t1 = clock::now();
    return std::chrono::duration<double, std::nano>(t1 - t0).count() /
           static_cast<double>(iters);
}

void bench() {
    constexpr int kLen = 16;
    constexpr int kIters = 200000;
    std::mt19937_64 rng(7);
    Rsp &rsp = N64::g_rsp();

    rsp.reset();
    fill_regs(rsp, rng);
    for (int i = 0; i < kLen; ++i)
        poke_imem(rsp, static_cast<uint16_t>(i * 4),
                  encode_cop2(0x0F, 3, 1, 2, 0));
    poke_imem(rsp, kLen * 4, 0x0000000D);

    VuSnap snap{};
    save_vu(rsp, snap);

    const double interp_ns = bench_ns(
        [&]() {
            load_vu(rsp, snap);
            prepare_run(rsp, 0);
            for (int i = 0; i < kLen; ++i)
                rsp.step();
        },
        kIters);

    N64::Rsp::Jit::g_dynarec().reset();
    {
        load_vu(rsp, snap);
        prepare_run(rsp, 0);
        N64::Rsp::Jit::g_dynarec().run(kLen);
    }

    const double jit_ns = bench_ns(
        [&]() {
            load_vu(rsp, snap);
            prepare_run(rsp, 0);
            N64::Rsp::Jit::g_dynarec().run(kLen);
        },
        kIters);

    std::printf("microbench VMADHx%d: interp=%.1fns  jit=%.1fns  speedup=%.2fx\n",
                kLen, interp_ns, jit_ns, interp_ns / jit_ns);
}

} // namespace

int main() {
    const int fails = correctness();
    if (fails) {
        std::printf("rsp_jit_vu_test: %d failures\n", fails);
        return 1;
    }
    std::printf("rsp_jit_vu_test: correctness ok\n");
    bench();
    return 0;
}
