#include "n64_system/scheduler.h"
#include "utils/utils.h"

namespace N64 {
namespace N64System {

bool scheduled_event_compare(scheduled_event_t e, scheduled_event_t o) {
    return e.first < o.first;
}

void Scheduler::set_timer(uint64_t cycles, Event event) {
    event_queue.push({current_time + cycles, event});
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

Scheduler Scheduler::instance{};

} // namespace N64System

N64System::Scheduler &g_scheduler() {
    return N64System::Scheduler::get_instance();
}

} // namespace N64
