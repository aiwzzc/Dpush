#pragma once

#include <unordered_map>
#include <list>
#include <optional>

template<typename Key, typename Value>
class LRUCache {

public:
    LRUCache(int capacity) : capacity_(capacity >= 0 ? capacity : 0) {}

    static_assert(std::is_default_constructible<Value>::value, "Value must be default constructible");

    std::optional<Value> get(Key key) {
        auto it_for_hash = this->key_to_iter_.find(key);

        if(it_for_hash == this->key_to_iter_.end()) return std::nullopt;

        auto it_for_list = it_for_hash->second;
        this->cache_list_.splice(this->cache_list_.begin(), this->cache_list_, it_for_list);

        return it_for_list->second;
    }

    void put(Key key, Value value) {
        auto it_for_hash = this->key_to_iter_.find(key);

        if(it_for_hash != this->key_to_iter_.end()) {
            auto it_for_list = it_for_hash->second;
            it_for_list->second = value;
            this->cache_list_.splice(this->cache_list_.begin(), this->cache_list_, it_for_list);

            return;
        }

        this->cache_list_.emplace_front(key, value);
        this->key_to_iter_[key] = this->cache_list_.begin();

        if(this->key_to_iter_.size() > this->capacity_) {
            this->key_to_iter_.erase(this->cache_list_.back().first);
            this->cache_list_.pop_back();
        }
    }

private:
    int capacity_;
    std::list<std::pair<Key, Value>> cache_list_;
    std::unordered_map<Key, typename std::list<std::pair<Key, Value>>::iterator> key_to_iter_;
};