#ifndef SCHEDULER_H
#define SCHEDULER_H

#include "utils.h"
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
    Event(std::function<void()> callback) : f(callback) {}

    void perform() { f(); }
};

typedef std::pair<uint64_t, Event> scheduled_event_t;

bool scheduled_event_compare(scheduled_event_t, scheduled_event_t);

class Scheduler {
  private:
    static Scheduler instance;

    // イベントのキュー. 要素は, イベントとそれが実行される時刻のペア
    std::priority_queue<
        scheduled_event_t, std::vector<scheduled_event_t>,
        std::function<bool(scheduled_event_t, scheduled_event_t)>>
        event_queue;

    // 現在の時刻
    uint64_t current_time;

  public:
    Scheduler() : current_time(0) {}

    void init() { current_time = 0; }

    // 現在からCPU cycles後にイベントを実行する
    void set_timer(uint64_t cycles, Event event) {
        event_queue.push({current_time + cycles, event});
    }

    // TODO: スケジューラ単体をテストしたほうがいい?
    void tick(uint64_t cycles = 1) {
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

    inline static Scheduler &get_instance() { return instance; }
};

} // namespace N64System

inline N64System::Scheduler &g_scheduler() {
    return N64System::Scheduler::get_instance();
}

} // namespace N64

#endif
