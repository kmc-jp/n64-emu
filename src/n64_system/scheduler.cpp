#include "n64_system/scheduler.h"

namespace N64 {
namespace N64System {

bool scheduled_event_compare(scheduled_event_t e, scheduled_event_t o) {
    return e.first < o.first;
}

Scheduler Scheduler::instance{};

} // namespace N64System
} // namespace N64
