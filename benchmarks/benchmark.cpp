#include "../include/limit_order_book.hpp"
#include <iostream>
#include <chrono>
#include <random>
#include <thread>
#include <vector>
#include <iomanip>

using namespace lob;

class BenchmarkSuite {
private:
    std::mt19937_64 rng_;
    std::uniform_int_distribution<uint64_t> price_dist_;
    std::uniform_int_distribution<uint64_t> quantity_dist_;
    std::uniform_int_distribution<int> side_dist_;
    
public:
    BenchmarkSuite() 
        : rng_(std::chrono::high_resolution_clock::now().time_since_epoch().count()),
          price_dist_(4800, 5200),
          quantity_dist_(100, 10000),
          side_dist_(0, 1) {}
    
    struct BenchmarkResult {
        std::string test_name;
        size_t num_operations;
        double duration_ms;
        double operations_per_second;
        double average_latency_ns;
    };
    
    BenchmarkResult benchmark_order_submission(size_t num_orders, size_t num_threads = 1) {
        OrderBookSimulator simulator(num_threads);
        constexpr uint32_t symbol_id = 100;
        
        auto start_time = std::chrono::high_resolution_clock::now();
        
        std::vector<std::thread> threads;
        size_t orders_per_thread = num_orders / num_threads;
        
        for (size_t t = 0; t < num_threads; ++t) {
            threads.emplace_back([&, t, orders_per_thread]() {
                for (size_t i = 0; i < orders_per_thread; ++i) {
                    Side side = static_cast<Side>(side_dist_(rng_));
                    uint64_t price = price_dist_(rng_);
                    uint64_t quantity = quantity_dist_(rng_);
                    
                    simulator.submit_order(symbol_id, side, OrderType::LIMIT, 
                                         quantity, price);
                }
            });
        }
        
        for (auto& thread : threads) {
            thread.join();
        }
        
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
        
        auto metrics = simulator.get_performance_metrics();
        
        BenchmarkResult result;
        result.test_name = "Order Submission (" + std::to_string(num_threads) + " threads)";
        result.num_operations = num_orders;
        result.duration_ms = duration.count() / 1000.0;
        result.operations_per_second = (num_orders * 1000.0) / result.duration_ms;
        result.average_latency_ns = metrics.average_latency_ns;
        
        return result;
    }
    
    BenchmarkResult benchmark_matching_performance(size_t num_orders) {
        OrderBook book(100);
        
        auto start_time = std::chrono::high_resolution_clock::now();
        
        // Create a balanced set of buy and sell orders
        for (size_t i = 0; i < num_orders / 2; ++i) {
            uint64_t price = 5000 + (i % 100);
            uint64_t quantity = 1000;
            
            // Add sell order
            auto sell_order = std::make_shared<Order>(i * 2, 100, Side::SELL, 
                                                   OrderType::LIMIT, quantity, price);
            book.add_order(sell_order);
            
            // Add matching buy order
            auto buy_order = std::make_shared<Order>(i * 2 + 1, 100, Side::BUY, 
                                                  OrderType::LIMIT, quantity, price);
            book.add_order(buy_order);
        }
        
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
        
        BenchmarkResult result;
        result.test_name = "Order Matching";
        result.num_operations = num_orders;
        result.duration_ms = duration.count() / 1000.0;
        result.operations_per_second = (num_orders * 1000.0) / result.duration_ms;
        result.average_latency_ns = (result.duration_ms * 1000000.0) / num_orders;
        
        return result;
    }
    
    BenchmarkResult benchmark_market_data_queries(size_t num_queries) {
        OrderBook book(100);
        
        // Pre-populate order book
        for (int i = 0; i < 100; ++i) {
            auto buy_order = std::make_shared<Order>(i, 100, Side::BUY, 
                                                   OrderType::LIMIT, 1000, 4900 + i);
            auto sell_order = std::make_shared<Order>(i + 100, 100, Side::SELL, 
                                                    OrderType::LIMIT, 1000, 5000 + i);
            book.add_order(buy_order);
            book.add_order(sell_order);
        }
        
        auto start_time = std::chrono::high_resolution_clock::now();
        
        for (size_t i = 0; i < num_queries; ++i) {
            auto market_data = book.get_market_data();
            auto bid_levels = book.get_bid_levels(10);
            auto ask_levels = book.get_ask_levels(10);
        }
        
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
        
        BenchmarkResult result;
        result.test_name = "Market Data Queries";
        result.num_operations = num_queries;
        result.duration_ms = duration.count() / 1000.0;
        result.operations_per_second = (num_queries * 1000.0) / result.duration_ms;
        result.average_latency_ns = (result.duration_ms * 1000000.0) / num_queries;
        
        return result;
    }
    
