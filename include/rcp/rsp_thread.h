#ifndef RCP_RSP_THREAD_H
#define RCP_RSP_THREAD_H

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <thread>

namespace N64 {
namespace Rsp {

// Opt-in RSP worker: run-until-halt (with a per-kick instruction quantum)
// so the CPU thread can overlap. Shared SP/DMEM/IMEM/DMA accesses must call
// wait_idle() first from the CPU thread.
class RspThread {
  public:
    static RspThread &get_instance();

    void configure(bool enabled);
    void start();
    void shutdown();

    bool enabled() const { return enabled_; }

    void kick_until_halt();
    void wait_idle();
    void note_sp_interrupt();
    bool on_worker_thread() const;

  private:
    RspThread() = default;

    void worker_main();
    void run_quantum();

    bool enabled_{false};
    std::atomic<bool> stop_{false};
    bool running_{false};
    bool kick_pending_{false};
    bool irq_pending_{false};

    // Cap per kick so CPU↔RSP busy-wait handshakes cannot deadlock.
    static constexpr uint32_t kQuantumInsns = 100000;

    std::mutex mu_;
    std::condition_variable cv_kick_;
    std::condition_variable cv_idle_;
    std::thread thr_;
    std::thread::id worker_id_{};
};

inline RspThread &g_rsp_thread() { return RspThread::get_instance(); }

} // namespace Rsp
} // namespace N64

#endif
