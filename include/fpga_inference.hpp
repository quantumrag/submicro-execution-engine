#pragma once

#include "spin_loop_engine.hpp"
#include "simd_features.hpp"

namespace hft {

struct MicrostructureFeatures {
    double ofi_level_1;
    double ofi_level_5;
    double ofi_level_10;
    double spread_ratio;
    double price_correlation;
    double volume_imbalance;
    double hawkes_buy_intensity;
    double hawkes_sell_intensity;
    double hawkes_imbalance;
    double bid_ask_spread_bps;
    double mid_price_momentum;
    double trade_flow_toxicity;

    MicrostructureFeatures() : ofi_level_1(0.0), ofi_level_5(0.0),
        ofi_level_10(0.0), spread_ratio(1.0), price_correlation(0.0),
        volume_imbalance(0.0), hawkes_buy_intensity(0.0),
        hawkes_sell_intensity(0.0), hawkes_imbalance(0.0),
        bid_ask_spread_bps(0.0), mid_price_momentum(0.0),
        trade_flow_toxicity(0.0) {}

    // Zero-allocation feature extraction info
    static constexpr size_t FEATURE_DIM = 12;
    
    void fill_array(std::array<double, 12>& arr) const {
        arr[0] = ofi_level_1;
        arr[1] = ofi_level_5;
        arr[2] = ofi_level_10;
        arr[3] = spread_ratio;
        arr[4] = price_correlation;
        arr[5] = volume_imbalance;
        arr[6] = hawkes_buy_intensity;
        arr[7] = hawkes_sell_intensity;
        arr[8] = hawkes_imbalance;
        arr[9] = bid_ask_spread_bps;
        arr[10] = mid_price_momentum;
        arr[11] = trade_flow_toxicity;
    }
};

class FPGA_DNN_Inference {
public:
    static constexpr size_t INPUT_DIM = 12;
    static constexpr size_t HIDDEN_DIM = 8;
    static constexpr size_t OUTPUT_DIM = 3;

    explicit FPGA_DNN_Inference()
        : fixed_latency_ns_(400) {

        // Initialize weights with random values (simulation)
        for (auto& w : weights_h_) w = (std::rand() % 200 - 100) / 1000.0;
        for (auto& b : bias_h_) b = 0.0;
        for (auto& w : weights_o_) w = (std::rand() % 200 - 100) / 1000.0;
        bias_o_.fill(0.0);
    }

    std::array<double, 3> predict(const MicrostructureFeatures& features) {
        const Timestamp start = now();

        alignas(64) std::array<double, INPUT_DIM> input;
        features.fill_array(input);

        auto output = forward_pass(input);

        const Timestamp end = now();
        const int64_t elapsed_ns = to_nanos(end) - to_nanos(start);

        if (elapsed_ns < fixed_latency_ns_) {
            while ((to_nanos(now()) - to_nanos(start)) < fixed_latency_ns_) {
                #if defined(__x86_64__)
                    _mm_pause();
                #elif defined(__aarch64__)
                    __asm__ __volatile__("yield");
                #endif
            }
        }

        return output;
    }

    int64_t get_fixed_latency_ns() const {
        return fixed_latency_ns_;
    }

