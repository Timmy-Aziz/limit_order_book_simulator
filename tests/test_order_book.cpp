#include "../include/limit_order_book.hpp"
#include <cassert>
#include <iostream>
#include <vector>
#include <thread>
#include <chrono>

using namespace lob;

// Test helper functions
void test_order_creation() {
    std::cout << "Testing order creation...\n";
    
    Order order(1, 100, Side::BUY, OrderType::LIMIT, 1000, 5000);
    
    assert(order.order_id == 1);
    assert(order.symbol_id == 100);
    assert(order.side == Side::BUY);
    assert(order.order_type == OrderType::LIMIT);
    assert(order.quantity == 1000);
    assert(order.price == 5000);
    assert(order.status == OrderStatus::NEW);
    assert(order.filled_quantity == 0);
    assert(!order.is_filled());
    assert(order.remaining_quantity() == 1000);
    
    std::cout << "✓ Order creation test passed\n";
}

void test_price_level() {
    std::cout << "Testing price level functionality...\n";
    
    PriceLevel level;
    assert(level.is_empty());
    assert(level.get_total_quantity() == 0);
    assert(level.get_order_count() == 0);
    
    // Add orders
    auto order1 = std::make_shared<Order>(1, 100, Side::BUY, OrderType::LIMIT, 1000, 5000);
    auto order2 = std::make_shared<Order>(2, 100, Side::BUY, OrderType::LIMIT, 2000, 5000);
    
    level.add_order(order1);
    level.add_order(order2);
    
    assert(!level.is_empty());
    assert(level.get_total_quantity() == 3000);
    assert(level.get_order_count() == 2);
    
    // Test FIFO ordering
    auto best_order = level.get_best_order();
    assert(best_order->order_id == 1);
    
    // Remove an order
    level.remove_order(1);
    assert(level.get_total_quantity() == 2000);
    assert(level.get_order_count() == 1);
    
    best_order = level.get_best_order();
    assert(best_order->order_id == 2);
    
    std::cout << "✓ Price level test passed\n";
}

void test_order_matching() {
    std::cout << "Testing order matching...\n";
    
    OrderBook book(100);
    
    // Add a sell order
    auto sell_order = std::make_shared<Order>(1, 100, Side::SELL, OrderType::LIMIT, 1000, 5000);
    book.add_order(sell_order);
    
    // Add a matching buy order
    auto buy_order = std::make_shared<Order>(2, 100, Side::BUY, OrderType::LIMIT, 1000, 5000);
    book.add_order(buy_order);
    
    // Both orders should be filled
    assert(buy_order->is_filled());
    assert(sell_order->is_filled());
    assert(book.get_trade_count() == 1);
    assert(book.get_total_volume() == 1000);
    
    // Market data should show no bid/ask spread
    auto market_data = book.get_market_data();
    assert(market_data.best_bid_price == 0);
    assert(market_data.best_ask_price == 0);
    
    std::cout << "✓ Order matching test passed\n";
}

void test_partial_fill() {
    std::cout << "Testing partial fills...\n";
    
    OrderBook book(100);
    
    // Add a large sell order
    auto sell_order = std::make_shared<Order>(1, 100, Side::SELL, OrderType::LIMIT, 5000, 5000);
    book.add_order(sell_order);
    
    // Add a smaller buy order
    auto buy_order = std::make_shared<Order>(2, 100, Side::BUY, OrderType::LIMIT, 2000, 5000);
    book.add_order(buy_order);
    
    // Buy order should be fully filled, sell order partially filled
    assert(buy_order->is_filled());
    assert(sell_order->status == OrderStatus::PARTIALLY_FILLED);
    assert(sell_order->filled_quantity == 2000);
    assert(sell_order->remaining_quantity() == 3000);
    
    // Market data should show remaining sell order
    auto market_data = book.get_market_data();
    assert(market_data.best_ask_price == 5000);
    assert(market_data.best_ask_quantity == 3000);
    
    std::cout << "✓ Partial fill test passed\n";
}

void test_price_priority() {
    std::cout << "Testing price priority...\n";
    
    OrderBook book(100);
    
    // Add sell orders at different prices
    auto sell1 = std::make_shared<Order>(1, 100, Side::SELL, OrderType::LIMIT, 1000, 5100);
    auto sell2 = std::make_shared<Order>(2, 100, Side::SELL, OrderType::LIMIT, 1000, 5000);
    auto sell3 = std::make_shared<Order>(3, 100, Side::SELL, OrderType::LIMIT, 1000, 5200);
    
    book.add_order(sell1);
    book.add_order(sell2);
    book.add_order(sell3);
    
    // Add a market buy order
    auto buy_order = std::make_shared<Order>(4, 100, Side::BUY, OrderType::MARKET, 1000, 0);
    book.add_order(buy_order);
    
    // Should match against the best price (lowest ask = 5000)
    assert(buy_order->is_filled());
    assert(sell2->is_filled());
    assert(sell1->status == OrderStatus::NEW);
    assert(sell3->status == OrderStatus::NEW);
    
    std::cout << "✓ Price priority test passed\n";
}

