Low-Latency Trading Systems Framework

## Overview

This is my approach for a high-performance, production-grade C++ framework for building ultra-low-latency trading systems, used for quantitative finance and systems programming. My focus was on highly efficient and especially clean code.
I saw this project as a challenge, trying to push as hard as I can, seeing what I can get done if I give it my all.

## Key Features

- 🚀 Sub-microsecond latency processing (blazingly fast IMO)
- 🔬 Statistical arbitrage strategy
- 🧊 Lock-free concurrent data structures
- 📊 Market data simulation (though, only a simulation)
- 🔍 Detailed performance benchmarking (run them yourself =)!)

## Performance Highlights

### Benchmark Results (M2 Max, macOS Sequoia)

#### Order Book Operations
- **Add Order**: 
  - Mean Latency: 347.20 ns
  - 99.9th Percentile: 2,458 ns
- **Best Bid/Ask Lookup**: 
  - Mean Latency: 14.32 ns
- **Cancel Order**: 
  - Mean Latency: 327.69 ns

#### Lock-Free Queue
- **Push/Pop Operations**: 
  - Mean Latency: ~14.5 ns
  - Consistently under 42 ns at 99th percentile

## Technical Architecture

### Components
- **Market Data Handler**: Real-time market data processing
- **Strategy Engine**: Advanced signal generation
- **Execution Engine**: Rapid order execution simulation
- **Utility Modules**: 
  - Lock-free queues
  - Memory pooling
  - Precise timing mechanisms

### Key Technologies
- Modern C++20
- Zero-overhead abstractions
- Cache-line optimized data structures
- Statistical arbitrage modeling

## Getting Started

### Prerequisites
- CMake (3.20+)
- Modern C++ Compiler (GCC 10+/Clang 10+/MSVC 19.2+)

### Build Instructions
```bash
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make
```

### Run Benchmarks
```bash
# From build directory
./benchmark/latency_benchmark
```

### Run Market Simulator
```bash
# From build directory
./examples/simulator
```

## Performance Philosophy

Each line of code is crafted to minimize computational overhead and maximize throughput. 

## Potential Applications

- Algorithmic Trading Platforms
- Market Making Systems
- Quantitative Research Infrastructure
- High-Performance Financial Simulations
- Anything where you think that speed is key

## Contributions

Feel free to give me feedback. Furthermore, if you find potential for optimization, I appreciate any feedback!

**Open Source with Attribution**

You are free to:
- Use the code in personal or commercial projects
- Modify and adapt the code
- Share and distribute the code

**Conditions**:
- You must give appropriate credit to the original author (link to this github is enough.)
- You may not present this work as entirely your own
- If you modify the code, clearly indicate your changes

This project is shared in the spirit of open collaboration and learning. Respect for the original work is the only requirement.


