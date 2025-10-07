#pragma once

#include <memory>
#include <mutex>
#include <unordered_map>
#include <vector>
#include <functional>
#include <map>
#include <iostream>
#include <chrono>

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
    uint64_t total_quantity_;
    mutable std::mutex level_mutex_;
    
public:
    PriceLevel() : total_quantity_(0) {}
    
    // Add order to this price level
    void add_order(std::shared_ptr<Order> order) {
        std::lock_guard<std::mutex> lock(level_mutex_);
        orders_.push_back(order);
        total_quantity_ += order->quantity;
    }
    
    // Remove order from this price level
    void remove_order(uint64_t order_id) {
        std::lock_guard<std::mutex> lock(level_mutex_);
        
        for (auto it = orders_.begin(); it != orders_.end(); ++it) {
            if ((*it)->order_id == order_id) {
                total_quantity_ -= (*it)->quantity;
                orders_.erase(it);
                break;
            }
        }
    }
    
    // Get best order (FIFO within price level)
    std::shared_ptr<Order> get_best_order() {
        std::lock_guard<std::mutex> lock(level_mutex_);
        
        // Find the oldest unfilled order (FIFO)
        for (auto& order : orders_) {
            if (order->status != OrderStatus::FILLED && 
                order->status != OrderStatus::CANCELLED) {
                return order;
            }
        }
        
        return nullptr;
    }
    
    // Get total quantity at this price level
    uint64_t get_total_quantity() const {
        std::lock_guard<std::mutex> lock(level_mutex_);
        return total_quantity_;
    }
    
    // Get number of orders at this price level
    size_t get_order_count() const {
        std::lock_guard<std::mutex> lock(level_mutex_);
        return orders_.size();
    }
    
    // Check if price level is empty
    bool is_empty() const {
        std::lock_guard<std::mutex> lock(level_mutex_);
        return total_quantity_ == 0;
    }
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

// Simple Order book for a single symbol
class SimpleOrderBook {
private:
    uint32_t symbol_id_;
    
    // Bid and ask price levels
    std::map<uint64_t, std::unique_ptr<PriceLevel>> bids_;
    std::map<uint64_t, std::unique_ptr<PriceLevel>> asks_;
    
    // Thread safety
    mutable std::mutex book_mutex_;
    
    // Order tracking
    std::unordered_map<uint64_t, std::shared_ptr<Order>> orders_;
    mutable std::mutex orders_mutex_;
    
    // Trade generation
    uint64_t next_trade_id_;
    
    // Statistics
    uint64_t total_volume_;
    uint64_t trade_count_;
    
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
    explicit SimpleOrderBook(uint32_t symbol_id) : symbol_id_(symbol_id), next_trade_id_(1), 
                                                 total_volume_(0), trade_count_(0) {}
    
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
    uint64_t get_total_volume() const { return total_volume_; }
    uint64_t get_trade_count() const { return trade_count_; }
    uint32_t get_symbol_id() const { return symbol_id_; }
};

} // namespace lob
