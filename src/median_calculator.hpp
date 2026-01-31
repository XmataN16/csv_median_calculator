#pragma once
/**
 * \file median_calculator.hpp
 * \brief Инкрементальный калькулятор медианы (гибрид).
 *
 * Поведение:
 *  - Пока накоплено < seed_threshold элементов — хранит их в векторе и
 *    вычисляет точную медиану (nth_element).
 *  - Как только накоплено >= seed_threshold — создаётся Boost.Accumulators
 *    с p_square_quantile (quantile_probability = 0.5) и буфер переносится в аккумулятор.
 *
 * Примечание: правильный параметр для P^2 — boost::accumulators::quantile_probability.
 */

#include <vector>
#include <optional>
#include <algorithm>
#include <limits>

#include <boost/accumulators/accumulators.hpp>
#include <boost/accumulators/statistics/stats.hpp>
#include <boost/accumulators/statistics/p_square_quantile.hpp>
#include <boost/accumulators/statistics/parameters/quantile_probability.hpp> // явно подключим параметр

namespace median {

    using namespace boost::accumulators;

    // Тип аккумулятора, оценивающего квантиль по алгоритму P^2
    using accumulator_t = accumulator_set<double, stats<tag::p_square_quantile>>;

    class median_calculator {
    public:
        explicit median_calculator(std::size_t seed_threshold = 64)
            : _seed_threshold(seed_threshold), _has_value(false), _using_psquare(false) {
        }

        void add(long double value) {
            double v = static_cast<double>(value);
            if (!_using_psquare) {
                _buffer.push_back(v);
                _has_value = true;
                if (_buffer.size() >= _seed_threshold) {
                    promote_to_psquare();
                }
            }
            else {
                // добавляем наблюдение в Boost-аккумулятор
                (*_acc)(v);
                _has_value = true;
            }
        }

        std::optional<long double> median() const {
            if (!_has_value) return std::nullopt;
            if (!_using_psquare) {
                return static_cast<long double>(exact_median_from_buffer());
            }
            else {
                // корректный способ извлечения P^2-оценки
                double m = boost::accumulators::p_square_quantile(*_acc);
                return static_cast<long double>(m);
            }
        }

        void reset() {
            _buffer.clear();
            _acc.reset();
            _using_psquare = false;
            _has_value = false;
        }

    private:
        void promote_to_psquare() {
            // Создаём аккумулятор с параметром quantile_probability = 0.5 (медиана)
            // В boost::accumulators параметр называется quantile_probability и находится в пространстве имен boost::accumulators.
            _acc.emplace(boost::accumulators::quantile_probability = 0.5);

            // "перекармливаем" буфер
            for (double v : _buffer) {
                (*_acc)(v);
            }
            _buffer.clear();
            _using_psquare = true;
        }

        double exact_median_from_buffer() const {
            if (_buffer.empty()) return std::numeric_limits<double>::quiet_NaN();
            std::vector<double> tmp = _buffer;
            const size_t n = tmp.size();
            const size_t mid = n / 2;
            std::nth_element(tmp.begin(), tmp.begin() + mid, tmp.end());
            if (n % 2 == 1) {
                return tmp[mid];
            }
            else {
                double hi = tmp[mid];
                double lo = *std::max_element(tmp.begin(), tmp.begin() + mid);
                return (lo + hi) / 2.0;
            }
        }

    private:
        std::size_t _seed_threshold;
        bool _has_value;
        bool _using_psquare;
        std::vector<double> _buffer;
        std::optional<accumulator_t> _acc;
    };

} // namespace median
