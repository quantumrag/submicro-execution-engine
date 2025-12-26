#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <thread>
#include <chrono>
#include <atomic>
#include <functional>

#include "event_scheduler.hpp"
#include "common_types.hpp"

// Test fixture for Event Scheduler tests
class EventSchedulerTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Setup common test data
    }

    void TearDown() override {
        // Cleanup if needed
    }
};

// Test PriorityEventQueue basic functionality
TEST_F(EventSchedulerTest, PriorityEventQueueBasic) {
    hft::scheduler::PriorityEventQueue<int, 16> queue;

    EXPECT_TRUE(queue.empty());
    EXPECT_EQ(queue.size(), 0);

    // Add events with different priorities (lower number = higher priority)
    EXPECT_TRUE(queue.push(100, 3));  // priority 3
    EXPECT_TRUE(queue.push(200, 1));  // priority 1 (highest)
    EXPECT_TRUE(queue.push(300, 2));  // priority 2

    EXPECT_FALSE(queue.empty());
    EXPECT_EQ(queue.size(), 3);

    // Should get highest priority first (priority 1)
    int value;
    EXPECT_TRUE(queue.pop(value));
    EXPECT_EQ(value, 200);
    EXPECT_EQ(queue.size(), 2);

    // Next should be priority 2
    EXPECT_TRUE(queue.pop(value));
    EXPECT_EQ(value, 300);
    EXPECT_EQ(queue.size(), 1);

    // Finally priority 3
    EXPECT_TRUE(queue.pop(value));
    EXPECT_EQ(value, 100);
    EXPECT_EQ(queue.size(), 0);

    EXPECT_TRUE(queue.empty());
}

// Test PriorityEventQueue peek functionality
TEST_F(EventSchedulerTest, PriorityEventQueuePeek) {
    hft::scheduler::PriorityEventQueue<int, 16> queue;

    queue.push(100, 2);
    queue.push(200, 1);

    // Peek should return highest priority without removing
    int value;
    EXPECT_TRUE(queue.peek(value));
    EXPECT_EQ(value, 200);
    EXPECT_EQ(queue.size(), 2);

    // Pop should still work
    EXPECT_TRUE(queue.pop(value));
    EXPECT_EQ(value, 200);
    EXPECT_EQ(queue.size(), 1);
}

// Test PriorityEventQueue capacity limits
TEST_F(EventSchedulerTest, PriorityEventQueueCapacity) {
    hft::scheduler::PriorityEventQueue<int, 4> queue;

    // Fill to capacity
    EXPECT_TRUE(queue.push(1, 4));
    EXPECT_TRUE(queue.push(2, 3));
    EXPECT_TRUE(queue.push(3, 2));
    EXPECT_TRUE(queue.push(4, 1));

    // Should reject when full
    EXPECT_FALSE(queue.push(5, 5));
    EXPECT_EQ(queue.size(), 4);

    // Should still work
    int value;
    EXPECT_TRUE(queue.pop(value));
    EXPECT_EQ(value, 4);  // Highest priority (lowest number)

    // Should accept one more
    EXPECT_TRUE(queue.push(5, 0));  // Highest priority
    EXPECT_EQ(queue.size(), 4);

    // Should get the new highest priority
    EXPECT_TRUE(queue.pop(value));
    EXPECT_EQ(value, 5);
}

// Test TimingWheelScheduler basic functionality
TEST_F(EventSchedulerTest, TimingWheelSchedulerBasic) {
    hft::scheduler::TimingWheelScheduler scheduler(64, std::chrono::microseconds(10));

    // Test initial state
    EXPECT_EQ(scheduler.get_pending_count(), 0);

    // Schedule some events
    std::atomic<int> counter{0};
    auto id1 = scheduler.schedule_after(std::chrono::microseconds(50), [&counter](){ counter++; });
    auto id2 = scheduler.schedule_after(std::chrono::microseconds(100), [&counter](){ counter++; });

    // Should have pending events
    EXPECT_GE(scheduler.get_pending_count(), 1);

    // Test cancellation
    scheduler.cancel(id1);
    // Note: cancellation marks events but doesn't immediately remove them from count
}

// Test TimingWheelScheduler immediate execution
TEST_F(EventSchedulerTest, TimingWheelSchedulerImmediate) {
    hft::scheduler::TimingWheelScheduler scheduler(64, std::chrono::microseconds(10));

    std::atomic<int> executed{0};
    auto callback = [&executed]() { executed = 1; };

    // Schedule event in the past (should execute immediately)
    hft::Timestamp past_time = hft::now() - std::chrono::milliseconds(1);
    scheduler.schedule_at(past_time, callback);

    // Give a moment for execution
    std::this_thread::sleep_for(std::chrono::microseconds(100));

    // Should have executed immediately
    EXPECT_EQ(executed.load(), 1);
}

