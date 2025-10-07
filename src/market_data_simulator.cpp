#include "../include/limit_order_book.hpp"
#include <iostream>
#include <random>
#include <thread>
#include <chrono>
#include <vector>
#include <iomanip>

namespace lob {

class MarketDataSimulator {
private:
    OrderBookSimulator& simulator_;
    std::atomic<bool> running_{false};
    std::vector<std::thread> worker_threads_;
    
    // Market data generation parameters
    struct SymbolConfig {
        uint32_t symbol_id;
        uint64_t base_price;
        uint64_t price_range;
        uint64_t min_quantity;
        uint64_t max_quantity;
        double volatility;
        uint32_t orders_per_second;
    };
    
    std::vector<SymbolConfig> symbol_configs_;
    std::mt19937_64 rng_;
    
    // Statistics tracking
    std::atomic<uint64_t> total_orders_generated_{0};
    std::atomic<uint64_t> total_trades_executed_{0};
    std::atomic<uint64_t> total_volume_{0};
    
public:
    explicit MarketDataSimulator(OrderBookSimulator& simulator) 
        : simulator_(simulator), rng_(std::chrono::high_resolution_clock::now().time_since_epoch().count()) {
        
        // Configure symbols
        setup_symbols();
    }
    
    ~MarketDataSimulator() {
        stop_simulation();
    }
    
    void setup_symbols() {
        // Add some example symbols with different characteristics
        symbol_configs_ = {
            {100, 5000, 500, 100, 5000, 0.02, 100},    // AAPL-like: high volume, moderate volatility
            {101, 3000, 300, 50, 3000, 0.03, 50},      // TSLA-like: high volatility
            {102, 150, 50, 1000, 10000, 0.01, 200},    // High volume, low price
            {103, 25000, 1000, 10, 100, 0.015, 25},    // High price, low volume
        };
    }
    
    void start_simulation(size_t num_threads = 2) {
        if (running_.load()) {
            return;
        }
        
        running_.store(true);
        
        // Start worker threads for each symbol
        for (const auto& config : symbol_configs_) {
            worker_threads_.emplace_back(&MarketDataSimulator::simulate_symbol, this, config);
        }
        
        // Start statistics reporting thread
        worker_threads_.emplace_back(&MarketDataSimulator::report_statistics, this);
        
        std::cout << "Market data simulation started with " << symbol_configs_.size() 
                  << " symbols and " << num_threads << " threads per symbol\n";
    }
    
    void stop_simulation() {
        if (!running_.load()) {
            return;
        }
        
        running_.store(false);
        
        // Wait for all threads to finish
        for (auto& thread : worker_threads_) {
            if (thread.joinable()) {
                thread.join();
            }
        }
        
        worker_threads_.clear();
        std::cout << "Market data simulation stopped\n";
    }
    
private:
    void simulate_symbol(const SymbolConfig& config) {
        std::uniform_real_distribution<double> price_change_dist(-config.volatility, config.volatility);
        std::uniform_int_distribution<uint64_t> quantity_dist(config.min_quantity, config.max_quantity);
        std::uniform_int_distribution<int> side_dist(0, 1);
        std::uniform_real_distribution<double> order_type_dist(0.0, 1.0);
        
        uint64_t current_price = config.base_price;
        auto last_order_time = std::chrono::high_resolution_clock::now();
        
        while (running_.load()) {
            auto now = std::chrono::high_resolution_clock::now();
            auto time_since_last_order = std::chrono::duration_cast<std::chrono::milliseconds>(
                now - last_order_time).count();
            
            // Calculate target interval between orders
            double target_interval_ms = 1000.0 / config.orders_per_second;
            
            if (time_since_last_order >= target_interval_ms) {
                // Generate order
                Side side = static_cast<Side>(side_dist(rng_));
                OrderType order_type = (order_type_dist(rng_) < 0.9) ? OrderType::LIMIT : OrderType::MARKET;
                
                uint64_t quantity = quantity_dist(rng_);
                uint64_t price = 0;
                
                if (order_type == OrderType::LIMIT) {
                    // Apply price volatility
                    double price_change = price_change_dist(rng_);
                    double price_multiplier = 1.0 + price_change;
                    
                    if (side == Side::BUY) {
                        // Buy orders slightly below current price
                        price = static_cast<uint64_t>(current_price * price_multiplier * 0.999);
                    } else {
                        // Sell orders slightly above current price
                        price = static_cast<uint64_t>(current_price * price_multiplier * 1.001);
                    }
                    
                    // Ensure price stays within reasonable bounds
                    uint64_t min_price = config.base_price - config.price_range;
                    uint64_t max_price = config.base_price + config.price_range;
                    price = std::max(min_price, std::min(max_price, price));
                }
                
                // Submit order
                uint64_t order_id = simulator_.submit_order(config.symbol_id, side, order_type, 
                                                          quantity, price);
                
                total_orders_generated_.fetch_add(1);
                last_order_time = now;
                
                // Update current price based on order flow
                if (order_type == OrderType::LIMIT) {
                    current_price = price;
                }
            } else {
                // Sleep for a short time to avoid busy waiting
                std::this_thread::sleep_for(std::chrono::microseconds(100));
            }
        }
    }
    
