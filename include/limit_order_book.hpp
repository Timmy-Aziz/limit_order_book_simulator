#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <queue>
#include <thread>
#include <unordered_map>
#include <vector>
#include <functional>
#include <map>
#include <shared_mutex>

namespace lob {

// Order side enumeration
enum class Side : uint8_t {
    BUY = 0,
    SELL = 1
};

// Order status enumeration
enum class OrderStatus : uint8_t {
    NEW = 0,
    PARTIALLY_FILLED = 1,
    FILLED = 2,
    CANCELLED = 3,
    REJECTED = 4
};

// Order type enumeration
enum class OrderType : uint8_t {
    LIMIT = 0,
    MARKET = 1,
    STOP = 2
};

// Core Order structure optimized for performance
struct Order {
    uint64_t order_id;
    uint32_t symbol_id;
    Side side;
    OrderType order_type;
    uint64_t quantity;
    uint64_t price;           // For limit orders, in ticks
    uint64_t stop_price;      // For stop orders
    uint64_t timestamp;       // Microsecond timestamp
    OrderStatus status;
    uint64_t filled_quantity;
    
    // Default constructor
    Order() noexcept : order_id(0), symbol_id(0), side(Side::BUY), 
                      order_type(OrderType::LIMIT), quantity(0), 
                      price(0), stop_price(0), timestamp(0), 
                      status(OrderStatus::NEW), filled_quantity(0) {}
    
    // Parameterized constructor
    Order(uint64_t id, uint32_t symbol, Side s, OrderType type, 
          uint64_t qty, uint64_t px, uint64_t stop_px = 0) noexcept
        : order_id(id), symbol_id(symbol), side(s), order_type(type),
          quantity(qty), price(px), stop_price(stop_px), 
          timestamp(std::chrono::duration_cast<std::chrono::microseconds>(
              std::chrono::high_resolution_clock::now().time_since_epoch()).count()),
          status(OrderStatus::NEW), filled_quantity(0) {}
    
    // Check if order is fully filled
    bool is_filled() const noexcept { return filled_quantity == quantity; }
    
    // Get remaining quantity
    uint64_t remaining_quantity() const noexcept { return quantity - filled_quantity; }
};

// Trade execution result
struct Trade {
    uint64_t trade_id;
    uint64_t buy_order_id;
    uint64_t sell_order_id;
    uint32_t symbol_id;
    uint64_t quantity;
    uint64_t price;
    uint64_t timestamp;
    
    Trade(uint64_t tid, uint64_t buy_id, uint64_t sell_id, 
          uint32_t symbol, uint64_t qty, uint64_t px) noexcept
        : trade_id(tid), buy_order_id(buy_id), sell_order_id(sell_id),
          symbol_id(symbol), quantity(qty), price(px),
          timestamp(std::chrono::duration_cast<std::chrono::microseconds>(
              std::chrono::high_resolution_clock::now().time_since_epoch()).count()) {}
};

// Price level containing orders at the same price
class PriceLevel {
private:
    std::vector<std::shared_ptr<Order>> orders_;
    std::atomic<uint64_t> total_quantity_{0};
    mutable std::mutex level_mutex_;
    
public:
    PriceLevel() = default;
    
    // Add order to this price level
    void add_order(std::shared_ptr<Order> order);
    
    // Remove order from this price level
    void remove_order(uint64_t order_id);
    
    // Get best order (FIFO within price level)
    std::shared_ptr<Order> get_best_order();
    
    // Get total quantity at this price level
    uint64_t get_total_quantity() const noexcept { return total_quantity_.load(); }
    
    // Get number of orders at this price level
    size_t get_order_count() const;
    
    // Check if price level is empty
    bool is_empty() const;
};

// Market data snapshot
struct MarketDataSnapshot {
    uint32_t symbol_id;
    uint64_t timestamp;
    uint64_t best_bid_price;
    uint64_t best_bid_quantity;
    uint64_t best_ask_price;
    uint64_t best_ask_quantity;
    uint64_t last_trade_price;
    uint64_t last_trade_quantity;
    uint64_t volume;
    
    MarketDataSnapshot(uint32_t symbol) noexcept
        : symbol_id(symbol), timestamp(0), best_bid_price(0),
          best_bid_quantity(0), best_ask_price(0), best_ask_quantity(0),
          last_trade_price(0), last_trade_quantity(0), volume(0) {}
};

// Order book for a single symbol
class OrderBook {
private:
    uint32_t symbol_id_;
    
    // Bid and ask price levels (using ordered maps for efficient price level management)
    std::map<uint64_t, std::unique_ptr<PriceLevel>> bids_;
    std::map<uint64_t, std::unique_ptr<PriceLevel>> asks_;
    
