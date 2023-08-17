#ifndef PIF_H
#define PIF_H

#include "memory/bus.h"
#include "cpu/cpu.h"
#include "rcp/rsp.h"

namespace N64 {
namespace Memory {

// TODO: move this to si.h
void pif_rom_execute();

} // namespace Memory
} // namespace N64

#endif