    // Instance method - uses internal SIMD engine for stateful tracking
    MicrostructureFeatures extract_features(
        const MarketTick& current_tick,
        const MarketTick& reference_asset_tick,
        double hawkes_buy_intensity,
        double hawkes_sell_intensity) {

        MicrostructureFeatures features;
        
        // 1. Hardware Accelerated Order Book Features (SIMD)
        // Computes OFI, Imbalance, and Quantities in parallel
        alignas(64) double simd_output[16];
        fast_engine_.calculate_features_fast(
            current_tick.bid_sizes.data(),
            current_tick.ask_sizes.data(),
            current_tick.depth_levels,
            simd_output
        );
        
        // Map SIMD output to features
        // Mapping from FastFeatureEngine:
        // [0]=TotalOFI, [1]=Top1OFI, [2]=VolImb, [3]=Top5OFI, [4]=BestAskQty, [5]=BestBidQty
        features.ofi_level_10 = simd_output[0];   // Total OFI (10 levels)
        features.ofi_level_1 = simd_output[1];    // Top 1 OFI
        features.volume_imbalance = simd_output[2];
        features.ofi_level_5 = simd_output[3];    // Top 5 OFI
        
        // 2. Compute remaining price-based features (Scalar is fine/fast enough here)
        const double current_spread = current_tick.ask_price - current_tick.bid_price;
        const double ref_spread = reference_asset_tick.ask_price - reference_asset_tick.bid_price;
        features.spread_ratio = (ref_spread > 1e-10) ? (current_spread / ref_spread) : 1.0;

        features.hawkes_buy_intensity = hawkes_buy_intensity;
        features.hawkes_sell_intensity = hawkes_sell_intensity;
        features.hawkes_imbalance = (hawkes_buy_intensity + hawkes_sell_intensity > 1e-10) ?
            (hawkes_buy_intensity - hawkes_sell_intensity) / (hawkes_buy_intensity + hawkes_sell_intensity) : 0.0;

        features.bid_ask_spread_bps = (current_tick.mid_price > 1e-10) ?
            (current_spread / current_tick.mid_price) * 10000.0 : 0.0;
            
        // Track momentum (using internal state of simple price)
        features.mid_price_momentum = current_tick.mid_price - last_mid_price_;
        last_mid_price_ = current_tick.mid_price;

        if (current_tick.trade_volume > 0) {
            // Approximation: price moved / volume
            features.trade_flow_toxicity = std::abs(features.mid_price_momentum) / static_cast<double>(current_tick.trade_volume);
        } else {
            features.trade_flow_toxicity = 0.0;
        }

        return features;
    }

private:
    hft::simd_features::FastFeatureEngine fast_engine_;
    double last_mid_price_ = 0.0;

    static double compute_ofi(const MarketTick& current,
                             const MarketTick& previous,
                             size_t depth) {
        double ofi = 0.0;
        const size_t levels = std::min(depth, static_cast<size_t>(current.depth_levels));

        for (size_t iter = 0; iter < levels; ++iter) {
            const int64_t bid_delta = static_cast<int64_t>(current.bid_sizes[iter]) -
                                      static_cast<int64_t>(previous.bid_sizes[iter]);

            const int64_t ask_delta = static_cast<int64_t>(current.ask_sizes[iter]) -
                                      static_cast<int64_t>(previous.ask_sizes[iter]);

            const double weight = 1.0 / (iter + 1.0);
            ofi += weight * (bid_delta - ask_delta);
        }

        return ofi;
    }

    std::array<double, 3> forward_pass(const std::array<double, INPUT_DIM>& input) const {
        alignas(64) std::array<double, HIDDEN_DIM> hidden;
        
        for (size_t iter = 0; iter < HIDDEN_DIM; ++iter) {
            double aggregate = bias_h_[iter];
            for (size_t col = 0; col < INPUT_DIM; ++col) {
                aggregate += weights_h_[iter * INPUT_DIM + col] * input[col];
            }
            hidden[iter] = std::max(0.0, aggregate); // ReLU
        }

        std::array<double, 3> output;
        for (size_t iter = 0; iter < 3; ++iter) {
            output[iter] = bias_o_[iter];
            for (size_t col = 0; col < HIDDEN_DIM; ++col) {
                output[iter] += weights_o_[iter * HIDDEN_DIM + col] * hidden[col];
            }
        }

        // Fast Softmax
        double max_val = std::max({output[0], output[1], output[2]});
        double sum_exp = 0.0;
        for (size_t i = 0; i < 3; ++i) {
            output[i] = hft::spin_loop::fast_exp(output[i] - max_val);
            sum_exp += output[i];
        }
        const double inv_sum = 1.0 / sum_exp;
        for (size_t i = 0; i < 3; ++i) {
            output[i] *= inv_sum;
        }

        return output;
    }

    int64_t fixed_latency_ns_;
    alignas(64) std::array<double, HIDDEN_DIM * INPUT_DIM> weights_h_;
    alignas(64) std::array<double, HIDDEN_DIM> bias_h_;
    alignas(64) std::array<double, OUTPUT_DIM * HIDDEN_DIM> weights_o_;
    alignas(64) std::array<double, OUTPUT_DIM> bias_o_;
};

} // namespace hft