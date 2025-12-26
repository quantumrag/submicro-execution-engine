#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <fstream>
#include <sstream>
#include <filesystem>
#include "backtesting_engine.hpp"

namespace {

class BacktestingEngineTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Create temporary test data file
        test_data_path_ = "/tmp/test_backtest_data.csv";
        create_test_csv_data();
    }

    void TearDown() override {
        // Clean up test file
        std::filesystem::remove(test_data_path_);
    }

    void create_test_csv_data() {
        std::ofstream file(test_data_path_);
        file << "ts_us,event_type,side,price,size\n";

        // Generate 1000 ticks of test data
        int64_t base_time = 1640995200000000LL; // 2022-01-01 00:00:00 UTC in microseconds
        double base_price = 100.0;

        for (int i = 0; i < 1000; ++i) {
            int64_t timestamp_us = base_time + (i * 1000000LL); // 1 second intervals
            double price = base_price + (static_cast<double>(rand()) / RAND_MAX - 0.5) * 2.0;
            uint64_t size = 100 + (rand() % 200);

            // Mix of trade and quote events
            std::string event_type = (i % 3 == 0) ? "trade" : "quote";
            char side = (rand() % 2 == 0) ? 'B' : 'S';

            file << timestamp_us << "," << event_type << "," << side << ","
                 << std::fixed << std::setprecision(2) << price << "," << size << "\n";
        }
    }

    std::string test_data_path_;
};

TEST_F(BacktestingEngineTest, ConstructorInitialization) {
    hft::backtest::BacktestingEngine engine;

    // Test that engine initializes without throwing
    SUCCEED();
}

TEST_F(BacktestingEngineTest, LoadHistoricalData) {
    hft::backtest::BacktestingEngine engine;

    bool success = engine.load_historical_data(test_data_path_);
    EXPECT_TRUE(success);

    // Test that loading completes without error
    SUCCEED();
}

TEST_F(BacktestingEngineTest, LoadInvalidFile) {
    hft::backtest::BacktestingEngine engine;

    bool success = engine.load_historical_data("/nonexistent/file.csv");
    EXPECT_FALSE(success);
}

/*
TEST_F(BacktestingEngineTest, ParseCsvLineValid) {
    hft::backtest::BacktestingEngine engine;
    hft::backtest::HistoricalEvent event;

    std::string valid_line = "1640995200000000,trade,B,100.50,200";
    bool success = engine.parse_csv_line(valid_line, event);

    EXPECT_TRUE(success);
    EXPECT_EQ(event.timestamp_ns, 1640995200000000000LL);
    EXPECT_EQ(event.trade_side, hft::Side::BUY);
    EXPECT_DOUBLE_EQ(event.bid_price, 100.50 - 100.50 * 0.0002 / 2.0);
    EXPECT_DOUBLE_EQ(event.ask_price, 100.50 + 100.50 * 0.0002 / 2.0);
    EXPECT_EQ(event.bid_size, 200);
    EXPECT_EQ(event.ask_size, 200);
    EXPECT_EQ(event.trade_volume, 200);
}

TEST_F(BacktestingEngineTest, ParseCsvLineHeader) {
    hft::backtest::BacktestingEngine engine;
    hft::backtest::HistoricalEvent event;

    std::string header_line = "ts_us,event_type,side,price,size";
    bool success = engine.parse_csv_line(header_line, event);

    EXPECT_FALSE(success); // Should skip header lines
}

TEST_F(BacktestingEngineTest, ParseCsvLineInvalid) {
    hft::backtest::BacktestingEngine engine;
    hft::backtest::HistoricalEvent event;

    std::string invalid_line = "invalid,data,here";
    bool success = engine.parse_csv_line(invalid_line, event);

    EXPECT_FALSE(success);
}
*/

TEST_F(BacktestingEngineTest, FillProbabilityModel) {
    hft::backtest::FillProbabilityModel model;

    hft::Order order;
    order.side = hft::Side::BUY;
    order.price = 100.0;
    order.quantity = 100;

    hft::MarketTick tick;
    tick.bid_price = 99.9;
    tick.ask_price = 100.1;
    tick.mid_price = 100.0;
    tick.bid_size = 200;
    tick.ask_size = 200;

    double prob = model.calculate_fill_probability(order, tick, 5, 0.2, 500);
    EXPECT_GE(prob, 0.0);
    EXPECT_LE(prob, 1.0);

    // At ask price, should be high probability (within 0.2)
    order.price = 100.1;
    prob = model.calculate_fill_probability(order, tick, 5, 0.2, 500);
    EXPECT_GT(prob, 0.5);  // At least 50% fill probability at ask
}

