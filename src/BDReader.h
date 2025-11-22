// BDReader.h
#pragma once

#include <string>
#include <vector>

struct Table {
    std::string name;
    std::vector<std::string> columns;
    std::vector<std::vector<std::string>> data;

    // Devuelve índice de columna por nombre, o -1 si no existe
    int getColumnIndex(const std::string &col) const {
        for (size_t i = 0; i < columns.size(); ++i) {
            if (columns[i] == col) return static_cast<int>(i);
        }
        return -1;
    }
};

// Carga CSV y devuelve Table (implementado en BDReader.cpp)
Table load_csv(const std::string &path);
