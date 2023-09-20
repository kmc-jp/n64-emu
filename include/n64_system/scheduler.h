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
    Event(std::function<void()> callback) : f(callback) {}

    void perform() { f(); }
};

using scheduled_event_t = std::pair<uint64_t, Event>;

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
    void set_timer(uint64_t cycles, Event event);

    // TODO: スケジューラ単体をテストしたほうがいい?
    void tick(uint64_t cycles = 1);

    uint64_t get_current_time() const { return current_time; }

    inline static Scheduler &get_instance() { return instance; }
};

} // namespace N64System

N64System::Scheduler &g_scheduler();

} // namespace N64

#endif