    // Thread safety
    mutable std::shared_mutex book_mutex_;
    
    // Order tracking
    std::unordered_map<uint64_t, std::shared_ptr<Order>> orders_;
    mutable std::shared_mutex orders_mutex_;
    
    // Trade generation
    std::atomic<uint64_t> next_trade_id_{1};
    
    // Statistics
    std::atomic<uint64_t> total_volume_{0};
    std::atomic<uint64_t> trade_count_{0};
    
    // Market data callbacks
    std::vector<std::function<void(const MarketDataSnapshot&)>> market_data_callbacks_;
    std::vector<std::function<void(const Trade&)>> trade_callbacks_;
    mutable std::mutex callbacks_mutex_;
    
    // Internal helper methods
    void process_limit_order(std::shared_ptr<Order> order);
    void process_market_order(std::shared_ptr<Order> order);
    bool try_match_order(std::shared_ptr<Order> order, std::map<uint64_t, std::unique_ptr<PriceLevel>>& opposite_side);
    void execute_trade(std::shared_ptr<Order> buy_order, std::shared_ptr<Order> sell_order, uint64_t quantity);
    void add_to_book(std::shared_ptr<Order> order);
    void notify_market_data();
    void notify_trade(const Trade& trade);
    
public:
    explicit OrderBook(uint32_t symbol_id) : symbol_id_(symbol_id) {}
    
    // Order management
    bool add_order(std::shared_ptr<Order> order);
    bool cancel_order(uint64_t order_id);
    bool modify_order(uint64_t order_id, uint64_t new_quantity, uint64_t new_price = 0);
    
    // Market data queries
    MarketDataSnapshot get_market_data() const;
    std::vector<std::pair<uint64_t, uint64_t>> get_bid_levels(uint32_t depth = 10) const;
    std::vector<std::pair<uint64_t, uint64_t>> get_ask_levels(uint32_t depth = 10) const;
    
    // Callback registration
    void register_market_data_callback(std::function<void(const MarketDataSnapshot&)> callback);
    void register_trade_callback(std::function<void(const Trade&)> callback);
    
    // Statistics
    uint64_t get_total_volume() const noexcept { return total_volume_.load(); }
    uint64_t get_trade_count() const noexcept { return trade_count_.load(); }
    uint32_t get_symbol_id() const noexcept { return symbol_id_; }
};

// High-performance order book simulator
class OrderBookSimulator {
private:
    std::unordered_map<uint32_t, std::unique_ptr<OrderBook>> order_books_;
    mutable std::shared_mutex books_mutex_;
    
    // Order ID generation
    std::atomic<uint64_t> next_order_id_{1};
    
    // Thread pool for concurrent processing
    std::vector<std::thread> worker_threads_;
    std::queue<std::function<void()>> task_queue_;
    std::mutex queue_mutex_;
    std::condition_variable queue_cv_;
    std::atomic<bool> shutdown_{false};
    
    // Performance metrics
    std::atomic<uint64_t> orders_processed_{0};
    std::atomic<uint64_t> total_latency_ns_{0};
    
    void worker_thread_function();
    
public:
    OrderBookSimulator(size_t num_threads = std::thread::hardware_concurrency());
    ~OrderBookSimulator();
    
    // Order operations
    uint64_t submit_order(uint32_t symbol_id, Side side, OrderType type, 
                         uint64_t quantity, uint64_t price, uint64_t stop_price = 0);
    bool cancel_order(uint64_t order_id);
    bool modify_order(uint64_t order_id, uint64_t new_quantity, uint64_t new_price = 0);
    
    // Market data
    MarketDataSnapshot get_market_data(uint32_t symbol_id) const;
    std::vector<std::pair<uint64_t, uint64_t>> get_bid_levels(uint32_t symbol_id, uint32_t depth = 10) const;
    std::vector<std::pair<uint64_t, uint64_t>> get_ask_levels(uint32_t symbol_id, uint32_t depth = 10) const;
    
    // Callback registration
    void register_market_data_callback(uint32_t symbol_id, std::function<void(const MarketDataSnapshot&)> callback);
    void register_trade_callback(uint32_t symbol_id, std::function<void(const Trade&)> callback);
    
    // Performance metrics
    struct PerformanceMetrics {
        uint64_t orders_processed;
        double average_latency_ns;
        double orders_per_second;
        uint64_t total_volume;
        uint64_t trade_count;
    };
    
    PerformanceMetrics get_performance_metrics() const;
    
    // Simulation control
    void start_simulation();
    void stop_simulation();
};

} // namespace lob
