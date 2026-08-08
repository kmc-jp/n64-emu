#include "n64_system/scheduler.h"
#include "utils/log.h"
#include <cstdint>

namespace N64 {
namespace N64System {

void Scheduler::set_timer(uint64_t cycles, Event event) {
    event_queue.push({current_time + cycles, std::move(event)});
}

void Scheduler::tick(uint64_t cycles) {
    current_time += cycles;
    // 少し余裕をもたせる
    if (current_time > 0x0000'FFFF'FFFF'FFFF) {
        // FIXME: fix this
        Utils::unimplemented("current_time is reaching max");
    }

    while (!event_queue.empty()) {
        // seek top event
        scheduled_event_t e = event_queue.top();
        if (e.first > current_time) {
            return;
        } else {
            // pop an event from queue and perform it
            event_queue.pop();
            e.second.perform();
        }
    }
}

uint64_t Scheduler::cycles_until_next_event() const {
    if (event_queue.empty())
        return UINT64_MAX;
    const uint64_t at = event_queue.top().first;
    if (at <= current_time)
        return 0;
    return at - current_time;
}

Scheduler Scheduler::instance{};

} // namespace N64System

N64System::Scheduler &g_scheduler() {
    return N64System::Scheduler::get_instance();
}

} // namespace N64
