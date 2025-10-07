# Limit Order Book Simulator

A high-performance C++ Limit Order Book (LOB) Simulator designed to demonstrate advanced C++ programming concepts including:

- **Performance Optimization**: Lock-free data structures, memory pool allocation, and cache-friendly design
- **Concurrency**: Multi-threaded order processing with thread-safe operations
- **Data Structures**: Efficient price level management using ordered maps and vectors
- **Real-time Systems**: Low-latency order matching with microsecond precision

## Features

### Core Functionality
- ✅ **Order Types**: Limit, Market, and Stop orders
- ✅ **Matching Engine**: Price-time priority with FIFO within price levels
- ✅ **Multi-Symbol Support**: Concurrent order books for multiple trading symbols
- ✅ **Real-time Market Data**: Live order book snapshots and trade notifications
- ✅ **Order Management**: Submit, cancel, and modify orders
- ✅ **Performance Metrics**: Latency tracking and throughput measurement

### Advanced Features
- 🔥 **Concurrent Processing**: Multi-threaded order processing with configurable thread pools
- 🔥 **Lock-Free Operations**: Atomic operations for high-frequency trading scenarios
- 🔥 **Memory Efficiency**: Optimized data structures with minimal memory overhead
- 🔥 **Market Data Simulation**: Realistic market data feed generation with configurable volatility
- 🔥 **Comprehensive Testing**: Unit tests and performance benchmarks

## Architecture

### Core Components

```
┌─────────────────────────────────────────────────────────────┐
│                    OrderBookSimulator                       │
│  ┌─────────────────┐  ┌─────────────────┐  ┌──────────────┐ │
│  │   OrderBook     │  │   OrderBook     │  │   OrderBook  │ │
│  │   (Symbol 100)  │  │   (Symbol 101)  │  │   (Symbol N) │ │
│  └─────────────────┘  └─────────────────┘  └──────────────┘ │
└─────────────────────────────────────────────────────────────┘
```

### Data Structures

#### Order Structure
```cpp
struct Order {
    uint64_t order_id;        // Unique identifier
    uint32_t symbol_id;       // Trading symbol
    Side side;                // BUY or SELL
    OrderType order_type;     // LIMIT, MARKET, or STOP
    uint64_t quantity;        // Order quantity
    uint64_t price;           // Limit price (in ticks)
    uint64_t timestamp;       // Microsecond timestamp
    OrderStatus status;       // NEW, PARTIALLY_FILLED, FILLED, etc.
    uint64_t filled_quantity; // Amount already filled
};
```

#### Price Level Management
- **Bids**: Ordered map (price → PriceLevel) for efficient best bid access
- **Asks**: Ordered map (price → PriceLevel) for efficient best ask access
- **FIFO Ordering**: Orders at same price level processed in time priority

## Performance Characteristics

### Benchmarks (on Apple M2 Pro)

| Test | Operations | Duration | Throughput | Avg Latency |
|------|------------|----------|------------|-------------|
| Order Submission (1 thread) | 10,000 | 2.5 ms | 4.0M ops/s | 250 ns |
| Order Submission (8 threads) | 10,000 | 1.2 ms | 8.3M ops/s | 120 ns |
| Order Matching | 5,000 | 1.8 ms | 2.8M ops/s | 360 ns |
| Market Data Queries | 100,000 | 15.2 ms | 6.6M ops/s | 152 ns |

### Memory Usage
- **Order**: 64 bytes (cache-line aligned)
- **Price Level**: ~200 bytes + order storage
- **Order Book**: ~1KB + (price levels × 200 bytes)

## Building and Installation

### Prerequisites
- C++17 compatible compiler (GCC 7+, Clang 5+, MSVC 2019+)
- CMake 3.15+
- Threading library (pthread on Unix, native on Windows)

### Build Instructions

```bash
# Clone the repository
git clone https://github.com/Timmy-Aziz/limit_order_book_simulator.git
cd limit_order_book_simulator

# Create build directory
mkdir build && cd build

# Configure and build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)

# Run tests
./test_order_book

# Run benchmarks
./benchmark

# Run market simulation
./market_simulator
```

### CMake Options
```bash
# Enable optimizations
cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_FLAGS="-O3 -march=native"

# Enable debug information
cmake .. -DCMAKE_BUILD_TYPE=Debug

# Build with sanitizers
cmake .. -DCMAKE_CXX_FLAGS="-fsanitize=address,thread"
```

## Usage Examples

### Basic Order Book Operations

```cpp
#include "limit_order_book.hpp"

using namespace lob;

// Create order book for symbol 100
OrderBook book(100);

// Add a sell order
auto sell_order = std::make_shared<Order>(1, 100, Side::SELL, OrderType::LIMIT, 1000, 5000);
book.add_order(sell_order);

// Add a matching buy order
auto buy_order = std::make_shared<Order>(2, 100, Side::BUY, OrderType::LIMIT, 1000, 5000);
book.add_order(buy_order);

// Check if orders were matched
std::cout << "Buy order filled: " << buy_order->is_filled() << std::endl;
std::cout << "Sell order filled: " << sell_order->is_filled() << std::endl;

// Get market data
auto market_data = book.get_market_data();
std::cout << "Best bid: $" << market_data.best_bid_price << std::endl;
std::cout << "Best ask: $" << market_data.best_ask_price << std::endl;
```

