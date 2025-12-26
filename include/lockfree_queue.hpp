#pragma once

#include <atomic>
#include <array>
#include <cstddef>

namespace hft {

template<typename T, size_t N>
class SPSCQueue {
    static_assert((N & (N - 1)) ==0, "size must be power of 2");

private:
    alignas(64) std::atomic<size_t> head_{0};
    alignas(64) std::atomic<size_t> tail_{0};
    std::array<T, N> buf_;

public:
    SPSCQueue() = default;
    SPSCQueue(const SPSCQueue&) = delete;
    SPSCQueue& operator=(const SPSCQueue&) = delete;

    bool push(const T& item) {
        auto t = tail_.load(std::memory_order_relaxed);
        auto next_t = (t + 1) & (N - 1);

        if (next_t ==head_.load(std::memory_order_acquire))
            return false;

        buf_[t] = item;
        tail_.store(next_t, std::memory_order_release);
        return true;
    }

    bool pop(T& item) {
        auto h = head_.load(std::memory_order_relaxed);

        if (h ==tail_.load(std::memory_order_acquire))
            return false;

        item = buf_[h];
        head_.store((h + 1) & (N - 1), std::memory_order_release);
        return true;
    }

    bool empty() const {
        return head_.load(std::memory_order_acquire) ==tail_.load(std::memory_order_acquire);
               }

    size_t size() const {
    auto h = head_.load(std::memory_order_acquire);
        auto t = tail_.load(std::memory_order_acquire);
        return (t - h) & (N - 1);
        }

size_t capacity() const { return N - 1; }
    };

}