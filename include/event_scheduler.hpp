#pragma once

#include "common_types.hpp"
#include <atomic>
#include <vector>
#include <algorithm>
#include <queue>
#include <functional>

namespace hft {
namespace scheduler {

struct ScheduledEvent {
    Timestamp execution_time;
    uint64_t event_id;
    std::function<void()> callback;
    bool is_cancelled;

    ScheduledEvent()
        : execution_time(now()), event_id(0),
          callback(nullptr), is_cancelled(false) {}

    ScheduledEvent(Timestamp time, uint64_t id, std::function<void()> cb)
        : execution_time(time), event_id(id),
          callback(std::move(cb)), is_cancelled(false) {}
};

class TimingWheelScheduler {
public:
    explicit TimingWheelScheduler(
        size_t num_slots = 1024,
        Duration slot_duration_ns = std::chrono::microseconds(10)
    ) : num_slots_(num_slots),
        slot_duration_(slot_duration_ns),
        current_slot_(0),
        next_event_id_(1),
        wheel_(num_slots),
        start_time_(now()) {

        for (auto& slot : wheel_) {
            slot.reserve(16);
        }
    }

    uint64_t schedule_at(Timestamp exec_time, std::function<void()> callback) {
        const int64_t delay_ns = to_nanos(exec_time) - to_nanos(now());

        if (delay_ns <= 0) {

            callback();
            return 0;
        }

        return schedule_after(std::chrono::nanoseconds(delay_ns), std::move(callback));
    }

    uint64_t schedule_after(Duration delay, std::function<void()> callback) {
        const uint64_t event_id = next_event_id_.fetch_add(1, std::memory_order_relaxed);

        const int64_t delay_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(delay).count();
        const int64_t slot_delay_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(slot_duration_).count();
        const size_t slots_ahead = static_cast<size_t>(delay_ns / slot_delay_ns);
        const size_t target_slot = (current_slot_ + slots_ahead) % num_slots_;

        const Timestamp exec_time = now() + delay;

        wheel_[target_slot].emplace_back(exec_time, event_id, std::move(callback));

        return event_id;
    }

    void cancel(uint64_t event_id) {

        for (auto& slot : wheel_) {
            for (auto& event : slot) {
                if (event.event_id == event_id) {
                    event.is_cancelled = true;
                    return;
                }
            }
        }
    }

    void tick() {
        auto& current_events = wheel_[current_slot_];
        const Timestamp current_time = now();

        for (auto& event : current_events) {
            if (!event.is_cancelled && event.callback) {

                if (to_nanos(event.execution_time) <= to_nanos(current_time)) {
                    event.callback();
                }
            }
        }

        current_events.clear();

        current_slot_ = (current_slot_ + 1) % num_slots_;
    }

    void process_until(Timestamp target_time) {
        while (to_nanos(now()) < to_nanos(target_time)) {
            tick();

            const Timestamp next_slot_time = start_time_ +
                slot_duration_ * (current_slot_ + 1);

            while (to_nanos(now()) < to_nanos(next_slot_time)) {

#if defined(__x86_64__) || defined(__i386__)
                __asm__ __volatile__("pause" ::: "memory");
#elif defined(__aarch64__)
                __asm__ __volatile__("yield" ::: "memory");
#else

#endif
            }
        }
    }

    size_t get_pending_count() const {
        size_t count = 0;
        for (const auto& slot : wheel_) {
            for (const auto& event : slot) {
                if (!event.is_cancelled) {
                    ++count;
                }
            }
        }
        return count;
    }

private:
    const size_t num_slots_;
    const Duration slot_duration_;
    size_t current_slot_;
    std::atomic<uint64_t> next_event_id_;
    std::vector<std::vector<ScheduledEvent>> wheel_;
    Timestamp start_time_;
};

template<typename EventType, size_t MaxEvents = 4096>
class PriorityEventQueue {
public:
    struct Event {
        EventType data;
        uint64_t priority;
        bool is_valid;

        Event() : data(), priority(UINT64_MAX), is_valid(false) {}

        bool operator<(const Event& other) const {
            return priority > other.priority;
        }
    };

    PriorityEventQueue() : event_count_(0) {
        events_.reserve(MaxEvents);
    }

    bool push(const EventType& event, uint64_t priority) {
        if (event_count_ >= MaxEvents) {
            return false;
        }

        Event e;
        e.data = event;
        e.priority = priority;
        e.is_valid = true;

        events_.push_back(e);
        std::push_heap(events_.begin(), events_.end());

        ++event_count_;
        return true;
    }

    bool pop(EventType& event) {
        while (!events_.empty()) {
            std::pop_heap(events_.begin(), events_.end());
            Event& e = events_.back();

            if (e.is_valid) {
                event = e.data;
                events_.pop_back();
                --event_count_;
                return true;
            }

            events_.pop_back();
        }

        return false;
    }

    bool peek(EventType& event) const {
        if (events_.empty()) {
            return false;
        }

        if (events_.front().is_valid) {
            event = events_.front().data;
            return true;
        }

        return false;
    }

    size_t size() const { return event_count_; }
    bool empty() const { return event_count_ == 0; }

private:
    std::vector<Event> events_;
    size_t event_count_;
};

class DeterministicEventLoop {
public:
    DeterministicEventLoop()
        : timing_wheel_(1024, std::chrono::microseconds(10)),
          is_running_(false) {}

    uint64_t schedule_at(Timestamp time, std::function<void()> callback) {
        return timing_wheel_.schedule_at(time, std::move(callback));
    }

    uint64_t schedule_after(Duration delay, std::function<void()> callback) {
        return timing_wheel_.schedule_after(delay, std::move(callback));
    }

    template<typename T>
    bool add_event(const T& event, uint64_t priority) {
        return priority_queue_.push(event, priority);
    }

    void cancel_event(uint64_t event_id) {
        timing_wheel_.cancel(event_id);
    }

    void run() {
        is_running_.store(true, std::memory_order_release);

        while (is_running_.load(std::memory_order_acquire)) {

            timing_wheel_.tick();

            MarketTick tick;
            while (priority_queue_.pop(tick)) {

            }

            __asm__ __volatile__("pause" ::: "memory");
        }
    }

    void stop() {
        is_running_.store(false, std::memory_order_release);
    }

private:
    TimingWheelScheduler timing_wheel_;
    PriorityEventQueue<MarketTick, 4096> priority_queue_;
    std::atomic<bool> is_running_;
};

}
}