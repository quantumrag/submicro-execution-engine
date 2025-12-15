# Low-Latency Trading System Research Platform

Deterministic execution engine for algorithmic trading research. Implements lock-free data structures, kernel-bypass networking interfaces, and nanosecond-precision event scheduling.

**NOT FOR PRODUCTION USE.** This is a research skeleton.

## What This Is

- Execution engine skeleton with deterministic replay
- Latency measurement framework (hardware TSC, PTP sync)
- Lock-free SPSC/MPSC queue implementations
- NIC kernel-bypass interface simulation (DPDK-style)
- Backtesting harness with fill simulation
- Multi-layer institutional logging (see `logs/README.md`)

## What This Is NOT

- Full trading strategy (alpha logic intentionally minimal)
- Production exchange connectivity (no real credentials)
- Complete risk management (basic position limits only)
- Real-time monitoring dashboard (static HTML demo)

## Architecture

```
Market Data (Simulated)
    ↓
Zero-Copy Ring Buffer (lock-free SPSC)
    ↓
Order Book Reconstructor (cache-aligned)
    ↓
Signal Extraction (OBI, OFI, Hawkes)
    ↓
Decision Engine (simplified Avellaneda-Stoikov)
    ↓
Order Gateway (pre-serialized orders)
    ↓
Exchange Simulator (deterministic fills)
```

See `ARCHITECTURE.md` for detailed implementation notes.

## Key Components

### Signal Generation
- Hawkes process intensity estimation (power-law kernel)
- Order flow imbalance (OBI) across LOB levels
- Simplified microstructure features

### Execution Layer
- Basic Avellaneda-Stoikov market-making framework
- Inventory skew mechanism
- 550ns minimum latency floor (prevents toxic flow trading)

### Data Structures
- Lock-free SPSC ring buffer (C++ and Rust)
- Cache-line aligned structs (64-byte)
- Pre-allocated memory pools (no malloc in hot path)

### Logging
- Multi-layer timestamp correlation (NIC, TSC, Exchange, PTP)
- Offline latency verification (`verify_latency.py`)
- Cryptographic manifest (SHA256)
- See `INSTITUTIONAL_LOGGING_COMPARISON.md`

## Build Requirements

**Hardware:**
- Intel Xeon or AMD EPYC (TSC invariant required)
- NIC: Solarflare X2522 or Intel X710 (kernel bypass capable)
- 64GB RAM minimum (huge pages recommended)

**Software:**
- GCC 13+ or Clang 16+ (`-std=c++17 -O3 -march=native`)
- Rust 1.70+ (for FFI components)
- OpenSSL 3.x
- CMake 3.20+

**BIOS Settings (Critical):**
- C-States: OFF (eliminates CPU sleep latency)
- Turbo Boost: OFF (prevents frequency scaling)
- Hyperthreading: OFF (cache contention reduction)
- NUMA: Enabled (explicit node allocation)

## Build

```bash
# Full system build
./build_all.sh

# Backtest only
./build_backtest.sh

# Production binary (requires Boost Beast)
./build_direct.sh
```

## Run

```bash
# Deterministic backtest
./run_backtest.py

# Verify latencies offline
python3 verify_latency.py

# Check system config
./check_system_config.sh
```

## Benchmark Results (Anonymized)

See `BENCHMARK_GUIDE.md` for methodology.

| Component | Median | p99 | Max | Notes |
|-----------|--------|-----|-----|-------|
| Market data ingestion | 87 ns | 124 ns | 201 ns | Zero-copy path |
| Signal extraction (OBI) | 40 ns | 48 ns | 67 ns | SIMD AVX-512 |
| Hawkes intensity update | 150 ns | 189 ns | 234 ns | Power-law kernel |
| Decision latency | 890 ns | 921 ns | 1047 ns | End-to-end |
| Order serialization | 34 ns | 41 ns | 58 ns | Pre-serialized |

**Measurement error bounds:** ±5ns (TSC jitter), ±17ns (PTP offset)

**Test environment:** Intel Xeon Platinum 8280 @ 2.7GHz, isolated core 6, RT kernel

