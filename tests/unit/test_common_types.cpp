#include <gtest/gtest.h>
#include "common_types.hpp"
#include <thread>
#include <chrono>

// Test fixture for Common Types tests
class CommonTypesTest : public ::testing::Test {
protected:
    void SetUp() override {
        // Setup if needed
    }

    void TearDown() override {
        // Cleanup if needed
    }
};

// Test timestamp utilities
TEST_F(CommonTypesTest, TimestampUtilities) {
    using namespace hft;

    // Test now() function
    Timestamp t1 = now();
    std::this_thread::sleep_for(std::chrono::milliseconds(1));
    Timestamp t2 = now();

    EXPECT_LT(t1, t2);

    // Test to_nanos conversion
    int64_t nanos1 = to_nanos(t1);
    int64_t nanos2 = to_nanos(t2);

    EXPECT_LT(nanos1, nanos2);
    EXPECT_GT(nanos2 - nanos1, 1000000); // At least 1ms difference
}

// Test Side enum
TEST_F(CommonTypesTest, SideEnum) {
    using namespace hft;

    EXPECT_EQ(static_cast<uint8_t>(Side::BUY), 0);
    EXPECT_EQ(static_cast<uint8_t>(Side::SELL), 1);

    Side buy = Side::BUY;
    Side sell = Side::SELL;

    EXPECT_EQ(buy, Side::BUY);
    EXPECT_EQ(sell, Side::SELL);
    EXPECT_NE(buy, sell);
}

// Test MarketTick structure
TEST_F(CommonTypesTest, MarketTickStructure) {
    using namespace hft;

    MarketTick tick;

    // Test default initialization
    EXPECT_DOUBLE_EQ(tick.bid_price, 0.0);
    EXPECT_DOUBLE_EQ(tick.ask_price, 0.0);
    EXPECT_DOUBLE_EQ(tick.mid_price, 0.0);
    EXPECT_EQ(tick.bid_size, 0ULL);
    EXPECT_EQ(tick.ask_size, 0ULL);
    EXPECT_EQ(tick.trade_volume, 0ULL);
    EXPECT_EQ(tick.trade_side, Side::BUY);
    EXPECT_EQ(tick.asset_id, 0U);
    EXPECT_EQ(tick.depth_levels, 0);

    // Test array initialization
    for (size_t i = 0; i < 10; ++i) {
        EXPECT_DOUBLE_EQ(tick.bid_prices[i], 0.0);
        EXPECT_DOUBLE_EQ(tick.ask_prices[i], 0.0);
        EXPECT_EQ(tick.bid_sizes[i], 0ULL);
        EXPECT_EQ(tick.ask_sizes[i], 0ULL);
    }

    // Test timestamp is set
    EXPECT_GT(to_nanos(tick.timestamp), 0);
}

// Test Order structure
TEST_F(CommonTypesTest, OrderStructure) {
    using namespace hft;

    // Test default constructor
    Order order1;
    EXPECT_EQ(order1.order_id, 0ULL);
    EXPECT_EQ(order1.asset_id, 0U);
    EXPECT_EQ(order1.side, Side::BUY);
    EXPECT_DOUBLE_EQ(order1.price, 0.0);
    EXPECT_EQ(order1.quantity, 0ULL);
    EXPECT_EQ(order1.venue_id, 0);
    EXPECT_FALSE(order1.is_active);

    // Test parameterized constructor
    Order order2(123ULL, 456U, Side::SELL, 99.50, 1000ULL);
    EXPECT_EQ(order2.order_id, 123ULL);
    EXPECT_EQ(order2.asset_id, 456U);
    EXPECT_EQ(order2.side, Side::SELL);
    EXPECT_DOUBLE_EQ(order2.price, 99.50);
    EXPECT_EQ(order2.quantity, 1000ULL);
    EXPECT_TRUE(order2.is_active);
}

// Test QuotePair structure
TEST_F(CommonTypesTest, QuotePairStructure) {
    using namespace hft;

    QuotePair quote;

    // Test default initialization
    EXPECT_DOUBLE_EQ(quote.bid_price, 0.0);
    EXPECT_DOUBLE_EQ(quote.ask_price, 0.0);
    EXPECT_DOUBLE_EQ(quote.bid_size, 0.0);
    EXPECT_DOUBLE_EQ(quote.ask_size, 0.0);
    EXPECT_DOUBLE_EQ(quote.spread, 0.0);
    EXPECT_DOUBLE_EQ(quote.mid_price, 0.0);

    // Test timestamp is set
    EXPECT_GT(to_nanos(quote.generated_at), 0);
}

// Test TradingEvent structure
TEST_F(CommonTypesTest, TradingEventStructure) {
    using namespace hft;

    // Test default constructor
    TradingEvent event1;
    EXPECT_EQ(event1.event_type, Side::BUY);
    EXPECT_EQ(event1.asset_id, 0U);
    EXPECT_DOUBLE_EQ(event1.intensity, 0.0);

    // Test parameterized constructor
    Timestamp test_time = now();
    TradingEvent event2(test_time, Side::SELL, 789U);
    EXPECT_EQ(event2.arrival_time, test_time);
    EXPECT_EQ(event2.event_type, Side::SELL);
    EXPECT_EQ(event2.asset_id, 789U);
    EXPECT_DOUBLE_EQ(event2.intensity, 0.0);
}

// Test MarketRegime enum
TEST_F(CommonTypesTest, MarketRegimeEnum) {
    using namespace hft;

    EXPECT_EQ(static_cast<uint8_t>(MarketRegime::NORMAL), 0);
    EXPECT_EQ(static_cast<uint8_t>(MarketRegime::ELEVATED_VOLATILITY), 1);
    EXPECT_EQ(static_cast<uint8_t>(MarketRegime::HIGH_STRESS), 2);
    EXPECT_EQ(static_cast<uint8_t>(MarketRegime::HALTED), 3);

    MarketRegime normal = MarketRegime::NORMAL;
    MarketRegime halted = MarketRegime::HALTED;

    EXPECT_EQ(normal, MarketRegime::NORMAL);
    EXPECT_EQ(halted, MarketRegime::HALTED);
    EXPECT_NE(normal, halted);
}

// Test cache-line alignment
TEST_F(CommonTypesTest, CacheLineAlignment) {
    using namespace hft;

    // Test that MarketTick is 64-byte aligned
    MarketTick tick;
    EXPECT_EQ(reinterpret_cast<uintptr_t>(&tick) % 64, 0ULL);

    // Test that Order is 64-byte aligned
    Order order;
    EXPECT_EQ(reinterpret_cast<uintptr_t>(&order) % 64, 0ULL);
}

// Test structure sizes (for memory layout verification)
TEST_F(CommonTypesTest, StructureSizes) {
    using namespace hft;

    // MarketTick should be 64-byte aligned and reasonably sized
    EXPECT_GE(sizeof(MarketTick), 64);
    EXPECT_LE(sizeof(MarketTick), 1024); // Reasonable upper bound

    // Order should be 64-byte aligned
    EXPECT_GE(sizeof(Order), 64);
    EXPECT_LE(sizeof(Order), 128); // Reasonable upper bound

    // Other structures should be reasonably sized
    EXPECT_GT(sizeof(QuotePair), 0);
    EXPECT_GT(sizeof(TradingEvent), 0);
}

int main(int argc, char **argv) {
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}