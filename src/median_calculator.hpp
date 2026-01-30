#pragma once
/**
 * \file median_calculator.hpp
 * \brief Incremental median calculator using two heaps (max-heap + min-heap)
 */

#include <queue>
#include <vector>
#include <functional>
#include <optional>

namespace median {

class median_calculator {
public:
    median_calculator() = default;

    // Add a value
    void add(long double value) {
        if (_max_heap.empty() || value <= _max_heap.top()) {
            _max_heap.push(value);
        } else {
            _min_heap.push(value);
        }
        balance();
    }

    // Current median (nullopt if empty)
    std::optional<long double> median() const {
        if (_max_heap.empty() && _min_heap.empty()) return std::nullopt;
        if (_max_heap.size() == _min_heap.size()) {
            long double a = _max_heap.top();
            long double b = _min_heap.top();
            return (a + b) / 2.0L;
        } else if (_max_heap.size() > _min_heap.size()) {
            return _max_heap.top();
        } else {
            return _min_heap.top();
        }
    }

private:
    void balance() {
        if (_max_heap.size() > _min_heap.size() + 1) {
            _min_heap.push(_max_heap.top());
            _max_heap.pop();
        } else if (_min_heap.size() > _max_heap.size() + 1) {
            _max_heap.push(_min_heap.top());
            _min_heap.pop();
        }
    }

    std::priority_queue<long double> _max_heap; // max at top
    std::priority_queue<long double, std::vector<long double>, std::greater<long double>> _min_heap; // min at top
};

}  // namespace median
