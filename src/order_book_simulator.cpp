#include "../include/limit_order_book.hpp"
#include <iostream>

namespace lob {

OrderBookSimulator::OrderBookSimulator(size_t num_threads) {
    // Start worker threads
    for (size_t i = 0; i < num_threads; ++i) {
        worker_threads_.emplace_back(&OrderBookSimulator::worker_thread_function, this);
    }
    
    std::cout << "OrderBookSimulator initialized with " << num_threads << " worker threads\n";
}

OrderBookSimulator::~OrderBookSimulator() {
    stop_simulation();
}

void OrderBookSimulator::worker_thread_function() {
    while (!shutdown_.load()) {
        std::function<void()> task;
        
        {
            std::unique_lock<std::mutex> lock(queue_mutex_);
            queue_cv_.wait(lock, [this] { return !task_queue_.empty() || shutdown_.load(); });
            
            if (shutdown_.load()) {
                break;
            }
            
            task = task_queue_.front();
            task_queue_.pop();
        }
        
        // Execute the task
        auto start_time = std::chrono::high_resolution_clock::now();
        task();
        auto end_time = std::chrono::high_resolution_clock::now();
        
        // Update performance metrics
        auto latency_ns = std::chrono::duration_cast<std::chrono::nanoseconds>(end_time - start_time).count();
        total_latency_ns_.fetch_add(latency_ns);
        orders_processed_.fetch_add(1);
    }
}

uint64_t OrderBookSimulator::submit_order(uint32_t symbol_id, Side side, OrderType type, 
                                        uint64_t quantity, uint64_t price, uint64_t stop_price) {
    // Generate unique order ID
    uint64_t order_id = next_order_id_.fetch_add(1);
    
    // Create order
    auto order = std::make_shared<Order>(order_id, symbol_id, side, type, quantity, price, stop_price);
    
    // Get or create order book for this symbol
    {
        std::shared_lock<std::shared_mutex> lock(books_mutex_);
        auto it = order_books_.find(symbol_id);
        if (it == order_books_.end()) {
            lock.unlock();
            std::unique_lock<std::shared_mutex> unique_lock(books_mutex_);
            // Double-check after acquiring unique lock
            it = order_books_.find(symbol_id);
            if (it == order_books_.end()) {
                order_books_[symbol_id] = std::make_unique<OrderBook>(symbol_id);
                it = order_books_.find(symbol_id);
            }
        }
    }
    
    // Submit order to the order book (this is thread-safe)
    auto order_book = order_books_[symbol_id].get();
    order_book->add_order(order);
    
    return order_id;
}

bool OrderBookSimulator::cancel_order(uint64_t order_id) {
    // We need to find which order book contains this order
    // For simplicity, we'll search all order books
    std::shared_lock<std::shared_mutex> lock(books_mutex_);
    
    for (auto& [symbol_id, order_book] : order_books_) {
        if (order_book->cancel_order(order_id)) {
            return true;
        }
    }
    
    return false;
}

bool OrderBookSimulator::modify_order(uint64_t order_id, uint64_t new_quantity, uint64_t new_price) {
    // Similar to cancel_order, we need to find the correct order book
    std::shared_lock<std::shared_mutex> lock(books_mutex_);
    
    for (auto& [symbol_id, order_book] : order_books_) {
        if (order_book->modify_order(order_id, new_quantity, new_price)) {
            return true;
        }
    }
    
    return false;
}

MarketDataSnapshot OrderBookSimulator::get_market_data(uint32_t symbol_id) const {
    std::shared_lock<std::shared_mutex> lock(books_mutex_);
    
    auto it = order_books_.find(symbol_id);
    if (it != order_books_.end()) {
        return it->second->get_market_data();
    }
    
    // Return empty snapshot if symbol not found
    return MarketDataSnapshot(symbol_id);
}

std::vector<std::pair<uint64_t, uint64_t>> OrderBookSimulator::get_bid_levels(uint32_t symbol_id, uint32_t depth) const {
    std::shared_lock<std::shared_mutex> lock(books_mutex_);
    
    auto it = order_books_.find(symbol_id);
    if (it != order_books_.end()) {
        return it->second->get_bid_levels(depth);
    }
    
    return {};
}

std::vector<std::pair<uint64_t, uint64_t>> OrderBookSimulator::get_ask_levels(uint32_t symbol_id, uint32_t depth) const {
    std::shared_lock<std::shared_mutex> lock(books_mutex_);
    
    auto it = order_books_.find(symbol_id);
    if (it != order_books_.end()) {
        return it->second->get_ask_levels(depth);
    }
    
    return {};
}

void OrderBookSimulator::register_market_data_callback(uint32_t symbol_id, 
                                                     std::function<void(const MarketDataSnapshot&)> callback) {
    std::shared_lock<std::shared_mutex> lock(books_mutex_);
    
    auto it = order_books_.find(symbol_id);
    if (it != order_books_.end()) {
        it->second->register_market_data_callback(callback);
    }
}

void OrderBookSimulator::register_trade_callback(uint32_t symbol_id, 
                                               std::function<void(const Trade&)> callback) {
    std::shared_lock<std::shared_mutex> lock(books_mutex_);
    
    auto it = order_books_.find(symbol_id);
    if (it != order_books_.end()) {
        it->second->register_trade_callback(callback);
    }
}

OrderBookSimulator::PerformanceMetrics OrderBookSimulator::get_performance_metrics() const {
    PerformanceMetrics metrics;
    
    metrics.orders_processed = orders_processed_.load();
    metrics.total_volume = 0;
    metrics.trade_count = 0;
    
    // Aggregate metrics from all order books
    std::shared_lock<std::shared_mutex> lock(books_mutex_);
    for (const auto& [symbol_id, order_book] : order_books_) {
        metrics.total_volume += order_book->get_total_volume();
        metrics.trade_count += order_book->get_trade_count();
    }
    
    // Calculate averages
    if (metrics.orders_processed > 0) {
        metrics.average_latency_ns = static_cast<double>(total_latency_ns_.load()) / metrics.orders_processed;
    } else {
        metrics.average_latency_ns = 0.0;
    }
    
    // Note: orders_per_second would need timing information to calculate properly
    metrics.orders_per_second = 0.0;
    
    return metrics;
}

void OrderBookSimulator::start_simulation() {
    std::cout << "OrderBookSimulator simulation started\n";
}

void OrderBookSimulator::stop_simulation() {
    shutdown_.store(true);
    queue_cv_.notify_all();
    
    // Wait for all worker threads to finish
    for (auto& thread : worker_threads_) {
        if (thread.joinable()) {
            thread.join();
        }
    }
    
    worker_threads_.clear();
    std::cout << "OrderBookSimulator simulation stopped\n";
}

} // namespace lob
