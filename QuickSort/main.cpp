// Подбор порога перехода на сортировку вставками и сравнение со std::sort.
// Запускать в конфигурации Release.
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <random>
#include <string>
#include <vector>

#include "Sort.h"

namespace {

template <typename T, typename SortFn>
double measureMs(const std::vector<std::vector<T>>& inputs, SortFn sortFn) {
    double best = 1e18;
    for (int rep = 0; rep < 5; ++rep) {
        std::vector<std::vector<T>> data = inputs;
        auto start = std::chrono::steady_clock::now();
        for (auto& v : data) sortFn(v);
        auto end = std::chrono::steady_clock::now();
        best = std::min(best, std::chrono::duration<double, std::milli>(end - start).count());
    }
    return best;
}

template <typename T, typename Compare>
void tune(const char* title, const std::vector<std::vector<T>>& inputs, Compare comp) {
    std::printf("\n%s\n threshold   time, ms\n", title);
    for (int t : {1, 2, 4, 6, 8, 10, 12, 16, 20, 24, 32, 48, 64, 96, 128}) {
        double ms = measureMs(inputs, [&](std::vector<T>& v) {
            algo::sortWithThreshold(v.data(), v.data() + v.size(), comp, t);
        });
        std::printf(" %9d   %8.2f\n", t, ms);
    }
    double stdMs = measureMs(inputs, [&](std::vector<T>& v) { std::sort(v.begin(), v.end(), comp); });
    double ourMs = measureMs(inputs, [&](std::vector<T>& v) { algo::sort(v.data(), v.data() + v.size(), comp); });
    std::printf(" algo::sort (threshold %d): %.2f ms, std::sort: %.2f ms\n",
                static_cast<int>(algo::kInsertionSortThreshold), ourMs, stdMs);
}

}  // namespace

int main() {
    std::mt19937 rng(2026);

    std::vector<std::vector<int>> ints(20, std::vector<int>(100000));
    for (auto& v : ints)
        for (int& x : v) x = static_cast<int>(rng());
    tune("int, 20 x 100000 random", ints, [](int a, int b) { return a < b; });

    std::vector<std::vector<std::string>> strings(5, std::vector<std::string>(50000));
    for (auto& v : strings)
        for (auto& s : v) s = std::to_string(rng());
    tune("std::string, 5 x 50000 random", strings,
         [](const std::string& a, const std::string& b) { return a < b; });
    return 0;
}
