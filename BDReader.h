#pragma once
#include <string>
#include <vector>

struct Table {
    std::string name;
    std::vector<std::string> columns;            // nombres de columnas
    std::vector<std::vector<std::string>> data;  // data[row][col]

    int getColumnIndex(const std::string &col) const {
        for (size_t i = 0; i < columns.size(); ++i) {
            if (columns[i] == col) return static_cast<int>(i);
        }
        return -1;
    }
};

Table load_csv(const std::string &path);
