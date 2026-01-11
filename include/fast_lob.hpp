#pragma once

#include "common_types.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <vector>

// Prefetch/branch macros (tiny latency wins on some CPUs)
#if defined(__GNUC__) || defined(__clang__)
    #define PREFETCH_READ(addr)  __builtin_prefetch((addr), 0, 3)
    #define PREFETCH_WRITE(addr) __builtin_prefetch((addr), 1, 3)
    #define LIKELY(x)            __builtin_expect(!!(x), 1)
    #define UNLIKELY(x)          __builtin_expect(!!(x), 0)
#else
    #define PREFETCH_READ(addr)
    #define PREFETCH_WRITE(addr)
    #define LIKELY(x) (x)
    #define UNLIKELY(x) (x)
#endif

namespace hft {

// 64-byte aligned level (fits in one cache line).
struct alignas(64) FastPriceLevel {
    double price = 0.0;
    double quantity = 0.0;
    uint32_t order_count = 0;
    bool is_active = false;
    char _pad[39] = {0};
};

/**
 * Flat, fixed-capacity LOB representation.
 *
 * API intentionally matches how it's used in:
 * - examples/busy_wait_example.cpp
 * - benchmarks/benchmark_main.cpp
 */
template<size_t MaxLevels = 100>
class ArrayBasedOrderBook {
public:
    ArrayBasedOrderBook() { clear(); }

    inline void update_bid(size_t level_idx, double price, double quantity) {
        update_level(bids_, level_idx, price, quantity);
    }

    inline void update_ask(size_t level_idx, double price, double quantity) {
        update_level(asks_, level_idx, price, quantity);
    }

    // Best bid/ask prices (scan active levels).
    inline double get_best_bid() const {
        double best = 0.0;
        for (const auto& lvl : bids_) {
            if (lvl.is_active && (best == 0.0 || lvl.price > best)) {
                best = lvl.price;
            }
        }
        return best;
    }

    inline double get_best_ask() const {
        double best = 0.0;
        for (const auto& lvl : asks_) {
            if (lvl.is_active && (best == 0.0 || lvl.price < best)) {
                best = lvl.price;
            }
        }
        return best;
    }

    // Order Flow Imbalance over top N levels.
    // Simple, deterministic definition for demos/benchmarks.
    inline double calculate_ofi(size_t depth_levels) const {
        const size_t n = std::min(depth_levels, MaxLevels);

        double bid_qty = 0.0;
        double ask_qty = 0.0;

        for (size_t i = 0; i < n; ++i) {
            if (bids_[i].is_active) bid_qty += bids_[i].quantity;
            if (asks_[i].is_active) ask_qty += asks_[i].quantity;
        }

        const double denom = bid_qty + ask_qty;
        if (denom <= 0.0) {
            return 0.0;
        }
        return (bid_qty - ask_qty) / denom;
    }

    inline void clear() {
        bids_.fill(FastPriceLevel{});
        asks_.fill(FastPriceLevel{});
    }

private:
    alignas(64) std::array<FastPriceLevel, MaxLevels> bids_;
    alignas(64) std::array<FastPriceLevel, MaxLevels> asks_;

    inline void update_level(std::array<FastPriceLevel, MaxLevels>& side,
                             size_t level_idx,
                             double price,
                             double quantity) {
        if (UNLIKELY(level_idx >= MaxLevels)) {
            return;
        }

        PREFETCH_WRITE(&side[level_idx]);

        auto& lvl = side[level_idx];
        lvl.price = price;
        lvl.quantity = quantity;
        lvl.is_active = (quantity > 0.0);
        lvl.order_count = lvl.is_active ? 1 : 0;
    }
};

// Benchmarks use `FastLOB` as a convenient concrete type.
using FastLOB = ArrayBasedOrderBook<100>;

} // namespace hft
