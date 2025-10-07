#include "include/simple_order_book.hpp"
#include <iostream>
#include <iomanip>

using namespace lob;

void print_market_data(const MarketDataSnapshot& snapshot) {
    std::cout << "\n=== Market Data Update ===\n";
    std::cout << "Symbol: " << snapshot.symbol_id << "\n";
    std::cout << "Best Bid: $" << snapshot.best_bid_price << " x " << snapshot.best_bid_quantity << "\n";
    std::cout << "Best Ask: $" << snapshot.best_ask_price << " x " << snapshot.best_ask_quantity << "\n";
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

int main() {
    std::cout << "Limit Order Book Simulator - Demo\n";
    std::cout << "=================================\n\n";
    
    try {
        // Create order book for symbol 100
        SimpleOrderBook book(100);
        
        // Register callbacks
        book.register_market_data_callback(print_market_data);
        book.register_trade_callback(print_trade);
        
        std::cout << "Adding initial orders to create a spread...\n";
        
        // Add some initial orders to create a spread
        auto sell_order1 = std::make_shared<Order>(1, 100, Side::SELL, OrderType::LIMIT, 1000, 5005);
        auto sell_order2 = std::make_shared<Order>(2, 100, Side::SELL, OrderType::LIMIT, 2000, 5010);
        auto buy_order1 = std::make_shared<Order>(3, 100, Side::BUY, OrderType::LIMIT, 1500, 4995);
        auto buy_order2 = std::make_shared<Order>(4, 100, Side::BUY, OrderType::LIMIT, 1000, 4990);
        
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
        
        // Test partial fills
        std::cout << "\nAdding a large sell order...\n";
        auto large_sell = std::make_shared<Order>(6, 100, Side::SELL, OrderType::LIMIT, 5000, 5000);
        book.add_order(large_sell);
        
        std::cout << "Adding a smaller buy order to partially fill...\n";
        auto partial_buy = std::make_shared<Order>(7, 100, Side::BUY, OrderType::LIMIT, 2000, 5000);
        book.add_order(partial_buy);
        
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
        
        std::cout << "Demo completed successfully!\n";
        
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