void test_order_cancellation() {
    std::cout << "Testing order cancellation...\n";
    
    OrderBook book(100);
    
    // Add an order
    auto order = std::make_shared<Order>(1, 100, Side::BUY, OrderType::LIMIT, 1000, 5000);
    book.add_order(order);
    
    // Cancel the order
    bool cancelled = book.cancel_order(1);
    assert(cancelled);
    assert(order->status == OrderStatus::CANCELLED);
    
    // Try to cancel non-existent order
    bool not_cancelled = book.cancel_order(999);
    assert(!not_cancelled);
    
    std::cout << "✓ Order cancellation test passed\n";
}

void test_market_data() {
    std::cout << "Testing market data...\n";
    
    OrderBook book(100);
    
    // Add some orders
    auto buy1 = std::make_shared<Order>(1, 100, Side::BUY, OrderType::LIMIT, 1000, 4900);
    auto buy2 = std::make_shared<Order>(2, 100, Side::BUY, OrderType::LIMIT, 2000, 4950);
    auto sell1 = std::make_shared<Order>(3, 100, Side::SELL, OrderType::LIMIT, 1500, 5000);
    auto sell2 = std::make_shared<Order>(4, 100, Side::SELL, OrderType::LIMIT, 1000, 5050);
    
    book.add_order(buy1);
    book.add_order(buy2);
    book.add_order(sell1);
    book.add_order(sell2);
    
    // Check market data
    auto market_data = book.get_market_data();
    assert(market_data.best_bid_price == 4950);
    assert(market_data.best_bid_quantity == 2000);
    assert(market_data.best_ask_price == 5000);
    assert(market_data.best_ask_quantity == 1500);
    
    // Check order book levels
    auto bid_levels = book.get_bid_levels(2);
    assert(bid_levels.size() == 2);
    assert(bid_levels[0].first == 4950);
    assert(bid_levels[0].second == 2000);
    assert(bid_levels[1].first == 4900);
    assert(bid_levels[1].second == 1000);
    
    auto ask_levels = book.get_ask_levels(2);
    assert(ask_levels.size() == 2);
    assert(ask_levels[0].first == 5000);
    assert(ask_levels[0].second == 1500);
    assert(ask_levels[1].first == 5050);
    assert(ask_levels[1].second == 1000);
    
    std::cout << "✓ Market data test passed\n";
}

void test_concurrent_operations() {
    std::cout << "Testing concurrent operations...\n";
    
    OrderBookSimulator simulator(4);
    
    constexpr int num_orders = 1000;
    constexpr int symbol_id = 100;
    
    std::vector<uint64_t> order_ids;
    
    // Submit orders from multiple threads
    std::vector<std::thread> threads;
    
    for (int t = 0; t < 4; ++t) {
        threads.emplace_back([&, t]() {
            for (int i = 0; i < num_orders / 4; ++i) {
                Side side = (i % 2 == 0) ? Side::BUY : Side::SELL;
                uint64_t price = 5000 + (i % 100);
                uint64_t quantity = 100 + (i % 50);
                
                uint64_t order_id = simulator.submit_order(symbol_id, side, OrderType::LIMIT, 
                                                         quantity, price);
                order_ids.push_back(order_id);
            }
        });
    }
    
    // Wait for all threads to complete
    for (auto& thread : threads) {
        thread.join();
    }
    
    // Check performance metrics
    auto metrics = simulator.get_performance_metrics();
    assert(metrics.orders_processed > 0);
    
    std::cout << "✓ Concurrent operations test passed\n";
    std::cout << "  Orders processed: " << metrics.orders_processed << "\n";
    std::cout << "  Total volume: " << metrics.total_volume << "\n";
    std::cout << "  Trade count: " << metrics.trade_count << "\n";
}

void run_all_tests() {
    std::cout << "Starting Limit Order Book tests...\n\n";
    
    test_order_creation();
    test_price_level();
    test_order_matching();
    test_partial_fill();
    test_price_priority();
    test_order_cancellation();
    test_market_data();
    test_concurrent_operations();
    
    std::cout << "\n🎉 All tests passed successfully!\n";
}

int main() {
    try {
        run_all_tests();
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Test failed with exception: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "Test failed with unknown exception" << std::endl;
        return 1;
    }
}
