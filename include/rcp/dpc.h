#ifndef DPC_H
#define DPC_H

#include "utils/pack.h"
#include <cstdint>

namespace N64 {
namespace Rdp {

constexpr uint32_t PADDR_DPC_START = 0x04100000;
constexpr uint32_t PADDR_DPC_END = 0x04100004;
constexpr uint32_t PADDR_DPC_CURRENT = 0x04100008;
constexpr uint32_t PADDR_DPC_STATUS = 0x0410000C;
constexpr uint32_t PADDR_DPC_CLOCK = 0x04100010;
constexpr uint32_t PADDR_DPC_BUFBUSY = 0x04100014;
constexpr uint32_t PADDR_DPC_PIPEBUSY = 0x04100018;
constexpr uint32_t PADDR_DPC_TMEM = 0x0410001C;

union dpc_status_t {
    uint32_t raw;
    PACK(struct {
        unsigned xbus_dmem_dma : 1;
        unsigned freeze : 1;
        unsigned flush : 1;
        unsigned start_gclk : 1;
        unsigned tmem_busy : 1;
        unsigned pipe_busy : 1;
        unsigned cmd_busy : 1;
        unsigned cbuf_ready : 1;
        unsigned dma_busy : 1;
        unsigned end_valid : 1;
        unsigned start_valid : 1;
        unsigned : 21;
    });
};

class Dpc {
  private:
    uint32_t start{};
    uint32_t end{};
    uint32_t current{};
    dpc_status_t status{};
    uint32_t clock{};
    uint32_t tmem{};

    static Dpc instance;

  public:
    Dpc() {}

    void reset();

    uint32_t read_paddr32(uint32_t paddr) const;

    void write_paddr32(uint32_t paddr, uint32_t value);

    void run_command();

    dpc_status_t &get_status() { return status; }
    uint32_t get_start() const { return start; }
    uint32_t get_end() const { return end; }
    uint32_t get_current() const { return current; }

    inline static Dpc &get_instance() { return instance; }

  private:
    void status_write(uint32_t value);
    void process_list();
};

} // namespace Rdp

Rdp::Dpc &g_dpc();

} // namespace N64

#endif
