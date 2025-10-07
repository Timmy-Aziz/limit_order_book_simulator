#include "../include/simple_order_book.hpp"
#include <algorithm>
#include <iostream>

namespace lob {

bool SimpleOrderBook::add_order(std::shared_ptr<Order> order) {
    // Store the order first
    {
        std::lock_guard<std::mutex> lock(orders_mutex_);
        orders_[order->order_id] = order;
    }
    
    // Process based on order type
    switch (order->order_type) {
        case OrderType::LIMIT:
            process_limit_order(order);
            break;
        case OrderType::MARKET:
            process_market_order(order);
            break;
        case OrderType::STOP:
            // For simplicity, treat as limit order for now
            process_limit_order(order);
            break;
        default:
            order->status = OrderStatus::REJECTED;
            return false;
    }
    
    // Notify market data subscribers
    notify_market_data();
    
    return true;
}

void SimpleOrderBook::process_limit_order(std::shared_ptr<Order> order) {
    std::lock_guard<std::mutex> lock(book_mutex_);
    
    // Try to match against opposite side
    auto& opposite_side = (order->side == Side::BUY) ? asks_ : bids_;
    
    if (try_match_order(order, opposite_side)) {
        // Order was fully or partially matched
        if (order->is_filled()) {
            order->status = OrderStatus::FILLED;
        } else {
            order->status = OrderStatus::PARTIALLY_FILLED;
            // Add remaining quantity to the book
            add_to_book(order);
        }
    } else {
        // No match found, add to book
        order->status = OrderStatus::NEW;
        add_to_book(order);
    }
}

void SimpleOrderBook::process_market_order(std::shared_ptr<Order> order) {
    std::lock_guard<std::mutex> lock(book_mutex_);
    
    // Try to match against opposite side
    auto& opposite_side = (order->side == Side::BUY) ? asks_ : bids_;
    
    if (try_match_order(order, opposite_side)) {
        if (order->is_filled()) {
            order->status = OrderStatus::FILLED;
        } else {
            order->status = OrderStatus::PARTIALLY_FILLED;
        }
    } else {
        // No liquidity available
        order->status = OrderStatus::REJECTED;
    }
}

bool SimpleOrderBook::try_match_order(std::shared_ptr<Order> order, 
                                    std::map<uint64_t, std::unique_ptr<PriceLevel>>& opposite_side) {
    if (opposite_side.empty()) {
        return false;
    }
    
    // For buy orders, match against lowest ask prices
    // For sell orders, match against highest bid prices
    auto it = (order->side == Side::BUY) ? 
        opposite_side.begin() : std::prev(opposite_side.end());
    
    while (it != opposite_side.end() && order->remaining_quantity() > 0) {
        auto& [price, price_level] = *it;
        
        // Check if price is acceptable
        bool price_acceptable = (order->side == Side::BUY) ? 
            (price <= order->price) : (price >= order->price);
        
        if (!price_acceptable) {
            break;
        }
        
        // Try to match against orders at this price level
        while (order->remaining_quantity() > 0) {
            auto matching_order = price_level->get_best_order();
            if (!matching_order) {
                break;
            }
            
            uint64_t trade_quantity = std::min(order->remaining_quantity(), 
                                             matching_order->remaining_quantity());
            
            // Execute the trade
            execute_trade(order, matching_order, trade_quantity);
            
            // Remove fully filled orders from the book
            if (matching_order->is_filled()) {
                price_level->remove_order(matching_order->order_id);
                matching_order->status = OrderStatus::FILLED;
            }
        }
        
        // Remove empty price levels
        if (price_level->is_empty()) {
            it = opposite_side.erase(it);
            if (opposite_side.empty()) break;
        } else {
            if (order->side == Side::BUY) {
                ++it;
            } else {
                if (it == opposite_side.begin()) break;
                --it;
            }
        }
    }
    
    return order->filled_quantity > 0;
}

void SimpleOrderBook::execute_trade(std::shared_ptr<Order> order1, std::shared_ptr<Order> order2, 
                                  uint64_t quantity) {
    // Determine buy and sell orders
    auto buy_order = (order1->side == Side::BUY) ? order1 : order2;
    auto sell_order = (order1->side == Side::SELL) ? order1 : order2;
    
    // Create trade record
    Trade trade(next_trade_id_++, buy_order->order_id, sell_order->order_id,
               symbol_id_, quantity, sell_order->price);
    
    // Update order quantities
    order1->filled_quantity += quantity;
    order2->filled_quantity += quantity;
    
    // Update statistics
    total_volume_ += quantity;
    trade_count_++;
    
    // Notify trade subscribers
    notify_trade(trade);
}

void SimpleOrderBook::add_to_book(std::shared_ptr<Order> order) {
    auto& side = (order->side == Side::BUY) ? bids_ : asks_;
    auto price = order->price;
    
    auto it = side.find(price);
    if (it == side.end()) {
        side[price] = std::make_unique<PriceLevel>();
    }
    
    side[price]->add_order(order);
}

bool SimpleOrderBook::cancel_order(uint64_t order_id) {
    std::shared_ptr<Order> order;
    
    // Find the order
    {
        std::lock_guard<std::mutex> lock(orders_mutex_);
        auto it = orders_.find(order_id);
        if (it == orders_.end()) {
            return false;
        }
        order = it->second;
    }
    
    if (order->status == OrderStatus::FILLED || order->status == OrderStatus::CANCELLED) {
        return false;
    }
    
    // Remove from book if it's not fully filled
    if (order->filled_quantity < order->quantity) {
        std::lock_guard<std::mutex> lock(book_mutex_);
        
        auto& side = (order->side == Side::BUY) ? bids_ : asks_;
        auto it = side.find(order->price);
        
        if (it != side.end()) {
            it->second->remove_order(order_id);
            
            // Remove empty price levels
            if (it->second->is_empty()) {
                side.erase(it);
            }
        }
    }
    
    order->status = OrderStatus::CANCELLED;
    notify_market_data();
    
    return true;
}

bool SimpleOrderBook::modify_order(uint64_t order_id, uint64_t new_quantity, uint64_t new_price) {
    std::shared_ptr<Order> order;
    
    // Find the order
    {
        std::lock_guard<std::mutex> lock(orders_mutex_);
        auto it = orders_.find(order_id);
        if (it == orders_.end()) {
            return false;
        }
        order = it->second;
    }
    
    if (order->status == OrderStatus::FILLED || order->status == OrderStatus::CANCELLED) {
        return false;
    }
    
    // Cancel the existing order and create a new one
    cancel_order(order_id);
    
    // Create new order with modified parameters
    auto new_order = std::make_shared<Order>(order_id, order->symbol_id, order->side,
                                           order->order_type, new_quantity, 
                                           new_price > 0 ? new_price : order->price);
    
    return add_order(new_order);
}

MarketDataSnapshot SimpleOrderBook::get_market_data() const {
    MarketDataSnapshot snapshot(symbol_id_);
    
    std::lock_guard<std::mutex> lock(book_mutex_);
    
    snapshot.timestamp = std::chrono::duration_cast<std::chrono::microseconds>(
        std::chrono::high_resolution_clock::now().time_since_epoch()).count();
    
    // Get best bid
    if (!bids_.empty()) {
        auto best_bid_it = std::prev(bids_.end());
        snapshot.best_bid_price = best_bid_it->first;
        snapshot.best_bid_quantity = best_bid_it->second->get_total_quantity();
    }
    
    // Get best ask
    if (!asks_.empty()) {
        auto best_ask_it = asks_.begin();
        snapshot.best_ask_price = best_ask_it->first;
        snapshot.best_ask_quantity = best_ask_it->second->get_total_quantity();
    }
    
    snapshot.volume = total_volume_;
    
    return snapshot;
}

std::vector<std::pair<uint64_t, uint64_t>> SimpleOrderBook::get_bid_levels(uint32_t depth) const {
    std::vector<std::pair<uint64_t, uint64_t>> levels;
    
    std::lock_guard<std::mutex> lock(book_mutex_);
    
    auto it = bids_.rbegin();
    for (uint32_t i = 0; i < depth && it != bids_.rend(); ++i, ++it) {
        levels.emplace_back(it->first, it->second->get_total_quantity());
    }
    
    return levels;
}

std::vector<std::pair<uint64_t, uint64_t>> SimpleOrderBook::get_ask_levels(uint32_t depth) const {
    std::vector<std::pair<uint64_t, uint64_t>> levels;
    
    std::lock_guard<std::mutex> lock(book_mutex_);
    
    auto it = asks_.begin();
    for (uint32_t i = 0; i < depth && it != asks_.end(); ++i, ++it) {
        levels.emplace_back(it->first, it->second->get_total_quantity());
    }
    
    return levels;
}

void SimpleOrderBook::register_market_data_callback(std::function<void(const MarketDataSnapshot&)> callback) {
    std::lock_guard<std::mutex> lock(callbacks_mutex_);
    market_data_callbacks_.push_back(callback);
}

void SimpleOrderBook::register_trade_callback(std::function<void(const Trade&)> callback) {
    std::lock_guard<std::mutex> lock(callbacks_mutex_);
    trade_callbacks_.push_back(callback);
}

void SimpleOrderBook::notify_market_data() {
    std::lock_guard<std::mutex> lock(callbacks_mutex_);
    
    auto snapshot = get_market_data();
    for (const auto& callback : market_data_callbacks_) {
        callback(snapshot);
    }
}

void SimpleOrderBook::notify_trade(const Trade& trade) {
    std::lock_guard<std::mutex> lock(callbacks_mutex_);
    
    for (const auto& callback : trade_callbacks_) {
        callback(trade);
    }
}

} // namespace lob
