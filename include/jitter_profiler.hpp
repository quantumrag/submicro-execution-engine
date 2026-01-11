#pragma once

#include "common_types.hpp"
#include <array>
#include <atomic>
#include <algorithm>
#include <inttypes.h>

namespace hft {

/**
 * Jitter Profiler for Low-Latency Systems
 * 
 * Purpose: Detect and measure "Micro-Stalls" in the hot path. 
 * Traditional average latency hides tail events. 
 * This class builds a histogram of busy-wait loop cycle deltas. 
 * If the loop is interrupted by the kernel, SMI, or GC, the delta will spike.
 */
class JitterProfiler {
public:
    static constexpr size_t HISTOGRAM_BUCKETS = 20;
    static constexpr uint64_t BUCKET_WIDTH_CYCLES = 100; // 100 cycles per bucket

    JitterProfiler() : last_tsc_(0), max_jitter_cycles_(0), total_samples_(0), stalled_samples_(0) {
        histogram_.fill(0);
    }
    
    // Call this inside the busy-wait loop
    inline void mark() {
        uint64_t now = rdtsc();
        
        if (last_tsc_ > 0) {
            uint64_t delta = now - last_tsc_;
            
            // Record if delta is suspicious (e.g. > 1000 cycles)
            if (delta > 1000) {
                 stalled_samples_++;
                 if (delta > max_jitter_cycles_) {
                     max_jitter_cycles_ = delta;
                 }
            }
            
            // Add to histogram
            size_t bucket = delta / BUCKET_WIDTH_CYCLES;
            if (bucket >= HISTOGRAM_BUCKETS) bucket = HISTOGRAM_BUCKETS - 1;
            histogram_[bucket]++;
            
            total_samples_++;
        }
        
        last_tsc_ = now;
    }
    
    // Software Prefetch wrapper (GCC/Clang built-in)
    // Hint: 0=Non-temporal, 1=Low, 2=Moderate, 3=High locality (L1)
    template<typename T>
    static inline void prefetch_L1(const T* ptr) {
        __builtin_prefetch(ptr, 0, 3);
    }
    
    static inline void prefetch_next_line(const void* ptr) {
        __builtin_prefetch(reinterpret_cast<const char*>(ptr) + 64, 0, 3);
    }
    
    void print_report() const {
        printf("\n=== Jitter Analysis (Inter-Cycle Gaps) ===\n");
        printf("Total Samples: %" PRIu64 "\n", total_samples_);
        printf("Stalled Samples (>1000 cycles): %" PRIu64 "\n", stalled_samples_);
        printf("Max Jitter: %" PRIu64 " cycles (~%.2f ns)\n",
               max_jitter_cycles_, max_jitter_cycles_ / 3.0);  // approx 3GHz

        printf("Histogram:\n");
        for (size_t i = 0; i < HISTOGRAM_BUCKETS; ++i) {
            if (histogram_[i] > 0) {
                const uint64_t lo = static_cast<uint64_t>(i) * BUCKET_WIDTH_CYCLES;
                const uint64_t hi = static_cast<uint64_t>(i + 1) * BUCKET_WIDTH_CYCLES;
                printf("[%" PRIu64 "-%" PRIu64 " cycles]: %" PRIu64 "\n", lo, hi, histogram_[i]);
            }
        }
        if (stalled_samples_ > 0) {
            printf("[CRITICAL] System interrupts detected! Check CPU isolation.\n");
        } else {
            printf("[PASS] Clean execution profile.\n");
        }
    }
    
private:
    uint64_t last_tsc_;
    uint64_t max_jitter_cycles_;
    uint64_t total_samples_;
    uint64_t stalled_samples_;
    std::array<uint64_t, HISTOGRAM_BUCKETS> histogram_;
    
    inline uint64_t rdtsc() {
        #if defined(__x86_64__) || defined(_M_X64)
            unsigned int lo, hi;
            __asm__ __volatile__ ("rdtsc" : "=a" (lo), "=d" (hi));
            return ((uint64_t)hi << 32) | lo;
        #elif defined(__aarch64__)
            uint64_t val;
            __asm__ __volatile__("mrs %0, cntvct_el0" : "=r" (val));
            return val;
        #else
            return 0;
        #endif
    }
};

}