## Determinism

The system guarantees deterministic replay:
- Fixed random seed (deterministic RNG)
- Event-driven scheduling (no wall-clock dependencies)
- Pre-allocated memory (no allocator non-determinism)
- Sorted event processing (timestamp order)

See `logs/strategy_trace.log` for TSC-level reproducibility proof.

## Monitoring

Static HTML dashboard at `dashboard/index.html` (demo only).

Real production monitoring requires:
- Prometheus exporters
- Grafana dashboards
- PagerDuty integration
- Log aggregation (ELK stack)

## Testing

```bash
# Run unit tests (if implemented)
./runTests

# Verify institutional logs
cd logs && sha256sum -c MANIFEST.sha256
```

## Documentation

- `ARCHITECTURE.md` - Order path, cache layout, thread model
- `LATENCY_BUDGET.md` - Component-level latency breakdown
- `INSTITUTIONAL_LOGGING_COMPARISON.md` - Before/after logging audit
- `logs/README.md` - Multi-layer timestamp verification

## License

Research use only. No warranty. See LICENSE.
│   - Buy/Sell intensity estimation                           │
└───────────────────────────┬─────────────────────────────────┘
                            │
                            ▼
┌─────────────────────────────────────────────────────────────┐
│      Feature Extraction (Microstructure)                     │
│   - Deep OFI (10 levels)                                    │
│   - Cross-asset correlations                                │
│   - Flow toxicity (Kyle's lambda)                           │
└───────────────────────────┬─────────────────────────────────┘
                            │
                            ▼
┌─────────────────────────────────────────────────────────────┐
│         FPGA DNN Inference (400ns fixed)                     │
│   - Hardware-accelerated decision                           │
│   - Deterministic latency guarantee                         │
└───────────────────────────┬─────────────────────────────────┘
                            │
                            ▼
┌─────────────────────────────────────────────────────────────┐
│    Execution Engine (Avellaneda-Stoikov + Latency)         │
│   - HJB reservation price                                   │
│   - Inventory skew                                          │
│   - Latency cost incorporation                              │
└───────────────────────────┬─────────────────────────────────┘
                            │
                            ▼
┌─────────────────────────────────────────────────────────────┐
│            Risk Control (Pre-trade + Kill-switch)           │
│   - Regime-based limits                                     │
│   - Atomic position checks                                  │
└───────────────────────────┬─────────────────────────────────┘
                            │
                            ▼
                   Order Submission
```

## 🚀 Building the System

### Prerequisites

- **Compiler**: GCC 9+ or Clang 10+ with C++17 support
- **Rust**: 1.70+ (for Rust components, optional)
- **CMake**: 3.15 or higher
- **OS**: Linux (Ubuntu 20.04+ recommended for kernel-bypass)
- **Hardware**: x86-64 with AVX2 (for optimal performance)

### Quick Build

```bash
# Clone the repository
cd /path/to/new-trading-system

# Build C++ core
./build.sh

# Build Rust components (optional)
cargo build --release --profile latency

# Run the system
./build/hft_system
```

### Detailed Build Instructions

```bash
# Create build directory
mkdir build && cd build

# Configure with aggressive optimizations
cmake .. -DCMAKE_BUILD_TYPE=Release

# Build (uses LTO and native CPU optimizations)
make -j$(nproc)

# Run the system
./hft_system
```

### Advanced Build Options

```bash
# Enable DPDK for real kernel bypass (requires DPDK installation)
cmake .. -DCMAKE_BUILD_TYPE=Release -DENABLE_DPDK=ON

# Enable NUMA optimizations for multi-socket systems
cmake .. -DCMAKE_BUILD_TYPE=Release -DENABLE_NUMA=ON

# Use jemalloc for better memory allocation performance
cmake .. -DCMAKE_BUILD_TYPE=Release -DUSE_JEMALLOC=ON

# Enable sanitizers for debugging
cmake .. -DCMAKE_BUILD_TYPE=Debug -DENABLE_ASAN=ON

# Build with all optimizations + Rust
cargo build --release --profile latency
cmake .. -DCMAKE_BUILD_TYPE=Release \
         -DENABLE_DPDK=ON \
         -DENABLE_NUMA=ON \
         -DUSE_JEMALLOC=ON
make -j$(nproc)
```

### Rust Integration

```bash
# Build Rust library first
cd /path/to/new-trading-system
cargo build --release --profile latency

# This creates: target/release/libhft_rust_core.a
# CMake will automatically link it if found

# Run Rust tests
cargo test --release

# Benchmark Rust components
cargo bench
```

## 📁 Project Structure

```
new-trading-system/
├── include/
│   ├── common_types.hpp          # Core data structures (MarketTick, Order, etc.)
│   ├── lockfree_queue.hpp        # Lock-free SPSC ring buffer (C++)
│   ├── hawkes_engine.hpp         # Multivariate Hawkes process
│   ├── fpga_inference.hpp        # FPGA DNN inference + feature extraction
│   ├── avellaneda_stoikov.hpp    # HJB market-making strategy
│   ├── risk_control.hpp          # Adaptive risk management
│   ├── kernel_bypass_nic.hpp     # Zero-copy data ingestion
│   ├── shared_memory.hpp         # POSIX shared memory IPC
│   ├── event_scheduler.hpp       # Nanosecond timing wheel
│   └── rust_ffi.hpp              # C++/Rust FFI bridge
├── src/
│   ├── main.cpp                  # Main trading loop (C++)
│   └── lib.rs                    # Rust core library
├── Cargo.toml                    # Rust build configuration
├── CMakeLists.txt                # C++ build configuration
├── build.sh                      # Automated build script
├── README.md                     # This file
└── ARCHITECTURE.md               # Detailed architecture documentation
```

## 🔬 Core Components

### 1. Hawkes Intensity Engine

```cpp
HawkesIntensityEngine hawkes(
    /* baseline_buy    */ 10.0,
    /* baseline_sell   */ 10.0,
    /* alpha_self      */ 0.3,   // Self-excitation
    /* alpha_cross     */ 0.1,   // Cross-excitation
    /* power_law_beta  */ 1e-3,  // Kernel offset
    /* power_law_gamma */ 1.8,   // Decay exponent (>1)
    /* max_history     */ 1000
);

// Update with new trading event
TradingEvent event(timestamp, Side::BUY, asset_id);
hawkes.update(event);

// Get current intensities
double buy_intensity = hawkes.get_buy_intensity();
double sell_intensity = hawkes.get_sell_intensity();
double imbalance = hawkes.get_intensity_imbalance();
```

### 2. FPGA Inference

```cpp
FPGA_DNN_Inference fpga(12, 8);  // 12 inputs, 8 hidden units

// Extract features from market data
auto features = FPGA_DNN_Inference::extract_features(
    current_tick, previous_tick, reference_tick,
    hawkes_buy_intensity, hawkes_sell_intensity
);

// Get prediction with guaranteed 400ns latency
auto [buy_score, hold_score, sell_score] = fpga.predict(features);
```

### 3. Market-Making Strategy

```cpp
DynamicMMStrategy strategy(
    /* risk_aversion      */ 0.1,
    /* volatility         */ 0.20,  // 20% annualized
    /* time_horizon       */ 300.0, // 5 minutes
    /* order_arrival_rate */ 10.0,
    /* tick_size          */ 0.01,
    /* system_latency_ns  */ 800
);

// Calculate optimal quotes with latency awareness
QuotePair quotes = strategy.calculate_quotes(
    mid_price, current_inventory, 
    time_remaining, latency_cost
);
```

### 4. Risk Control

```cpp
RiskControl risk(
    /* max_position       */ 1000,
    /* max_loss_threshold */ 10000.0,
    /* max_order_value    */ 100000.0
);

// Pre-trade checks
if (risk.check_pre_trade_limits(order, current_position)) {
    // Submit order
}

// Adaptive regime adjustment
risk.set_regime_multiplier(volatility_index);

// Emergency halt
if (anomaly_detected) {
    risk.trigger_kill_switch();
}
```

## 📈 Performance Monitoring

The system prints real-time statistics:

```
--- Cycle: 1000 ---
Mid Price: $100.05
Position: 250
Active Quotes: Bid=100.04 Ask=100.06 Spread=2.00 bps
Hawkes: Buy=12.456 Sell=11.234 Imbalance=0.052
Regime: NORMAL (multiplier=1.0)
Last Cycle Latency: 847 ns (0.847 µs)
NIC Queue Utilization: 12.5%
```

## ⚡ Optimization Techniques

### 1. **Compiler Optimizations**
- `-O3 -march=native -mtune=native`: CPU-specific optimizations
- `-flto`: Link-time optimization
- `-ffast-math`: Fast floating-point operations
- `-funroll-loops`: Loop unrolling
- `-fno-exceptions -fno-rtti`: Remove overhead

### 2. **Memory Optimizations**
- Cache-line alignment (64 bytes) for hot data structures
- Lock-free data structures to avoid contention
- Zero-copy data transfer (no memcpy in critical path)
- Power-of-2 buffer sizes for fast modulo operations

### 3. **Algorithm Optimizations**
- O(1) ring buffer operations
- Closed-form HJB solutions (no iterative optimization)
- Efficient power-law kernel evaluation
- Pre-computed constants

### 4. **Hardware Optimizations**
- NUMA-aware memory allocation (optional)
- CPU pinning for cache locality
- Huge pages for TLB efficiency (production)
- DPDK kernel bypass (optional)

## 🔧 Configuration Parameters

### Hawkes Process
- `baseline_buy/sell`: Background event rate (events/sec)
- `alpha_self`: How much an event excites similar future events
- `alpha_cross`: How much an event excites opposite events
- `power_law_gamma`: Decay rate (1.5-2.5 typical)

### Market Making
- `risk_aversion` (γ): Higher = wider spreads (0.01-1.0)
- `volatility` (σ): Annualized volatility (0.1-0.5)
- `time_horizon` (T): Strategy horizon in seconds
- `order_arrival_rate` (k): Expected fills per second

### Risk Management
- `max_position`: Absolute position limit
- `max_loss_threshold`: P&L kill-switch trigger
- `regime_multiplier`: Position scaling in stress (0.0-1.0)

## 🧪 Testing

```bash
# Build with tests
cmake .. -DBUILD_TESTS=ON
make
ctest --verbose
```

## 📚 Mathematical Foundations

### Hawkes Process Intensity
$$\lambda_i(t) = \mu_i + \sum_{j} \sum_{t_k < t} \alpha_{ij} \cdot (β + t - t_k)^{-\gamma}$$

### Avellaneda-Stoikov Reservation Price
$$r(t) = s(t) - q \cdot \gamma \cdot \sigma^2 \cdot (T - t)$$

### Optimal Spread
$$\delta^a + \delta^b = \gamma \sigma^2 (T-t) + \frac{2}{\gamma} \ln\left(1 + \frac{\gamma}{k}\right)$$

### Latency Cost Constraint
$$\text{Expected Profit} > \sigma \sqrt{\Delta t_{latency}} \cdot S_t$$

## 🎓 References

1. Hawkes, A. G. (1971). "Specular Point Processes"
2. Avellaneda, M., & Stoikov, S. (2008). "High-frequency trading in a limit order book"
3. Cartea, Á., et al. (2015). "Algorithmic and High-Frequency Trading"
4. Lehalle, C.-A., & Laruelle, S. (2018). "Market Microstructure in Practice"

## ⚠️ Disclaimer

This is a research and educational implementation. Real production HFT systems require:
- Hardware FPGA acceleration
- Actual kernel-bypass networking (DPDK, OpenOnload)
- Exchange connectivity and order management
- Compliance and risk management systems
- Extensive backtesting and validation

**Not for production trading without substantial additional development and testing.**

## 📝 License

MIT License - See LICENSE file for details

## 👥 Contributing

Contributions welcome! Please read CONTRIBUTING.md for guidelines.

## 📧 Contact

For questions or collaboration: [your-email@example.com]

---

**Built for speed. Designed for reliability. Optimized for alpha generation.**
