#include "rcp/rsp_thread.h"
#include "n64_system/interrupt.h"
#include "rcp/rsp.h"

namespace N64 {
namespace Rsp {

RspThread &RspThread::get_instance() {
    static RspThread inst;
    return inst;
}

void RspThread::configure(bool enabled) {
    shutdown();
    enabled_ = enabled;
}

void RspThread::start() {
    if (!enabled_ || thr_.joinable())
        return;
    stop_ = false;
    kick_pending_ = false;
    running_ = false;
    irq_pending_ = false;
    thr_ = std::thread([this] { worker_main(); });
}

void RspThread::shutdown() {
    {
        std::lock_guard<std::mutex> lock(mu_);
        stop_ = true;
        kick_pending_ = false;
        cv_kick_.notify_all();
    }
    if (thr_.joinable())
        thr_.join();
    enabled_ = false;
    running_ = false;
    irq_pending_ = false;
    worker_id_ = {};
}

void RspThread::kick_until_halt() {
    if (!enabled_)
        return;
    {
        std::lock_guard<std::mutex> lock(mu_);
        if (stop_)
            return;
        kick_pending_ = true;
    }
    cv_kick_.notify_one();
}

void RspThread::wait_idle() {
    if (!enabled_)
        return;
    if (std::this_thread::get_id() == worker_id_)
        return;

    std::unique_lock<std::mutex> lock(mu_);
    cv_idle_.wait(lock, [&] { return stop_ || (!running_ && !kick_pending_); });

    if (irq_pending_) {
        irq_pending_ = false;
        lock.unlock();
        N64System::check_interrupt();
    }
}

void RspThread::note_sp_interrupt() {
    if (!enabled_ || std::this_thread::get_id() != worker_id_) {
        N64System::check_interrupt();
        return;
    }
    std::lock_guard<std::mutex> lock(mu_);
    irq_pending_ = true;
}

bool RspThread::on_worker_thread() const {
    return enabled_ && std::this_thread::get_id() == worker_id_;
}

void RspThread::worker_main() {
    worker_id_ = std::this_thread::get_id();
    for (;;) {
        {
            std::unique_lock<std::mutex> lock(mu_);
            cv_kick_.wait(lock, [&] { return stop_ || kick_pending_; });
            if (stop_)
                break;
            kick_pending_ = false;
            running_ = true;
        }

        run_quantum();

        {
            std::lock_guard<std::mutex> lock(mu_);
            running_ = false;
        }
        cv_idle_.notify_all();
    }
}

void RspThread::run_quantum() {
    Rsp &rsp = g_rsp();
    uint32_t ran = 0;
    while (!rsp.halted() && ran < kQuantumInsns) {
        {
            std::lock_guard<std::mutex> lock(mu_);
            if (stop_)
                return;
        }
        rsp.step();
        ++ran;
    }
}

} // namespace Rsp
} // namespace N64