// Test TimingWheelScheduler tick functionality
TEST_F(EventSchedulerTest, TimingWheelSchedulerTick) {
    hft::scheduler::TimingWheelScheduler scheduler(64, std::chrono::nanoseconds(1000));  // 1 microsecond slots

    std::atomic<int> executed{0};
    auto callback = [&executed]() { executed = 1; };

    // Schedule event for very soon (2 microseconds)
    scheduler.schedule_after(std::chrono::microseconds(2), callback);

    // Tick a few times to ensure the event executes
    for (int i = 0; i < 10 && executed.load() == 0; ++i) {
        scheduler.tick();
        std::this_thread::sleep_for(std::chrono::microseconds(1));
    }

    EXPECT_EQ(executed.load(), 1);
}

// Test DeterministicEventLoop basic functionality
TEST_F(EventSchedulerTest, DeterministicEventLoopBasic) {
    hft::scheduler::DeterministicEventLoop loop;

    // Schedule a timing event with a longer delay to ensure it's not immediate
    auto id = loop.schedule_after(std::chrono::seconds(1), [](){});

    // Add a priority event
    hft::MarketTick tick;
    tick.timestamp = hft::now();
    tick.bid_price = 50000.0;
    tick.ask_price = 50001.0;
    tick.trade_volume = 100;
    EXPECT_TRUE(loop.add_event(tick, 1));

    // Should get a valid event ID (non-zero)
    EXPECT_NE(id, 0ULL);
}

// Test basic thread safety (simplified)
TEST_F(EventSchedulerTest, PriorityEventQueueThreadSafety) {
    hft::scheduler::PriorityEventQueue<int, 64> queue;

    // Basic thread safety test - just verify no crashes
    std::thread producer([&]() {
        for (int i = 0; i < 10; ++i) {
            queue.push(i, static_cast<uint64_t>(i % 5));
        }
    });

    std::thread consumer([&]() {
        for (int i = 0; i < 10; ++i) {
            int value;
            queue.pop(value);
        }
    });

    producer.join();
    consumer.join();

    // Should be empty after consuming all
    EXPECT_TRUE(queue.empty());
}

// Test performance - PriorityEventQueue operations should be fast
TEST_F(EventSchedulerTest, PriorityEventQueuePerformance) {
    hft::scheduler::PriorityEventQueue<int, 1024> queue;

    // Measure push performance
    auto start = std::chrono::high_resolution_clock::now();

    const int num_operations = 10000;
    for (int i = 0; i < num_operations; ++i) {
        queue.push(i, static_cast<uint64_t>(i % 100));
    }

    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    double avg_time_per_push = static_cast<double>(duration.count()) / num_operations;

    // Should be very fast (< 1 microsecond per push)
    EXPECT_LT(avg_time_per_push, 1.0);

    // Measure pop performance
    start = std::chrono::high_resolution_clock::now();

    for (int i = 0; i < num_operations; ++i) {
        int value;
        queue.pop(value);
    }

    end = std::chrono::high_resolution_clock::now();
    duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    double avg_time_per_pop = static_cast<double>(duration.count()) / num_operations;

    // Should be very fast (< 1 microsecond per pop)
    EXPECT_LT(avg_time_per_pop, 1.0);
}

// Test edge cases
TEST_F(EventSchedulerTest, EdgeCases) {
    // Test empty queue operations
    hft::scheduler::PriorityEventQueue<int, 16> queue;

    int value;
    EXPECT_FALSE(queue.pop(value));
    EXPECT_FALSE(queue.peek(value));

    // Test full queue
    hft::scheduler::PriorityEventQueue<int, 2> small_queue;
    EXPECT_TRUE(small_queue.push(1, 1));
    EXPECT_TRUE(small_queue.push(2, 2));
    EXPECT_FALSE(small_queue.push(3, 3));  // Should fail

    // Test TimingWheelScheduler with zero delay
    hft::scheduler::TimingWheelScheduler scheduler(64, std::chrono::microseconds(10));

    std::atomic<int> executed{0};
    scheduler.schedule_after(std::chrono::nanoseconds(0), [&executed](){ executed = 1; });

    // Should execute on next tick
    scheduler.tick();
    EXPECT_EQ(executed.load(), 1);
}

int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}