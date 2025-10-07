#include "../include/limit_order_book.hpp"
#include <iostream>
#include <iomanip>

using namespace lob;

void print_market_data(const MarketDataSnapshot& snapshot) {
    std::cout << "\n=== Market Data Update ===\n";
    std::cout << "Symbol: " << snapshot.symbol_id << "\n";
    std::cout << "Best Bid: $" << snapshot.best_bid_price << " x " << snapshot.best_bid_quantity << "\n";
    std::cout << "Best Ask: $" << snapshot.best_ask_price << " x " << snapshot.best_ask_quantity << "\n";
    std::cout << "Last Trade: $" << snapshot.last_trade_price << " x " << snapshot.last_trade_quantity << "\n";
    std::cout << "Total Volume: " << snapshot.volume << "\n";
    std::cout << "========================\n";
}

void print_trade(const Trade& trade) {
    std::cout << "\n*** TRADE EXECUTED ***\n";
    std::cout << "Trade ID: " << trade.trade_id << "\n";
    std::cout << "Symbol: " << trade.symbol_id << "\n";
    std::cout << "Price: $" << trade.price << "\n";
    std::cout << "Quantity: " << trade.quantity << "\n";
    std::cout << "Buy Order: " << trade.buy_order_id << "\n";
    std::cout << "Sell Order: " << trade.sell_order_id << "\n";
    std::cout << "**********************\n";
}

void print_order_book_levels(const std::vector<std::pair<uint64_t, uint64_t>>& levels, 
                           const std::string& side, uint32_t depth = 5) {
    std::cout << "\n" << side << " Levels (Top " << depth << "):\n";
    std::cout << "Price\t\tQuantity\n";
    std::cout << "-----\t\t--------\n";
    
    for (size_t i = 0; i < std::min(levels.size(), static_cast<size_t>(depth)); ++i) {
        std::cout << "$" << levels[i].first << "\t\t" << levels[i].second << "\n";
    }
}

void basic_order_book_demo() {
    std::cout << "=== Basic Order Book Demo ===\n\n";
    
    OrderBook book(100); // Symbol ID 100
    
    // Register callbacks
    book.register_market_data_callback(print_market_data);
    book.register_trade_callback(print_trade);
    
    // Add some initial orders to create a spread
    auto sell_order1 = std::make_shared<Order>(1, 100, Side::SELL, OrderType::LIMIT, 1000, 5005);
    auto sell_order2 = std::make_shared<Order>(2, 100, Side::SELL, OrderType::LIMIT, 2000, 5010);
    auto buy_order1 = std::make_shared<Order>(3, 100, Side::BUY, OrderType::LIMIT, 1500, 4995);
    auto buy_order2 = std::make_shared<Order>(4, 100, Side::BUY, OrderType::LIMIT, 1000, 4990);
    
    std::cout << "Adding initial orders...\n";
    book.add_order(sell_order1);
    book.add_order(sell_order2);
    book.add_order(buy_order1);
    book.add_order(buy_order2);
    
    // Show order book levels
    auto bid_levels = book.get_bid_levels(5);
    auto ask_levels = book.get_ask_levels(5);
    
    print_order_book_levels(bid_levels, "BID", 5);
    print_order_book_levels(ask_levels, "ASK", 5);
    
    // Add a market order that will match
    std::cout << "\nAdding market buy order for 800 shares...\n";
    auto market_buy = std::make_shared<Order>(5, 100, Side::BUY, OrderType::MARKET, 800, 0);
    book.add_order(market_buy);
    
    // Show updated order book
    bid_levels = book.get_bid_levels(5);
    ask_levels = book.get_ask_levels(5);
    
    print_order_book_levels(bid_levels, "BID", 5);
    print_order_book_levels(ask_levels, "ASK", 5);
    
    // Cancel an order
    std::cout << "\nCancelling order ID 4...\n";
    book.cancel_order(4);
    
    // Show final order book
    bid_levels = book.get_bid_levels(5);
    ask_levels = book.get_ask_levels(5);
    
    print_order_book_levels(bid_levels, "BID", 5);
    print_order_book_levels(ask_levels, "ASK", 5);
    
    // Show statistics
    std::cout << "\n=== Order Book Statistics ===\n";
    std::cout << "Total Volume: " << book.get_total_volume() << "\n";
    std::cout << "Trade Count: " << book.get_trade_count() << "\n";
    std::cout << "============================\n\n";
}

