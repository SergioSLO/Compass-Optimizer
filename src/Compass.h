#pragma once
#include <vector>
#include <string>
#include <cstdint>

struct CMSketch {
    int depth;
    int width;
    std::vector<std::uint64_t> salts;
    std::vector<std::vector<std::uint64_t>> counts;

    CMSketch(int d = 4, int w = 1021);
    void add(const std::string &key, std::uint64_t c = 1);
    std::uint64_t estimate_point(const std::string &key) const;
};

// Estima |A ⋈ B| usando join de sketches bucket-wise
double estimate_join_cardinality(const CMSketch &A, const CMSketch &B);
