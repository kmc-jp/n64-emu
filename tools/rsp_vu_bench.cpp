#include "rcp/rsp.h"
#include <chrono>
#include <cstdio>
#include <cstdint>
#include <random>
#include <string>
#include <vector>

#if !N64_RSP_SIMD
#error "rsp_vu_bench requires N64_RSP_SIMD=ON"
#endif

namespace {

using N64::Rsp::Rsp;
using clock = std::chrono::steady_clock;

uint32_t encode_vu(int funct, int vd, int vs, int vt, int element) {
    return (static_cast<uint32_t>(element & 0xF) << 21) |
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
    rsp.vco_ref() = static_cast<uint16_t>(rng());
    rsp.vcc_ref() = static_cast<uint16_t>(rng());
}

template <typename Fn>
double bench_ns(Fn &&fn, int iters) {
    for (int i = 0; i < 1000; i++)
        fn();
    const auto t0 = clock::now();
    for (int i = 0; i < iters; i++)
        fn();
    const auto t1 = clock::now();
    const double ns =
        std::chrono::duration<double, std::nano>(t1 - t0).count();
    return ns / static_cast<double>(iters);
}

struct OpGroup {
    const char *name;
    std::vector<int> functs;
};

void run_group(const OpGroup &g, int iters) {
    Rsp rsp;
    rsp.reset();
    std::mt19937_64 rng(42);
    fill_regs(rsp, rng);

    // Rotate through encoded instructions in the group.
    std::vector<uint32_t> insts;
    insts.reserve(g.functs.size() * 16);
    for (int funct : g.functs) {
        for (int element = 0; element < 16; element++)
            insts.push_back(encode_vu(funct, 3, 1, 2, element));
    }

    size_t idx = 0;
    auto next = [&]() {
        const uint32_t inst = insts[idx++ % insts.size()];
        return inst;
    };

    const double scalar_ns = bench_ns(
        [&]() {
            N64::Rsp::vu_execute_compute_scalar(rsp, next());
        },
        iters);

    idx = 0;
    fill_regs(rsp, rng);
    const double eve_ns = bench_ns(
        [&]() {
            N64::Rsp::vu_execute_compute_simd(rsp, next());
        },
        iters);

    const double speedup = scalar_ns / eve_ns;
    std::printf("%-12s  scalar=%6.2fns  eve=%6.2fns  speedup=%5.2fx\n", g.name,
                scalar_ns, eve_ns, speedup);
}

} // namespace

int main() {
    constexpr int kIters = 2'000'000;

    std::printf("rsp_vu_bench (Release recommended, iters=%d)\n", kIters);
    std::printf("N64_RSP_SIMD=1\n\n");

    run_group({"logical", {0x28, 0x29, 0x2A, 0x2B, 0x2C, 0x2D, 0x27}}, kIters);
    run_group({"compare", {0x20, 0x21, 0x22, 0x23, 0x13}}, kIters);
    run_group({"addsub", {0x10, 0x11, 0x14, 0x15}}, kIters);
    run_group({"mulmac",
               {0x00, 0x01, 0x08, 0x09, 0x04, 0x05, 0x06, 0x07, 0x0C, 0x0D, 0x0E,
                0x0F}},
              kIters);

    // Synthetic microcode chain: VMULF -> VMACF -> VADD
    {
        Rsp rsp;
        rsp.reset();
        std::mt19937_64 rng(7);
        fill_regs(rsp, rng);
        const uint32_t i0 = encode_vu(0x00, 3, 1, 2, 0); // VMULF
        const uint32_t i1 = encode_vu(0x08, 4, 3, 2, 0); // VMACF
        const uint32_t i2 = encode_vu(0x10, 5, 4, 1, 0); // VADD

        const double scalar_ns = bench_ns(
            [&]() {
                N64::Rsp::vu_execute_compute_scalar(rsp, i0);
                N64::Rsp::vu_execute_compute_scalar(rsp, i1);
                N64::Rsp::vu_execute_compute_scalar(rsp, i2);
            },
            kIters);
        fill_regs(rsp, rng);
        const double eve_ns = bench_ns(
            [&]() {
                N64::Rsp::vu_execute_compute_simd(rsp, i0);
                N64::Rsp::vu_execute_compute_simd(rsp, i1);
                N64::Rsp::vu_execute_compute_simd(rsp, i2);
            },
            kIters);
        std::printf("%-12s  scalar=%6.2fns  eve=%6.2fns  speedup=%5.2fx\n",
                    "chain3", scalar_ns, eve_ns, scalar_ns / eve_ns);
    }

    return 0;
}
