#pragma once
#include <queue>
#include <set>
#include <stdexcept>

class NumberPool {
public:
    explicit NumberPool(int max_size) {
        for (int i = 0; i < max_size; ++i) {
            available_ids.push(i);
        }
    }

    int get_next_number() {
        if (available_ids.empty()) {
            throw std::runtime_error("No more IDs available");
        }
        int id = available_ids.front();
        available_ids.pop();
        in_use.insert(id);
        return id;
    }

    void release_number(int id) {
        if (in_use.erase(id) > 0) {
            available_ids.push(id);
        } else {
            throw std::runtime_error("Tried to release an ID that is not in use");
        }
    }

private:
    std::queue<int> available_ids;
    std::set<int> in_use;
};