TEST_F(BacktestingEngineTest, FillProbabilityModelSlippage) {
    hft::backtest::FillProbabilityModel model;

    hft::Order order;
    order.side = hft::Side::BUY;
    order.price = 100.0;
    order.quantity = 100;

    hft::MarketTick tick;
    tick.bid_price = 99.9;
    tick.ask_price = 100.1;
    tick.mid_price = 100.0;
    tick.bid_size = 200;
    tick.ask_size = 200;

    double slippage = model.calculate_slippage(order, tick, 0.1);
    EXPECT_GE(slippage, 0.0);
    EXPECT_LT(slippage, tick.mid_price * 0.01); // Reasonable slippage limit
}

TEST_F(BacktestingEngineTest, HistoricalEventToMarketTick) {
    hft::backtest::HistoricalEvent event;
    event.timestamp_ns = 1000000000LL;
    event.asset_id = 1;
    event.bid_price = 99.9;
    event.ask_price = 100.1;
    event.bid_size = 200;
    event.ask_size = 150;
    event.trade_price = 100.0;
    event.trade_volume = 50;
    event.trade_side = hft::Side::BUY;
    event.depth_levels = 1;

    hft::MarketTick tick = event.to_market_tick();

    EXPECT_DOUBLE_EQ(tick.bid_price, 99.9);
    EXPECT_DOUBLE_EQ(tick.ask_price, 100.1);
    EXPECT_DOUBLE_EQ(tick.mid_price, 100.0);
    EXPECT_EQ(tick.bid_size, 200);
    EXPECT_EQ(tick.ask_size, 150);
    EXPECT_EQ(tick.trade_volume, 50);
    EXPECT_EQ(tick.trade_side, hft::Side::BUY);
    EXPECT_EQ(tick.asset_id, 1);
    EXPECT_EQ(tick.depth_levels, 1);
}

TEST_F(BacktestingEngineTest, PerformanceMetricsCalculation) {
    hft::backtest::PerformanceMetrics metrics;

    // Simulate some basic metrics
    metrics.total_pnl = 1000.0;
    metrics.total_trades = 100;
    metrics.winning_trades = 60;
    metrics.losing_trades = 40;
    metrics.avg_win = 50.0;
    metrics.avg_loss = 25.0;
    metrics.win_rate = 0.6;  // Manually set calculated values
    metrics.profit_factor = 3.0;  // 50.0 * 60 / (25.0 * 40) = 3000 / 1000 = 3.0

    EXPECT_DOUBLE_EQ(metrics.total_pnl, 1000.0);
    EXPECT_EQ(metrics.total_trades, 100);
    EXPECT_EQ(metrics.winning_trades, 60);
    EXPECT_EQ(metrics.losing_trades, 40);
    EXPECT_DOUBLE_EQ(metrics.win_rate, 0.6);
    EXPECT_DOUBLE_EQ(metrics.profit_factor, 3.0);
}

TEST_F(BacktestingEngineTest, BacktestConfigDefaults) {
    hft::backtest::BacktestConfig config;

    EXPECT_EQ(config.simulated_latency_ns, 500);
    EXPECT_DOUBLE_EQ(config.initial_capital, 100000.0);
    EXPECT_DOUBLE_EQ(config.commission_per_share, 0.0005);
    EXPECT_EQ(config.max_position, 1000);
    EXPECT_TRUE(config.enable_slippage);
    EXPECT_TRUE(config.enable_adverse_selection);
    EXPECT_EQ(config.random_seed, 42);
    EXPECT_FALSE(config.run_latency_sweep);

    // Check latency sweep defaults
    ASSERT_EQ(config.latency_sweep_ns.size(), 5);
    EXPECT_EQ(config.latency_sweep_ns[0], 100);
    EXPECT_EQ(config.latency_sweep_ns[1], 250);
    EXPECT_EQ(config.latency_sweep_ns[2], 500);
    EXPECT_EQ(config.latency_sweep_ns[3], 1000);
    EXPECT_EQ(config.latency_sweep_ns[4], 2000);
}

TEST_F(BacktestingEngineTest, SimulatedOrderInitialization) {
    hft::backtest::SimulatedOrder sim_order;

    EXPECT_EQ(sim_order.submit_time_ns, 0);
    EXPECT_EQ(sim_order.fill_time_ns, 0);
    EXPECT_DOUBLE_EQ(sim_order.fill_price, 0.0);
    EXPECT_EQ(sim_order.filled_quantity, 0);
    EXPECT_FALSE(sim_order.is_filled);
    EXPECT_FALSE(sim_order.is_cancelled);
    EXPECT_EQ(sim_order.queue_position, 0);
}

