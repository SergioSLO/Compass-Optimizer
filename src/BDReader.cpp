#include "BDReader.h"

#include <fstream>
#include <sstream>
#include <iostream>
#include <cctype>

// trim local para este módulo
static std::string trim(const std::string &s) {
    size_t start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    size_t end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

Table load_csv(const std::string &path) {
    Table t;
    // nombre de tabla = nombre de archivo sin ruta ni extensión
    std::string filename = path;
    {
        size_t pos = filename.find_last_of("/\\");
        if (pos != std::string::npos) filename = filename.substr(pos + 1);
        pos = filename.find_last_of('.');
        if (pos != std::string::npos) filename = filename.substr(0, pos);
    }
    t.name = filename;

    std::ifstream in(path);
    if (!in) {
        throw std::runtime_error("No se pudo abrir CSV: " + path);
    }

    std::string line;
    bool first = true;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        std::vector<std::string> fields;
        std::string cur;
        bool in_quotes = false;

        // CSV simple con comillas
        for (size_t i = 0; i < line.size(); ++i) {
            char c = line[i];
            if (c == '"') {
                in_quotes = !in_quotes;
            } else if (c == ',' && !in_quotes) {
                fields.push_back(cur);
                cur.clear();
            } else {
                cur.push_back(c);
            }
        }
        fields.push_back(cur);

        if (first) {
            for (auto &f : fields) {
                t.columns.push_back(trim(f));
            }
            first = false;
        } else {
            for (auto &f : fields) {
                f = trim(f);
            }
            t.data.push_back(fields);
        }
    }

    std::cerr << "Cargada tabla '" << t.name << "' con "
              << t.data.size() << " filas y "
              << t.columns.size() << " columnas.\n";

    return t;
}