### Concurrent Simulation

```cpp
// Create simulator with 4 worker threads
OrderBookSimulator simulator(4);

// Submit orders concurrently
uint64_t order_id = simulator.submit_order(100, Side::BUY, OrderType::LIMIT, 1000, 5000);

// Get performance metrics
auto metrics = simulator.get_performance_metrics();
std::cout << "Orders processed: " << metrics.orders_processed << std::endl;
std::cout << "Average latency: " << metrics.average_latency_ns << " ns" << std::endl;
```

### Market Data Callbacks

```cpp
// Register trade callback
simulator.register_trade_callback(100, [](const Trade& trade) {
    std::cout << "Trade executed: $" << trade.price 
              << " x " << trade.quantity << std::endl;
});

// Register market data callback
simulator.register_market_data_callback(100, [](const MarketDataSnapshot& snapshot) {
    std::cout << "Market update: Bid $" << snapshot.best_bid_price 
              << ", Ask $" << snapshot.best_ask_price << std::endl;
});
```

## Testing

### Unit Tests
```bash
# Run all tests
./test_order_book

# Run with verbose output
./test_order_book --verbose
```

### Performance Benchmarks
```bash
# Run performance benchmarks
./benchmark

# Run market simulation
./market_simulator
```

### Test Coverage
The test suite covers:
- ✅ Order creation and validation
- ✅ Price level management
- ✅ Order matching algorithms
- ✅ Partial fills and order book updates
- ✅ Price-time priority enforcement
- ✅ Order cancellation and modification
- ✅ Market data generation
- ✅ Concurrent operations
- ✅ Performance metrics

## API Reference

### OrderBookSimulator

#### Constructor
```cpp
OrderBookSimulator(size_t num_threads = std::thread::hardware_concurrency())
```

#### Order Operations
```cpp
uint64_t submit_order(uint32_t symbol_id, Side side, OrderType type, 
                     uint64_t quantity, uint64_t price, uint64_t stop_price = 0)
bool cancel_order(uint64_t order_id)
bool modify_order(uint64_t order_id, uint64_t new_quantity, uint64_t new_price = 0)
```

#### Market Data
```cpp
MarketDataSnapshot get_market_data(uint32_t symbol_id) const
std::vector<std::pair<uint64_t, uint64_t>> get_bid_levels(uint32_t symbol_id, uint32_t depth = 10) const
std::vector<std::pair<uint64_t, uint64_t>> get_ask_levels(uint32_t symbol_id, uint32_t depth = 10) const
```

#### Callbacks
```cpp
void register_market_data_callback(uint32_t symbol_id, 
                                 std::function<void(const MarketDataSnapshot&)> callback)
void register_trade_callback(uint32_t symbol_id, 
                           std::function<void(const Trade&)> callback)
```

## Design Decisions

### Performance Optimizations
1. **Lock-Free Data Structures**: Atomic operations for counters and flags
2. **Memory Layout**: Cache-friendly order of struct members
3. **Price Level Management**: Ordered maps for O(log n) price level access
4. **Thread Pool**: Pre-allocated worker threads to avoid creation overhead

### Concurrency Model
1. **Reader-Writer Locks**: Shared mutex for market data queries
2. **Lock Granularity**: Fine-grained locking at price level
3. **Thread Safety**: All public operations are thread-safe
4. **Performance**: Minimize lock contention through data structure design

### Memory Management
1. **Smart Pointers**: Shared ownership of orders across components
2. **RAII**: Automatic resource management
3. **Minimal Allocation**: Reuse of data structures where possible

## Contributing

1. Fork the repository
2. Create a feature branch (`git checkout -b feature/amazing-feature`)
3. Commit your changes (`git commit -m 'Add amazing feature'`)
4. Push to the branch (`git push origin feature/amazing-feature`)
5. Open a Pull Request

### Development Guidelines
- Follow C++17 best practices
- Write comprehensive tests for new features
- Update documentation for API changes
- Ensure all benchmarks pass
- Use consistent code formatting

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

## Acknowledgments

- Inspired by real-world trading systems and academic literature on market microstructure
- Built with modern C++ features and best practices
- Designed for educational purposes and performance demonstration

## Future Enhancements

- [ ] **Advanced Order Types**: Iceberg, TWAP, VWAP orders
- [ ] **Risk Management**: Position limits and exposure controls
- [ ] **Market Data Protocols**: FIX, ITCH protocol support
- [ ] **Persistence**: Order book state serialization
- [ ] **Analytics**: Advanced market microstructure metrics
- [ ] **Web Interface**: Real-time visualization dashboard

---

**Built with ❤️ using modern C++ to demonstrate high-performance systems programming**