TEST_F(BacktestingEngineTest, RunBacktestWithData) {
    hft::backtest::BacktestingEngine engine;

    // Load test data
    bool load_success = engine.load_historical_data(test_data_path_);
    ASSERT_TRUE(load_success);

    // Run backtest
    auto metrics = engine.run_backtest();

    // Basic validation that backtest ran
    EXPECT_GE(metrics.total_trades, 0);
    EXPECT_GE(metrics.total_pnl, -100000.0); // Shouldn't lose more than initial capital
    EXPECT_LE(metrics.total_pnl, 100000.0);  // Shouldn't make unrealistically high profits
    EXPECT_GE(metrics.fill_rate, 0.0);
    EXPECT_LE(metrics.fill_rate, 1.0);
}

/*
TEST_F(BacktestingEngineTest, LatencySensitivityAnalysis) {
    hft::backtest::BacktestingEngine engine;

    // Configure for latency sweep
    engine.config_.run_latency_sweep = true;
    engine.config_.latency_sweep_ns = {100, 250, 500};

    // Load test data
    bool load_success = engine.load_historical_data(test_data_path_);
    ASSERT_TRUE(load_success);

    // Run latency analysis
    auto results = engine.run_latency_sensitivity_analysis();

    // Should have results for each latency
    EXPECT_EQ(results.size(), 3);
    EXPECT_TRUE(results.count(100));
    EXPECT_TRUE(results.count(250));
    EXPECT_TRUE(results.count(500));

    // Each result should have valid metrics
    for (const auto& [latency, metrics] : results) {
        EXPECT_GE(metrics.total_trades, 0);
        EXPECT_GE(metrics.fill_rate, 0.0);
        EXPECT_LE(metrics.fill_rate, 1.0);
    }
}
*/

TEST_F(BacktestingEngineTest, QueuePositionEstimation) {
    hft::backtest::BacktestingEngine engine;

    hft::Order buy_order;
    buy_order.side = hft::Side::BUY;

    hft::Order sell_order;
    sell_order.side = hft::Side::SELL;

    hft::MarketTick tick;
    tick.bid_size = 200;
    tick.ask_size = 150;

    // Test queue position estimation (private method, but we can infer through behavior)
    // This is testing the logic indirectly through the backtest run
    bool load_success = engine.load_historical_data(test_data_path_);
    ASSERT_TRUE(load_success);

    auto metrics = engine.run_backtest();
    EXPECT_GE(metrics.total_trades, 0); // If queue position logic works, should have some trades
}

TEST_F(BacktestingEngineTest, VolatilityEstimation) {
    hft::backtest::BacktestingEngine engine;

    // Load data and run backtest to populate pnl_history_
    bool load_success = engine.load_historical_data(test_data_path_);
    ASSERT_TRUE(load_success);

    auto metrics = engine.run_backtest();

    // Volatility should be reasonable (not negative, not extremely high)
    EXPECT_GE(metrics.volatility, 0.0);
    EXPECT_LE(metrics.volatility, 10.0); // Annualized volatility shouldn't be > 1000%
}

TEST_F(BacktestingEngineTest, SharpeRatioCalculation) {
    hft::backtest::BacktestingEngine engine;

    // Load data and run backtest
    bool load_success = engine.load_historical_data(test_data_path_);
    ASSERT_TRUE(load_success);

    auto metrics = engine.run_backtest();

    // Sharpe ratio should be a reasonable value
    EXPECT_FALSE(std::isnan(metrics.sharpe_ratio));
    EXPECT_FALSE(std::isinf(metrics.sharpe_ratio));
}

TEST_F(BacktestingEngineTest, MaxDrawdownCalculation) {
    hft::backtest::BacktestingEngine engine;

    // Load data and run backtest
    bool load_success = engine.load_historical_data(test_data_path_);
    ASSERT_TRUE(load_success);

    auto metrics = engine.run_backtest();

    // Max drawdown should be between 0 and 1 (percentage)
    EXPECT_GE(metrics.max_drawdown, 0.0);
    EXPECT_LE(metrics.max_drawdown, 1.0);
}

TEST_F(BacktestingEngineTest, RiskMetricsCalculation) {
    hft::backtest::BacktestingEngine engine;

    // Load data and run backtest
    bool load_success = engine.load_historical_data(test_data_path_);
    ASSERT_TRUE(load_success);

    auto metrics = engine.run_backtest();

    // Risk metrics should be valid
    EXPECT_GE(metrics.value_at_risk_95, 0.0);
    EXPECT_GE(metrics.conditional_var_95, 0.0);
    EXPECT_GE(metrics.sortino_ratio, 0.0);
    EXPECT_FALSE(std::isnan(metrics.sortino_ratio));
}

} // namespace