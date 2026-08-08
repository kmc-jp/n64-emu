#include "rcp/rsp.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <random>
#include <string>
#include <vector>

#if !N64_RSP_SIMD
#error "rsp_vu_diff_test requires N64_RSP_SIMD=ON"
#endif

#if defined(N64_RSP_JIT)
#include "rcp/jit/vu_sse.h"
#endif

namespace {

using N64::Rsp::Rsp;
using N64::Rsp::VuReg;

uint32_t encode_vu(int funct, int vd, int vs, int vt, int element) {
    // COP2 encode: element in bits 21..24, vt 16..20, vs 11..15, vd 6..10,
    // funct 0..5
    return (static_cast<uint32_t>(element & 0xF) << 21) |
           (static_cast<uint32_t>(vt & 0x1F) << 16) |
           (static_cast<uint32_t>(vs & 0x1F) << 11) |
           (static_cast<uint32_t>(vd & 0x1F) << 6) |
           (static_cast<uint32_t>(funct & 0x3F));
}

void randomize_vu_state(Rsp &rsp, std::mt19937_64 &rng) {
    std::uniform_int_distribution<uint16_t> u16;
    std::uniform_int_distribution<int64_t> acc_dist(-0x7FFFFFFFFFFFLL,
                                                    0x7FFFFFFFFFFFLL);
    for (int i = 0; i < 32; i++) {
        for (int l = 0; l < 8; l++)
            rsp.vreg(i).set_lane(l, u16(rng));
    }
    for (int i = 0; i < 8; i++)
        rsp.acc_set(i, acc_dist(rng));
    rsp.vcc_ref() = u16(rng);
    rsp.vco_ref() = u16(rng);
    rsp.vce_ref() = static_cast<uint8_t>(u16(rng) & 0xFF);
    rsp.divin_ref() = static_cast<int16_t>(u16(rng));
    rsp.divout_ref() = static_cast<int16_t>(u16(rng));
    rsp.divin_loaded_ref() = (rng() & 1) != 0;
}

void copy_vu_state(Rsp &dst, const Rsp &src) {
    for (int i = 0; i < 32; i++)
        dst.vreg(i) = src.vreg(i);
    for (int i = 0; i < 8; i++)
        dst.acc_set(i, src.acc_get(i));
    dst.vcc_ref() = src.vcc_ref();
    dst.vco_ref() = src.vco_ref();
    dst.vce_ref() = src.vce_ref();
    dst.divin_ref() = src.divin_ref();
    dst.divout_ref() = src.divout_ref();
    dst.divin_loaded_ref() = src.divin_loaded_ref();
}

bool same_vu_state(const Rsp &a, const Rsp &b, int vd, std::string &why) {
    for (int l = 0; l < 8; l++) {
        if (a.vreg(vd).lane(l) != b.vreg(vd).lane(l)) {
            why = "vd lane " + std::to_string(l);
            return false;
        }
    }
    for (int i = 0; i < 8; i++) {
        if (a.acc_get(i) != b.acc_get(i)) {
            why = "acc " + std::to_string(i);
            return false;
        }
    }
    if (a.vcc_ref() != b.vcc_ref()) {
        why = "vcc";
        return false;
    }
    if (a.vco_ref() != b.vco_ref()) {
        why = "vco";
        return false;
    }
    if (a.vce_ref() != b.vce_ref()) {
        why = "vce";
        return false;
    }
    if (a.divin_ref() != b.divin_ref()) {
        why = "divin";
        return false;
    }
    if (a.divout_ref() != b.divout_ref()) {
        why = "divout";
        return false;
    }
    if (a.divin_loaded_ref() != b.divin_loaded_ref()) {
        why = "divin_loaded";
        return false;
    }
    return true;
}

const int kSimdFuncts[] = {
    0x00, 0x01, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0C, 0x0D,
    0x0E, 0x0F, 0x10, 0x11, 0x13, 0x14, 0x15, 0x1D, 0x20, 0x21,
    0x22, 0x23, 0x25, 0x27, 0x28, 0x29, 0x2A, 0x2B, 0x2C, 0x2D,
    0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36,
};

} // namespace

