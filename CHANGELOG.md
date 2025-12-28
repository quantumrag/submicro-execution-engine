# Changelog

All notable changes to the **Sub-Microsecond Execution Engine** will be documented in this file.

## [v2.2.0] - 2025-12-28

### Added
- **Vectorized Multi-Kernel Hawkes Engine**: Implemented a SIMD-accelerated engine supporting 4 simultaneous kernels.
  - *Why it helps:* Markets react at different speeds; this captures both microsecond liquidity bursts and millisecond price trends simultaneously, significantly improving signal accuracy without increasing latency.
- **Engagement Framework**: Added `docs/ENGAGEMENTS.md` and GitHub issue templates.
  - *Why it helps:* Provides a structured pathway for institutional partners and research labs to request custom hardware (FPGA/NIC) integrations or collaborative research without public disclosure.

### Improved
- **Build System**: Updated `scripts/build_all.sh` with automatic macOS Homebrew path detection.
  - *Why it helps:* Streamlines developer onboarding and ensures seamless local testing on modern Apple Silicon (ARM64) research environments.
- **Documentation**: Simplified README and organized commercial support sections.

## [v2.1.0] - 2025-12-16

### Enhancement
- **890ns End-to-End Latency**: Optimized decision pipeline down to 890ns median.
- **Institutional Logging**: Implemented 7-layer professional logging (NIC, TSC, PTP, etc.).
- **Deterministic Replay**: Bit-identical verification with SHA-256 manifests.

## [v2.0.0] - 2025-12-15

### Added
- **Custom NIC Drivers**: Support for Intel X710 and Mellanox ConnectX hardware mapping.
- **Solarflare ef_vi**: Integration for X2522/X2542 series.
- **Kernel Bypass**: Lock-free DPDK-style interface.

## [v1.5.0] - 2025-12-10

### Added
- **AVX-512 SIMD**: Vectorized OFI computation (40ns).
- **Vectorized ML**: FPGA-simulated inference pipeline (400ns fixed latency).

## [v1.0.0] - 2025-12-01

### Added
- Initial High-Frequency Trading skeleton.
- Hawkes Process Engine (Single-Kernel).
- Avellaneda-Stoikov Market Making strategy.
- Lock-Free SPSC Queues and Atomic Risk Controls.
