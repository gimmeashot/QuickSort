// Release benchmark: warm-up, identical inputs, validation outside timing, CSV and SVG.
#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>
#include "Sort.h"

namespace {
constexpr int kWarmups = 2;
constexpr int kRepeats = 5;
struct Point {
    std::string type, order, algorithm;
    int x;
    double ms;
};

// Every trial sorts a fresh copy; the reference also checks element preservation.
template <class T, class Sort>
double measure(const std::vector<std::vector<T>>& inputs, Sort sort) {
    auto expected = inputs;
    for (auto& row : expected) std::sort(row.begin(), row.end());
    std::vector<double> samples;
    for (int trial = -kWarmups; trial < kRepeats; ++trial) {
        auto data = inputs;
        const auto start = std::chrono::steady_clock::now();
        for (auto& row : data) sort(row);
        const auto stop = std::chrono::steady_clock::now();
        if (data != expected) throw std::runtime_error("Incorrect sorting result");
        if (trial >= 0) {
            const double ms = std::chrono::duration<double, std::milli>(stop - start).count();
            samples.push_back(ms / inputs.size());
        }
    }
    std::sort(samples.begin(), samples.end());
    return samples[samples.size() / 2]; // Median milliseconds per array.
}

template <class T> T valueOf(int value);
template <> int valueOf<int>(int value) { return value; }
template <> std::string valueOf<std::string>(int value) {
    // Fixed width makes lexical order agree with numeric order.
    const auto text = std::to_string(value);
    return std::string(8 - text.size(), '0') + text;
}

// Distinct elements ensure reverse order is the worst case for insertion sort.
template <class T>
std::vector<std::vector<T>> makeInputs(int n, int batches, const std::string& order) {
    std::mt19937 engine(2026);
    std::vector<std::vector<T>> inputs(batches);
    for (auto& row : inputs) {
        row.reserve(n);
        for (int i = 0; i < n; ++i) row.push_back(valueOf<T>(i));
        if (order == "random") std::shuffle(row.begin(), row.end(), engine);
        else if (order == "reverse") std::reverse(row.begin(), row.end());
    }
    return inputs;
}

template <class T>
void benchmark(const std::string& type, std::vector<Point>& timings, std::vector<Point>& thresholds) {
    const auto less = [](const T& a, const T& b) { return a < b; };
    for (const std::string order : {"random", "sorted", "reverse"}) {
        for (int n : {128, 256, 512, 1024, 2048, 4096}) {
            const auto inputs = makeInputs<T>(n, std::max(4, 16384 / n), order);
            const double quick = measure(inputs, [&](auto& row) {
                algo::sort(row.begin(), row.end(), less);
            });
            const double insertion = measure(inputs, [&](auto& row) {
                algo::insertionSort(row.begin(), row.end(), less);
            });
            const double standard = measure(inputs, [&](auto& row) {
                std::sort(row.begin(), row.end(), less);
            });
            timings.push_back({type, order, "QuickSort", n, quick});
            timings.push_back({type, order, "InsertionSort", n, insertion});
            timings.push_back({type, order, "std::sort", n, standard});
            std::cout << type << ' ' << order << " n=" << n << ": "
                      << quick << ", " << insertion << ", " << standard << " ms\n";
        }
        // Use identical arrays at each threshold; do not time huge insertion-only arrays.
        const auto inputs = makeInputs<T>(4096, 4, order);
        for (int threshold : {2, 4, 8, 12, 16, 24, 32, 48, 64, 96, 128}) {
            const double ms = measure(inputs, [&](auto& row) {
                algo::sortWithThreshold(row.begin(), row.end(), less, threshold);
            });
            thresholds.push_back({type, order, "QuickSort", threshold, ms});
        }
    }
}

std::ofstream output(const std::filesystem::path& path) {
    std::ofstream file(path);
    if (!file) throw std::runtime_error("Cannot open output: " + path.string());
    file.exceptions(std::ios::badbit | std::ios::failbit);
    file << std::setprecision(9);
    return file;
}

void writeCsv(const std::filesystem::path& path, const std::vector<Point>& points, const char* axis) {
    auto file = output(path);
    file << "type,order,algorithm," << axis << ",median_ms_per_array\n";
    for (const auto& p : points)
        file << p.type << ',' << p.order << ',' << p.algorithm << ',' << p.x << ',' << p.ms << '\n';
    file.close();
}

// Six panels: two element types times three input orders. SVG opens in any browser.
void writeSvg(const std::filesystem::path& path, const std::vector<Point>& points, bool thresholds) {
    auto file = output(path);
    file << "<svg xmlns='http://www.w3.org/2000/svg' width='1200' height='1120' viewBox='0 0 1200 1120'>"
            "<rect width='1200' height='1120' fill='white'/>"
            "<g font-family='sans-serif' fill='#172033'>"
            "<text x='40' y='32' font-size='22'>";
    file << (thresholds ? "QuickSort threshold sweep (n = 4096)" : "Sorting time versus array size") << "</text>";
    file << "<text x='40' y='56' font-size='13'>Release; median of 5 trials; 2 warm-ups; milliseconds per array; linear axes</text>";
    const char* colors[] = {"#185abd", "#b24010", "#08785c"};
    const char* names[] = {"QuickSort", "InsertionSort", "std::sort"};
    for (int a = 0; a < (thresholds ? 1 : 3); ++a)
        file << "<text x='" << 40 + a * 190 << "' y='80' fill='" << colors[a]
             << "' font-size='14'>" << names[a] << "</text>";
    int panel = 0;
    for (const std::string order : {"random", "sorted", "reverse"}) {
        for (const std::string type : {"int", "string"}) {
            const double left = 75 + (panel % 2) * 590, top = 135 + (panel / 2) * 320;
            constexpr double width = 485, height = 220;
            double ymax = 0; int xmax = 0;
            for (const auto& p : points) if (p.type == type && p.order == order) {
                ymax = std::max(ymax, p.ms); xmax = std::max(xmax, p.x);
            }
            ymax = std::max(0.000001, ymax * 1.1);
            file << "<text x='" << left << "' y='" << top - 16 << "' font-size='17'>"
                 << type << " / " << order << "</text>";
            for (int tick = 0; tick <= 4; ++tick) {
                const double y = top + height - height * tick / 4;
                file << "<path d='M " << left << ' ' << y << " h " << width << "' stroke='#dce1e8'/>"
                     << "<text x='" << left - 8 << "' y='" << y + 4 << "' text-anchor='end' font-size='11'>"
                     << std::setprecision(3) << ymax * tick / 4 << "</text>" << std::setprecision(9);
                const double x = left + width * tick / 4;
                file << "<text x='" << x << "' y='" << top + height + 20
                     << "' text-anchor='middle' font-size='11'>" << xmax * tick / 4 << "</text>";
            }
            file << "<text x='" << left << "' y='" << top - 2 << "' font-size='10'>ms</text>"
                 << "<text x='" << left + width / 2 << "' y='" << top + height + 40
                 << "' text-anchor='middle' font-size='12'>" << (thresholds ? "Threshold" : "Elements") << "</text>";
            for (int a = 0; a < (thresholds ? 1 : 3); ++a) {
                file << "<polyline fill='none' stroke='" << colors[a] << "' stroke-width='2' points='";
                for (const auto& p : points) if (p.type == type && p.order == order && p.algorithm == names[a])
                    file << left + width * p.x / xmax << ',' << top + height - height * p.ms / ymax << ' ';
                file << "'/>";
            }
            ++panel;
        }
    }
    file << "<text x='40' y='1090' font-size='12'>Measured on this machine; no universal threshold or speed advantage is implied.</text></g></svg>";
    file.close();
}
} // namespace

int main(int argc, char** argv) {
#ifndef NDEBUG
    std::cerr << "Build Release (/O2 /DNDEBUG) for meaningful timings.\n";
    return 1;
#endif
    try {
        if (argc > 2) throw std::invalid_argument("Usage: QuickSort [output-directory]");
        const std::filesystem::path dir = argc == 2 ? argv[1] : "benchmark-results";
        std::filesystem::create_directories(dir);
        std::vector<Point> timings, thresholds;
        benchmark<int>("int", timings, thresholds);
        benchmark<std::string>("string", timings, thresholds);
        writeCsv(dir / "timings.csv", timings, "size");
        writeCsv(dir / "thresholds.csv", thresholds, "threshold");
        writeSvg(dir / "timings.svg", timings, false);
        writeSvg(dir / "thresholds.svg", thresholds, true);
        std::cout << "Results saved to " << dir.string() << '\n';
    } catch (const std::exception& e) {
        std::cerr << e.what() << '\n';
        return 1;
    }
    return 0;
}