int main() {
    std::mt19937_64 rng(0x52535053494dull);

    Rsp scalar;
    Rsp simd;
    scalar.reset();
    simd.reset();

    constexpr int kCasesPerOp = 4000;
    int failures = 0;

    for (int funct : kSimdFuncts) {
        for (int c = 0; c < kCasesPerOp; c++) {
            randomize_vu_state(scalar, rng);
            copy_vu_state(simd, scalar);

            const int vd = static_cast<int>(rng() % 32);
            const int vs = static_cast<int>(rng() % 32);
            const int vt = static_cast<int>(rng() % 32);
            const int element = static_cast<int>(rng() % 16);
            const uint32_t inst = encode_vu(funct, vd, vs, vt, element);

            N64::Rsp::vu_execute_compute_scalar(scalar, inst);
            N64::Rsp::vu_execute_compute_simd(simd, inst);

            std::string why;
            if (!same_vu_state(scalar, simd, vd, why)) {
                std::fprintf(stderr,
                             "mismatch funct=%#x case=%d vd=%d vs=%d vt=%d "
                             "elem=%d (%s) vcc=%04x/%04x vco=%04x/%04x\n",
                             funct, c, vd, vs, vt, element, why.c_str(),
                             scalar.vcc_ref(), simd.vcc_ref(), scalar.vco_ref(),
                             simd.vco_ref());
                for (int l = 0; l < 8; l++) {
                    std::fprintf(
                        stderr, "  lane%d vd=%04x/%04x acc=%llx/%llx\n", l,
                        scalar.vreg(vd).lane(l), simd.vreg(vd).lane(l),
                        static_cast<unsigned long long>(scalar.acc_get(l)),
                        static_cast<unsigned long long>(simd.acc_get(l)));
                }
                failures++;
                if (failures >= 20)
                    return 1;
            }
        }
    }

    // broadcast_lane smoke: element patterns
    for (int element = 0; element < 16; element++) {
        for (int dest = 0; dest < 8; dest++) {
            int expect;
            if (element < 2)
                expect = dest;
            else if (element < 4)
                expect = (dest & ~1) | (element & 1);
            else if (element < 8)
                expect = (dest & ~3) | (element & 3);
            else
                expect = element & 7;

            // Drive through VAND with distinctive vt lanes
            scalar.reset();
            simd.reset();
            for (int l = 0; l < 8; l++) {
                scalar.vreg(1).set_lane(l, 0xFFFF);
                scalar.vreg(2).set_lane(l, static_cast<uint16_t>(0x1000 + l));
            }
            copy_vu_state(simd, scalar);
            const uint32_t inst = encode_vu(0x28, 0, 1, 2, element);
            N64::Rsp::vu_execute_compute_scalar(scalar, inst);
            N64::Rsp::vu_execute_compute_simd(simd, inst);
            if (scalar.vreg(0).lane(dest) !=
                static_cast<uint16_t>(0x1000 + expect)) {
                std::fprintf(stderr,
                             "broadcast expect failed element=%d dest=%d\n",
                             element, dest);
                return 1;
            }
            std::string why;
            if (!same_vu_state(scalar, simd, 0, why)) {
                std::fprintf(stderr, "broadcast mismatch element=%d (%s)\n",
                             element, why.c_str());
                return 1;
            }
        }
    }

#if defined(N64_RSP_JIT)
    {
        static const int kSseFuncts[] = {0x00, 0x04, 0x05, 0x06, 0x07, 0x08,
                                         0x0C, 0x0D, 0x0E, 0x0F};
        Rsp sse;
        for (int funct : kSseFuncts) {
            for (int c = 0; c < kCasesPerOp; c++) {
                randomize_vu_state(scalar, rng);
                copy_vu_state(sse, scalar);
                const int vd = static_cast<int>(rng() % 32);
                const int vs = static_cast<int>(rng() % 32);
                const int vt = static_cast<int>(rng() % 32);
                const int element = static_cast<int>(rng() % 16);
                const uint32_t inst = encode_vu(funct, vd, vs, vt, element);
                N64::Rsp::vu_execute_compute_scalar(scalar, inst);
                if (!N64::Rsp::Jit::vu_sse_compute(
                        sse, static_cast<unsigned>(vd),
                        static_cast<unsigned>(vs), static_cast<unsigned>(vt),
                        static_cast<unsigned>(element),
                        static_cast<unsigned>(funct))) {
                    std::fprintf(stderr, "sse helper rejected funct=%#x\n",
                                 funct);
                    return 1;
                }
                std::string why;
                if (!same_vu_state(scalar, sse, vd, why)) {
                    std::fprintf(stderr,
                                 "sse mismatch funct=%#x case=%d (%s)\n", funct,
                                 c, why.c_str());
                    failures++;
                    if (failures >= 20)
                        return 1;
                }
            }
        }
    }
#endif

    if (failures) {
        std::fprintf(stderr, "%d failures\n", failures);
        return 1;
    }
    std::printf("rsp_vu_diff_test: ok (%zu ops x %d cases)\n",
                sizeof(kSimdFuncts) / sizeof(kSimdFuncts[0]), kCasesPerOp);
    return 0;
}
