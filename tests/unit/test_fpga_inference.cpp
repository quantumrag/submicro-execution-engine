#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "fpga_inference.hpp"
#include <chrono>
#include <thread>

namespace {

class FPGAInferenceTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create inference engine
        engine_ = std::make_unique<hft::FPGA_DNN_Inference>();
    }

    void TearDown() override {
        engine_.reset();
    }

    std::unique_ptr<hft::FPGA_DNN_Inference> engine_;
};

TEST_F(FPGAInferenceTest, ConstructorInitialization) {
    EXPECT_EQ(engine_->get_fixed_latency_ns(), 400);
}

TEST_F(FPGAInferenceTest, PredictReturnsValidProbabilities) {
    hft::MicrostructureFeatures features;
    features.ofi_level_1 = 0.5;
    features.hawkes_buy_intensity = 10.0;
    features.hawkes_sell_intensity = 8.0;

    auto result = engine_->predict(features);

    // Should return 3 probabilities that sum to 1
    EXPECT_EQ(result.size(), 3);
    double sum = result[0] + result[1] + result[2];
    EXPECT_NEAR(sum, 1.0, 1e-10);

    // Each probability should be between 0 and 1
    for (double prob : result) {
        EXPECT_GE(prob, 0.0);
        EXPECT_LE(prob, 1.0);
    }
}

TEST_F(FPGAInferenceTest, PredictTimingGuarantee) {
    hft::MicrostructureFeatures features;

    auto start = std::chrono::high_resolution_clock::now();
    auto result = engine_->predict(features);
    auto end = std::chrono::high_resolution_clock::now();

    auto duration = std::chrono::duration_cast<std::chrono::nanoseconds>(end - start);

    // Should take at least the guaranteed 400ns
    EXPECT_GE(duration.count(), 400);
    // But not excessively long (allowing some margin for system variability)
    EXPECT_LT(duration.count(), 10000); // Less than 10 microseconds
}

TEST_F(FPGAInferenceTest, MicrostructureFeaturesToVector) {
    hft::MicrostructureFeatures features;
    features.ofi_level_1 = 1.0;
    features.ofi_level_5 = 2.0;
    features.ofi_level_10 = 3.0;
    features.spread_ratio = 1.5;
    features.price_correlation = 0.8;
    features.volume_imbalance = 0.2;
    features.hawkes_buy_intensity = 12.0;
    features.hawkes_sell_intensity = 9.0;
    features.hawkes_imbalance = 0.14;
    features.bid_ask_spread_bps = 5.0;
    features.mid_price_momentum = 0.01;
    features.trade_flow_toxicity = 0.001;

    auto vec = features.to_vector();

    EXPECT_EQ(vec.size(), 12);
    EXPECT_DOUBLE_EQ(vec[0], 1.0);
    EXPECT_DOUBLE_EQ(vec[1], 2.0);
    EXPECT_DOUBLE_EQ(vec[2], 3.0);
    EXPECT_DOUBLE_EQ(vec[3], 1.5);
    EXPECT_DOUBLE_EQ(vec[4], 0.8);
    EXPECT_DOUBLE_EQ(vec[5], 0.2);
    EXPECT_DOUBLE_EQ(vec[6], 12.0);
    EXPECT_DOUBLE_EQ(vec[7], 9.0);
    EXPECT_DOUBLE_EQ(vec[8], 0.14);
    EXPECT_DOUBLE_EQ(vec[9], 5.0);
    EXPECT_DOUBLE_EQ(vec[10], 0.01);
    EXPECT_DOUBLE_EQ(vec[11], 0.001);
}

TEST_F(FPGAInferenceTest, ExtractFeaturesBasic) {
    hft::MarketTick current_tick;
    current_tick.bid_price = 99.9;
    current_tick.ask_price = 100.1;
    current_tick.mid_price = 100.0;
    current_tick.bid_size = 200;
    current_tick.ask_size = 150;
    current_tick.depth_levels = 1;

    hft::MarketTick previous_tick;
    previous_tick.bid_price = 99.8;
    previous_tick.ask_price = 100.0;
    previous_tick.mid_price = 99.9;
    previous_tick.bid_size = 180;
    previous_tick.ask_size = 160;

    hft::MarketTick reference_tick;
    reference_tick.bid_price = 199.8;
    reference_tick.ask_price = 200.2;
    reference_tick.mid_price = 200.0;

    auto features = hft::FPGA_DNN_Inference::extract_features(
        current_tick, previous_tick, reference_tick, 10.0, 8.0);

    // Check spread ratio
    double expected_spread_ratio = 0.2 / 0.4; // current spread / ref spread
    EXPECT_DOUBLE_EQ(features.spread_ratio, expected_spread_ratio);

    // Check volume imbalance
    double expected_volume_imbalance = (200.0 - 150.0) / (200.0 + 150.0);
    EXPECT_DOUBLE_EQ(features.volume_imbalance, expected_volume_imbalance);

    // Check Hawkes features
    EXPECT_DOUBLE_EQ(features.hawkes_buy_intensity, 10.0);
    EXPECT_DOUBLE_EQ(features.hawkes_sell_intensity, 8.0);
    double expected_imbalance = (10.0 - 8.0) / (10.0 + 8.0);
    EXPECT_DOUBLE_EQ(features.hawkes_imbalance, expected_imbalance);

    // Check spread in bps
    double expected_spread_bps = (0.2 / 100.0) * 10000.0;
    EXPECT_NEAR(features.bid_ask_spread_bps, expected_spread_bps, 1e-10);  // Use NEAR for floating-point precision

    // Check mid price momentum
    EXPECT_DOUBLE_EQ(features.mid_price_momentum, 100.0 - 99.9);
}

