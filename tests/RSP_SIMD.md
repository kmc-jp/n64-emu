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

SIMD: VAND–VNXOR, VMRG, VABS, VLT/VEQ/VNE/VGE, VADD/VSUB/VADDC/VSUBC, VMULF/U, VMACF/U, VMUD*/VMAD*.

Scalar fallback: VRCP/VRSQ family, VCH/VCL/VCR, VMOV, reserved ops, LWC2/SWC2.
