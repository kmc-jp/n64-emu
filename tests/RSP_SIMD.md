# RSP VU EVE SIMD

## Build

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DN64_RSP_SIMD=ON -DN64_SIMD_ARCH=native
cmake --build build -j
```

- `N64_RSP_SIMD=OFF` — scalar-only path (A/B comparison of the full emulator)
- `N64_SIMD_ARCH` — `native` (default), `x86-64-v2`, `x86-64-v3`, or empty string for no `-march`

Pinned EVE: submodule `third_party/eve` ([jfalcou/eve](https://github.com/jfalcou/eve), BSL-1.0).

ACC is stored as **H/M/L** three `VuReg` (16bit×8), matching CEN64 / parallel-rsp.

## Covered vs scalar fallback

SIMD compute: VAND–VNXOR, VMRG, VABS, VLT/VEQ/VNE/VGE, VCH, VADD/VSUB/VADDC/VSUBC, VMULF/U, VMACF/U, VMUD*/VMAD*, VSAR, VMOV, VRCP/VRCPL/VRCPH/VRSQ/VRSQL/VRSQH (ACC via SIMD; single-lane result stays scalar table lookup).

LWC2/SWC2: LLV/LDV/LQV/LRV and SLV/SDV/SQV/SRV use bulk DMEM↔VU packing; LSV/SSV use halfword DMEM; LPV/LUV bulk-load a 16B window then expand.

Scalar fallback: VCL/VCR, reserved ops, LHV/LFV/LTV and SPV/SUV/SHV/SFV/SWV/STV.

## Profiling

```bash
N64_PROFILE_VU=1 N64_PROFILE_FRAME=1 ./src/n64 --jit rom.z64
```

Dumps top COP2 compute ops (with scalar_fallback counts), LWC2/SWC2 majors, and COP2 moves about once per second with the frame profiler.