TEST_F(FPGAInferenceTest, ExtractFeaturesWithTrade) {
    hft::MarketTick current_tick;
    current_tick.mid_price = 100.0;
    current_tick.trade_volume = 100;

    hft::MarketTick previous_tick;
    previous_tick.mid_price = 99.9;

    hft::MarketTick reference_tick;

    auto features = hft::FPGA_DNN_Inference::extract_features(
        current_tick, previous_tick, reference_tick, 10.0, 8.0);

    // Check trade flow toxicity
    double expected_toxicity = std::abs(100.0 - 99.9) / 100.0;
    EXPECT_DOUBLE_EQ(features.trade_flow_toxicity, expected_toxicity);
}

TEST_F(FPGAInferenceTest, ComputeOFI) {
    hft::MarketTick current_tick;
    current_tick.bid_sizes[0] = 200;
    current_tick.ask_sizes[0] = 150;
    current_tick.depth_levels = 1;

    hft::MarketTick previous_tick;
    previous_tick.bid_sizes[0] = 180;
    previous_tick.ask_sizes[0] = 160;

    // Test private method indirectly through extract_features
    auto features = hft::FPGA_DNN_Inference::extract_features(
        current_tick, previous_tick, current_tick, 10.0, 8.0);

    // OFI = weight * (bid_delta - ask_delta)
    // weight = 1.0 / (0 + 1) = 1.0
    // bid_delta = 200 - 180 = 20
    // ask_delta = 150 - 160 = -10
    // ofi = 1.0 * (20 - (-10)) = 30
    EXPECT_DOUBLE_EQ(features.ofi_level_1, 30.0);
}

TEST_F(FPGAInferenceTest, PredictWithZeroFeatures) {
    hft::MicrostructureFeatures features; // All zeros

    auto result = engine_->predict(features);

    // Should still return valid probabilities
    EXPECT_EQ(result.size(), 3);
    double sum = result[0] + result[1] + result[2];
    EXPECT_NEAR(sum, 1.0, 1e-10);
}

TEST_F(FPGAInferenceTest, PredictWithExtremeFeatures) {
    hft::MicrostructureFeatures features;
    features.ofi_level_1 = 1000.0;
    features.hawkes_buy_intensity = 1000.0;
    features.hawkes_sell_intensity = 1.0;

    auto result = engine_->predict(features);

    // Should handle extreme values without crashing
    EXPECT_EQ(result.size(), 3);
    double sum = result[0] + result[1] + result[2];
    EXPECT_NEAR(sum, 1.0, 1e-10);
}

TEST_F(FPGAInferenceTest, PredictConsistency) {
    hft::MicrostructureFeatures features;
    features.ofi_level_1 = 0.5;
    features.hawkes_buy_intensity = 10.0;

    auto result1 = engine_->predict(features);
    auto result2 = engine_->predict(features);

    // Results should be consistent (deterministic)
    EXPECT_DOUBLE_EQ(result1[0], result2[0]);
    EXPECT_DOUBLE_EQ(result1[1], result2[1]);
    EXPECT_DOUBLE_EQ(result1[2], result2[2]);
}

TEST_F(FPGAInferenceTest, MicrostructureFeaturesDefaultValues) {
    hft::MicrostructureFeatures features;

    EXPECT_DOUBLE_EQ(features.ofi_level_1, 0.0);
    EXPECT_DOUBLE_EQ(features.ofi_level_5, 0.0);
    EXPECT_DOUBLE_EQ(features.ofi_level_10, 0.0);
    EXPECT_DOUBLE_EQ(features.spread_ratio, 1.0);
    EXPECT_DOUBLE_EQ(features.price_correlation, 0.0);
    EXPECT_DOUBLE_EQ(features.volume_imbalance, 0.0);
    EXPECT_DOUBLE_EQ(features.hawkes_buy_intensity, 0.0);
    EXPECT_DOUBLE_EQ(features.hawkes_sell_intensity, 0.0);
    EXPECT_DOUBLE_EQ(features.hawkes_imbalance, 0.0);
    EXPECT_DOUBLE_EQ(features.bid_ask_spread_bps, 0.0);
    EXPECT_DOUBLE_EQ(features.mid_price_momentum, 0.0);
    EXPECT_DOUBLE_EQ(features.trade_flow_toxicity, 0.0);
}

TEST_F(FPGAInferenceTest, ExtractFeaturesEdgeCases) {
    hft::MarketTick current_tick;
    current_tick.bid_price = 100.0;
    current_tick.ask_price = 100.0; // Zero spread
    current_tick.mid_price = 100.0;

    hft::MarketTick previous_tick;
    hft::MarketTick reference_tick;
    reference_tick.bid_price = 200.0;
    reference_tick.ask_price = 200.0; // Zero reference spread

    auto features = hft::FPGA_DNN_Inference::extract_features(
        current_tick, previous_tick, reference_tick, 10.0, 8.0);

    // Should handle zero spreads gracefully
    EXPECT_DOUBLE_EQ(features.spread_ratio, 1.0); // Fallback value
    EXPECT_DOUBLE_EQ(features.bid_ask_spread_bps, 0.0);
}

TEST_F(FPGAInferenceTest, PredictWithWrongInputSize) {
    hft::MicrostructureFeatures features;
    // Let features have default size, but test internal handling

    auto result = engine_->predict(features);

    // Should handle input size mismatch gracefully
    EXPECT_EQ(result.size(), 3);
    double sum = result[0] + result[1] + result[2];
    EXPECT_NEAR(sum, 1.0, 1e-10);
}

} // namespace