    void report_statistics() {
        auto start_time = std::chrono::high_resolution_clock::now();
        
        while (running_.load()) {
            std::this_thread::sleep_for(std::chrono::seconds(10));
            
            auto now = std::chrono::high_resolution_clock::now();
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - start_time).count();
            
            if (elapsed > 0) {
                auto metrics = simulator_.get_performance_metrics();
                
                std::cout << "\n=== Market Data Simulation Statistics (after " << elapsed << "s) ===\n";
                std::cout << "Orders generated: " << total_orders_generated_.load() << "\n";
                std::cout << "Orders processed: " << metrics.orders_processed << "\n";
                std::cout << "Trades executed: " << metrics.trade_count << "\n";
                std::cout << "Total volume: " << metrics.total_volume << "\n";
                std::cout << "Average latency: " << std::fixed << std::setprecision(2) 
                          << metrics.average_latency_ns / 1000.0 << " μs\n";
                std::cout << "Orders/second: " << total_orders_generated_.load() / elapsed << "\n";
                std::cout << "================================================\n\n";
            }
        }
    }
};

} // namespace lob

// Example usage function
void run_market_simulation_example() {
    lob::OrderBookSimulator simulator(4);
    
    // Register callbacks for trade notifications
    for (uint32_t symbol_id = 100; symbol_id <= 103; ++symbol_id) {
        simulator.register_trade_callback(symbol_id, [symbol_id](const lob::Trade& trade) {
            std::cout << "TRADE: Symbol " << symbol_id 
                      << ", Price: " << trade.price 
                      << ", Quantity: " << trade.quantity
                      << ", Trade ID: " << trade.trade_id << "\n";
        });
    }
    
    lob::MarketDataSimulator market_sim(simulator);
    
    std::cout << "Starting market data simulation...\n";
    std::cout << "Press Ctrl+C to stop\n\n";
    
    market_sim.start_simulation(2);
    
    // Run for 60 seconds
    std::this_thread::sleep_for(std::chrono::seconds(60));
    
    market_sim.stop_simulation();
    
    // Print final statistics
    auto final_metrics = simulator.get_performance_metrics();
    std::cout << "\n=== Final Performance Metrics ===\n";
    std::cout << "Total orders processed: " << final_metrics.orders_processed << "\n";
    std::cout << "Total trades: " << final_metrics.trade_count << "\n";
    std::cout << "Total volume: " << final_metrics.total_volume << "\n";
    std::cout << "Average latency: " << std::fixed << std::setprecision(2) 
              << final_metrics.average_latency_ns / 1000.0 << " μs\n";
}

int main() {
    run_market_simulation_example();
    return 0;
}
