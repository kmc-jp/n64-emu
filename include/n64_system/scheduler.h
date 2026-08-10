#ifndef SCHEDULER_H
#define SCHEDULER_H

#include <cstdint>
#include <functional>
#include <queue>
#include <utility>
#include <vector>

namespace N64 {
namespace N64System {

class Event {
  private:
    std::function<void()> f;

  public:
    Event(std::function<void()> callback) : f(std::move(callback)) {}

    void perform() { f(); }
};

using scheduled_event_t = std::pair<uint64_t, Event>;

struct ScheduledEventEarlier {
    bool operator()(const scheduled_event_t &a,
                    const scheduled_event_t &b) const {
        return a.first > b.first;
    }
};

enum class NamedEventId : int {
    Sp = 0,
    SpDma = 1,
    Count = 2,
};

class Scheduler {
  private:
    static Scheduler instance;

    std::priority_queue<scheduled_event_t, std::vector<scheduled_event_t>,
                        ScheduledEventEarlier>
        event_queue;

    struct NamedEvent {
        bool enabled = false;
        uint64_t at = 0;
        std::function<void()> cb;
    };
    NamedEvent named_[static_cast<int>(NamedEventId::Count)]{};

    uint64_t current_time;

    bool dispatch_one_due();

  public:
    Scheduler() : current_time(0) {}

    void init();

    void set_timer(uint64_t cycles, Event event);
    void schedule_named(NamedEventId id, uint64_t cycles,
                        std::function<void()> cb);
    void cancel_named(NamedEventId id);

    void tick(uint64_t cycles = 1);

    uint64_t get_current_time() const { return current_time; }
    uint64_t cycles_until_next_event() const;

    inline static Scheduler &get_instance() { return instance; }
};

} // namespace N64System

N64System::Scheduler &g_scheduler();

} // namespace N64

#endif
