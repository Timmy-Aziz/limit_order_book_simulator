#include "../include/limit_order_book.hpp"
#include <algorithm>

namespace lob {

void PriceLevel::add_order(std::shared_ptr<Order> order) {
    std::lock_guard<std::mutex> lock(level_mutex_);
    orders_.push_back(order);
    total_quantity_.fetch_add(order->quantity);
}

void PriceLevel::remove_order(uint64_t order_id) {
    std::lock_guard<std::mutex> lock(level_mutex_);
    
    auto it = std::find_if(orders_.begin(), orders_.end(),
        [order_id](const std::shared_ptr<Order>& order) {
            return order->order_id == order_id;
        });
    
    if (it != orders_.end()) {
        total_quantity_.fetch_sub((*it)->quantity);
        orders_.erase(it);
    }
}

std::shared_ptr<Order> PriceLevel::get_best_order() {
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

size_t PriceLevel::get_order_count() const {
    std::lock_guard<std::mutex> lock(level_mutex_);
    return orders_.size();
}

bool PriceLevel::is_empty() const {
    return total_quantity_.load() == 0;
}

} // namespace lob
