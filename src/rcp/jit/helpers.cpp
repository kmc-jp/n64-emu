#include "rcp/jit/helpers.h"
#include "rcp/jit/jit.h"
#include "rcp/jit/vu_sse.h"
#include "rcp/rsp.h"

namespace N64 {
namespace Rsp {
namespace Jit {

int exec_one(uint32_t inst) {
    Rsp &rsp = g_rsp();
    if (rsp.halted())
        return 0;
    const uint32_t gen = g_dynarec().code_generation();
    rsp.jit_step(inst);
    if (g_dynarec().code_generation() != gen)
        return 2;
    return 1;
}

void vu_compute(uint32_t inst) {
    Rsp &rsp = g_rsp();
    const unsigned vd = (inst >> 6) & 0x1F;
    const unsigned vs = (inst >> 11) & 0x1F;
    const unsigned vt = (inst >> 16) & 0x1F;
    const unsigned e = (inst >> 21) & 0xF;
    const unsigned funct = inst & 0x3F;
    if (vu_sse_compute(rsp, vd, vs, vt, e, funct))
        return;
    vu_execute_compute(rsp, inst);
}

void vu_run_computes(const uint32_t *insts, uint32_t count) {
    for (uint32_t i = 0; i < count; ++i)
        vu_compute(insts[i]);
}

void vu_run_vector_ops(const uint32_t *insts, uint32_t count) {
    for (uint32_t i = 0; i < count; ++i) {
        const uint32_t inst = insts[i];
        switch ((inst >> 26) & 0x3F) {
        case 0x12:
            vu_compute(inst);
            break;
        case 0x32:
            vu_lwc2(inst);
            break;
        case 0x3A:
            vu_swc2(inst);
            break;
        default:
            vu_compute(inst);
            break;
        }
    }
}

void vu_lwc2(uint32_t inst) { vu_load(g_rsp(), inst); }
void vu_swc2(uint32_t inst) { vu_store(g_rsp(), inst); }

void vu_compute_fields(uint32_t vd, uint32_t vs, uint32_t vt, uint32_t e,
                       uint32_t funct) {
    Rsp &rsp = g_rsp();
    if (vu_sse_compute(rsp, vd, vs, vt, e, funct))
        return;
    const uint32_t inst = (0x12u << 26) | (1u << 25) | ((e & 0xFu) << 21) |
                          ((vt & 0x1Fu) << 16) | ((vs & 0x1Fu) << 11) |
                          ((vd & 0x1Fu) << 6) | (funct & 0x3Fu);
    vu_execute_compute(rsp, inst);
}

void do_break() { g_rsp().take_break(); }

uint32_t mem_lw(uint32_t addr) { return g_rsp().dmem_load32(addr); }
uint32_t mem_lh(uint32_t addr) {
    return static_cast<uint32_t>(static_cast<int32_t>(
        static_cast<int16_t>(g_rsp().dmem_load16(addr))));
}
uint32_t mem_lb(uint32_t addr) {
    return static_cast<uint32_t>(
        static_cast<int32_t>(static_cast<int8_t>(g_rsp().dmem_load8(addr))));
}
uint32_t mem_lhu(uint32_t addr) { return g_rsp().dmem_load16(addr); }
uint32_t mem_lbu(uint32_t addr) { return g_rsp().dmem_load8(addr); }

void mem_sw(uint32_t addr, uint32_t val) { g_rsp().dmem_store32(addr, val); }
void mem_sh(uint32_t addr, uint32_t val) {
    g_rsp().dmem_store16(addr, static_cast<uint16_t>(val));
}
void mem_sb(uint32_t addr, uint32_t val) {
    g_rsp().dmem_store8(addr, static_cast<uint8_t>(val));
}

} // namespace Jit
} // namespace Rsp
} // namespace N64