void concurrent_simulation_demo() {
    std::cout << "=== Concurrent Simulation Demo ===\n\n";
    
    OrderBookSimulator simulator(4); // 4 worker threads
    
    // Register callbacks for multiple symbols
    simulator.register_trade_callback(100, [](const Trade& trade) {
        std::cout << "Symbol 100 Trade: $" << trade.price << " x " << trade.quantity << "\n";
    });
    
    simulator.register_trade_callback(101, [](const Trade& trade) {
        std::cout << "Symbol 101 Trade: $" << trade.price << " x " << trade.quantity << "\n";
    });
    
    std::cout << "Submitting orders to multiple symbols...\n";
    
    // Submit orders to different symbols
    std::vector<uint64_t> order_ids;
    
    // Symbol 100 orders
    order_ids.push_back(simulator.submit_order(100, Side::SELL, OrderType::LIMIT, 1000, 5000));
    order_ids.push_back(simulator.submit_order(100, Side::BUY, OrderType::LIMIT, 1000, 5000));
    
    // Symbol 101 orders
    order_ids.push_back(simulator.submit_order(101, Side::SELL, OrderType::LIMIT, 500, 3000));
    order_ids.push_back(simulator.submit_order(101, Side::BUY, OrderType::LIMIT, 500, 3000));
    
    // Show market data for both symbols
    auto market_data_100 = simulator.get_market_data(100);
    auto market_data_101 = simulator.get_market_data(101);
    
    std::cout << "\nSymbol 100 Market Data:\n";
    print_market_data(market_data_100);
    
    std::cout << "\nSymbol 101 Market Data:\n";
    print_market_data(market_data_101);
    
    // Show performance metrics
    auto metrics = simulator.get_performance_metrics();
    std::cout << "\n=== Performance Metrics ===\n";
    std::cout << "Orders Processed: " << metrics.orders_processed << "\n";
    std::cout << "Total Volume: " << metrics.total_volume << "\n";
    std::cout << "Trade Count: " << metrics.trade_count << "\n";
    std::cout << "Average Latency: " << std::fixed << std::setprecision(2) 
              << metrics.average_latency_ns / 1000.0 << " μs\n";
    std::cout << "==========================\n\n";
}

void performance_comparison_demo() {
    std::cout << "=== Performance Comparison Demo ===\n\n";
    
    constexpr size_t num_orders = 10000;
    
    // Single-threaded
    std::cout << "Running single-threaded simulation...\n";
    auto start = std::chrono::high_resolution_clock::now();
    
    OrderBookSimulator single_threaded(1);
    for (size_t i = 0; i < num_orders; ++i) {
        Side side = (i % 2 == 0) ? Side::BUY : Side::SELL;
        uint64_t price = 5000 + (i % 100);
        single_threaded.submit_order(100, side, OrderType::LIMIT, 1000, price);
    }
    
    auto single_end = std::chrono::high_resolution_clock::now();
    auto single_duration = std::chrono::duration_cast<std::chrono::microseconds>(single_end - start);
    
    // Multi-threaded
    std::cout << "Running multi-threaded simulation...\n";
    start = std::chrono::high_resolution_clock::now();
    
    OrderBookSimulator multi_threaded(4);
    for (size_t i = 0; i < num_orders; ++i) {
        Side side = (i % 2 == 0) ? Side::BUY : Side::SELL;
        uint64_t price = 5000 + (i % 100);
        multi_threaded.submit_order(100, side, OrderType::LIMIT, 1000, price);
    }
    
    auto multi_end = std::chrono::high_resolution_clock::now();
    auto multi_duration = std::chrono::duration_cast<std::chrono::microseconds>(multi_end - start);
    
    // Results
    std::cout << "\n=== Performance Results ===\n";
    std::cout << "Orders: " << num_orders << "\n";
    std::cout << "Single-threaded: " << single_duration.count() / 1000.0 << " ms\n";
    std::cout << "Multi-threaded: " << multi_duration.count() / 1000.0 << " ms\n";
    std::cout << "Speedup: " << std::fixed << std::setprecision(2) 
              << static_cast<double>(single_duration.count()) / multi_duration.count() << "x\n";
    std::cout << "==========================\n\n";
}

int main() {
    std::cout << "Limit Order Book Simulator - Basic Usage Examples\n";
    std::cout << "================================================\n\n";
    
    try {
        basic_order_book_demo();
        concurrent_simulation_demo();
        performance_comparison_demo();
        
        std::cout << "All examples completed successfully!\n";
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
