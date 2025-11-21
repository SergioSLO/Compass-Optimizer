#include "Compass.h"

#include <random>
#include <functional>
#include <limits>
#include <stdexcept>

CMSketch::CMSketch(int d, int w) : depth(d), width(w) {
    salts.resize(depth);
    counts.assign(depth, std::vector<std::uint64_t>(width, 0));
    std::mt19937_64 rng(123456789);
    for (int i = 0; i < depth; ++i) {
        salts[i] = rng();
    }
}

void CMSketch::add(const std::string &key, std::uint64_t c) {
    std::hash<std::string> H;
    for (int i = 0; i < depth; ++i) {
        std::uint64_t h = H(key) ^ salts[i];
        std::size_t bucket = h % static_cast<std::size_t>(width);
        counts[i][bucket] += c;
    }
}

std::uint64_t CMSketch::estimate_point(const std::string &key) const {
    std::hash<std::string> H;
    std::uint64_t est = std::numeric_limits<std::uint64_t>::max();
    for (int i = 0; i < depth; ++i) {
        std::uint64_t h = H(key) ^ salts[i];
        std::size_t bucket = h % static_cast<std::size_t>(width);
        est = std::min(est, counts[i][bucket]);
    }
    return est;
}

double estimate_join_cardinality(const CMSketch &A, const CMSketch &B) {
    if (A.depth != B.depth || A.width != B.width) {
        throw std::runtime_error("Sketches con dimensiones distintas");
    }
    int d = A.depth;
    int w = A.width;

    double total = 0.0;
    for (int i = 0; i < d; ++i) {
        long double Ji = 0.0L;
        for (int b = 0; b < w; ++b) {
            long double ca = A.counts[i][b];
            long double cb = B.counts[i][b];
            Ji += ca * cb;
        }
        total += static_cast<double>(Ji);
    }
    return total / static_cast<double>(d);
}

double estimate_join_cardinality_multi(const std::vector<CMSketch> &sketches) {
    if (sketches.empty()) return 0.0;
    int d = sketches[0].depth;
    int w = sketches[0].width;
    // consistency check
    for (const auto &s : sketches) {
        if (s.depth != d || s.width != w) {
            throw std::runtime_error("Sketches con dimensiones distintas (multi)");
        }
    }

    long double total = 0.0L;
    for (int i = 0; i < d; ++i) {
        long double Ji = 0.0L;
        for (int b = 0; b < w; ++b) {
            // product of counts across all sketches in this bucket
            long double prod = 1.0L;
            for (const auto &s : sketches) {
                prod *= static_cast<long double>(s.counts[i][b]);
                if (prod == 0.0L) break; // short-circuit
            }
            Ji += prod;
        }
        total += Ji;
    }

    return static_cast<double>(total / static_cast<long double>(d));
<<<<<<< Updated upstream
}
=======
}
>>>>>>> Stashed changes