    BenchmarkResult benchmark_concurrent_access(size_t num_operations, size_t num_threads) {
        OrderBookSimulator simulator(num_threads);
        constexpr uint32_t symbol_id = 100;
        
        auto start_time = std::chrono::high_resolution_clock::now();
        
        std::vector<std::thread> threads;
        size_t operations_per_thread = num_operations / num_threads;
        
        for (size_t t = 0; t < num_threads; ++t) {
            threads.emplace_back([&, t, operations_per_thread]() {
                for (size_t i = 0; i < operations_per_thread; ++i) {
                    if (i % 4 == 0) {
                        // Submit order
                        Side side = static_cast<Side>(side_dist_(rng_));
                        uint64_t price = price_dist_(rng_);
                        uint64_t quantity = quantity_dist_(rng_);
                        simulator.submit_order(symbol_id, side, OrderType::LIMIT, 
                                             quantity, price);
                    } else if (i % 4 == 1) {
                        // Cancel order (will fail for non-existent orders, but that's ok)
                        simulator.cancel_order(i);
                    } else if (i % 4 == 2) {
                        // Get market data
                        auto market_data = simulator.get_market_data(symbol_id);
                    } else {
                        // Get order book levels
                        auto bid_levels = simulator.get_bid_levels(symbol_id, 5);
                        auto ask_levels = simulator.get_ask_levels(symbol_id, 5);
                    }
                }
            });
        }
        
        for (auto& thread : threads) {
            thread.join();
        }
        
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
        
        auto metrics = simulator.get_performance_metrics();
        
        BenchmarkResult result;
        result.test_name = "Concurrent Access (" + std::to_string(num_threads) + " threads)";
        result.num_operations = num_operations;
        result.duration_ms = duration.count() / 1000.0;
        result.operations_per_second = (num_operations * 1000.0) / result.duration_ms;
        result.average_latency_ns = metrics.average_latency_ns;
        
        return result;
    }
    
    void print_result(const BenchmarkResult& result) {
        std::cout << std::left << std::setw(35) << result.test_name
                  << std::setw(12) << std::right << result.num_operations
                  << std::setw(12) << std::right << std::fixed << std::setprecision(2) 
                  << result.duration_ms << " ms"
                  << std::setw(15) << std::right << std::scientific << std::setprecision(2)
                  << result.operations_per_second << " ops/s"
                  << std::setw(15) << std::right << std::fixed << std::setprecision(0)
                  << result.average_latency_ns << " ns"
                  << std::endl;
    }
    
    void run_all_benchmarks() {
        std::cout << "Limit Order Book Performance Benchmarks\n";
        std::cout << "=======================================\n\n";
        
        std::cout << std::left << std::setw(35) << "Test"
                  << std::setw(12) << std::right << "Operations"
                  << std::setw(12) << std::right << "Duration"
                  << std::setw(15) << std::right << "Throughput"
                  << std::setw(15) << std::right << "Avg Latency"
                  << std::endl;
        
        std::cout << std::string(89, '-') << std::endl;
        
        // Single-threaded order submission
        print_result(benchmark_order_submission(10000, 1));
        
        // Multi-threaded order submission
        print_result(benchmark_order_submission(10000, 4));
        print_result(benchmark_order_submission(10000, 8));
        
        // Order matching performance
        print_result(benchmark_matching_performance(5000));
        
        // Market data queries
        print_result(benchmark_market_data_queries(100000));
        
        // Concurrent access patterns
        print_result(benchmark_concurrent_access(20000, 4));
        print_result(benchmark_concurrent_access(20000, 8));
        
        std::cout << "\nBenchmark completed!\n";
    }
};

int main() {
    BenchmarkSuite suite;
    suite.run_all_benchmarks();
    return 0;
}
