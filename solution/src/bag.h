#pragma once

#include <vector>
#include <cstddef>

namespace model {

struct CollectedLoot {
    size_t id;      // ID предмета
    size_t type;    // Тип предмета
};

class Bag {
public:
    explicit Bag(size_t capacity) : capacity_(capacity) {}
    
    // Попробовать добавить предмет
    bool TryAddItem(size_t item_id, size_t item_type) {
        if (items_.size() >= capacity_) {
            return false;
        }
        items_.push_back(CollectedLoot{item_id, item_type});
        return true;
    }
    
    // Очистить рюкзак (сдать предметы)
    std::vector<CollectedLoot> Clear() {
        std::vector<CollectedLoot> result;
        std::swap(result, items_);
        return result;
    }
    
    // Получить содержимое рюкзака
    const std::vector<CollectedLoot>& GetItems() const noexcept {
        return items_;
    }
    
    // Текущее количество предметов
    size_t Size() const noexcept {
        return items_.size();
    }
    
    // Вместимость рюкзака
    size_t Capacity() const noexcept {
        return capacity_;
    }
    
    // Проверить, полон ли рюкзак
    bool IsFull() const noexcept {
        return items_.size() >= capacity_;
    }
    
    // Проверить, пуст ли рюкзак
    bool IsEmpty() const noexcept {
        return items_.empty();
    }

private:
    size_t capacity_;
    std::vector<CollectedLoot> items_;
};

}  // namespace model