#include "n64_system/scheduler.h"
#include "utils/log.h"
#include <cstdint>

namespace N64 {
namespace N64System {

void Scheduler::init() {
    current_time = 0;
    event_queue = {};
    for (auto &e : named_) {
        e.enabled = false;
        e.at = 0;
        e.cb = {};
    }
}

void Scheduler::set_timer(uint64_t cycles, Event event) {
    event_queue.push({current_time + cycles, std::move(event)});
}

void Scheduler::schedule_named(NamedEventId id, uint64_t cycles,
                               std::function<void()> cb) {
    auto &e = named_[static_cast<int>(id)];
    e.enabled = true;
    e.at = current_time + cycles;
    e.cb = std::move(cb);
}

void Scheduler::cancel_named(NamedEventId id) {
    auto &e = named_[static_cast<int>(id)];
    e.enabled = false;
    e.at = 0;
    e.cb = {};
}

bool Scheduler::dispatch_one_due() {
    uint64_t best_named = UINT64_MAX;
    int best_id = -1;
    for (int i = 0; i < static_cast<int>(NamedEventId::Count); ++i) {
        if (named_[i].enabled && named_[i].at < best_named) {
            best_named = named_[i].at;
            best_id = i;
        }
    }

    const bool have_q = !event_queue.empty();
    const uint64_t q_at = have_q ? event_queue.top().first : UINT64_MAX;

    if (have_q && q_at <= best_named) {
        if (q_at > current_time)
            return false;
        scheduled_event_t e = event_queue.top();
        event_queue.pop();
        e.second.perform();
        return true;
    }
    if (best_id >= 0) {
        if (best_named > current_time)
            return false;
        auto cb = std::move(named_[best_id].cb);
        named_[best_id].enabled = false;
        named_[best_id].cb = {};
        if (cb)
            cb();
        return true;
    }
    return false;
}

void Scheduler::tick(uint64_t cycles) {
    current_time += cycles;
    if (current_time > 0x0000'FFFF'FFFF'FFFF) {
        Utils::unimplemented("current_time is reaching max");
    }

    while (dispatch_one_due()) {
    }
}

uint64_t Scheduler::cycles_until_next_event() const {
    uint64_t next = UINT64_MAX;
    if (!event_queue.empty()) {
        const uint64_t at = event_queue.top().first;
        next = at <= current_time ? 0 : at - current_time;
    }
    for (const auto &e : named_) {
        if (!e.enabled)
            continue;
        const uint64_t rem = e.at <= current_time ? 0 : e.at - current_time;
        if (rem < next)
            next = rem;
    }
    return next;
}

Scheduler Scheduler::instance{};

} // namespace N64System

N64System::Scheduler &g_scheduler() {
    return N64System::Scheduler::get_instance();
}

} // namespace N64
