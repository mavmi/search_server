#pragma once

#include <cstdlib>
#include <future>
#include <map>
#include <numeric>
#include <random>
#include <string>
#include <vector>
#include <mutex>

using namespace std::string_literals;

template <typename Key, typename Value>
class ConcurrentMap {
public:
    static_assert(std::is_integral_v<Key>, "ConcurrentMap supports only integer keys"s);

    struct PartOfTheMap {
        std::map<Key, Value> map_;
        std::mutex mutex_;
    };

    struct Access {
        friend class ConcurrentMap;

        Access(Value& value, std::mutex& map_mutex) : ref_to_value(value), mutex_(map_mutex) {
        }

        ~Access() {
            mutex_.unlock();
        }

        Value& ref_to_value;

    private:
        std::mutex& mutex_;
    };

    explicit ConcurrentMap(size_t bucket_count) {
        count_of_maps_ = bucket_count;
        parts_of_the_map_ = std::vector<PartOfTheMap>(count_of_maps_);
    }

    Access operator[](const Key& key) {
        uint64_t part_number = key % count_of_maps_;
        PartOfTheMap& part_of_the_map = parts_of_the_map_[part_number];

        part_of_the_map.mutex_.lock();
        if (!part_of_the_map.map_.count(key)) {
            part_of_the_map.map_[key] = Value();
        }

        return Access(part_of_the_map.map_[key], part_of_the_map.mutex_);
    }

    std::map<Key, Value> BuildOrdinaryMap() {
        std::map<Key, Value> whole_map_;

        for (auto& [map_, mutex_] : parts_of_the_map_) {
            std::lock_guard<std::mutex> guard(mutex_);
            whole_map_.insert(map_.begin(), map_.end());
        }

        return whole_map_;
    }

    void Erase(Key key) {
        uint64_t part_number = key % count_of_maps_;
        PartOfTheMap& part_of_the_map = parts_of_the_map_[part_number];

        std::lock_guard<std::mutex> guard(part_of_the_map.mutex_);
        part_of_the_map.map_.erase(key);
    }

private:
    std::vector<PartOfTheMap> parts_of_the_map_;
    size_t count_of_maps_;
};