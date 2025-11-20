#include <iostream>
#include <unordered_map>
#include <string>
#include <exception>

#include "BDReader.h"
#include "SQLParser.h"
#include "Optimizer.h"

int main(int argc, char **argv) {
    std::ios::sync_with_stdio(false);
    std::cin.tie(nullptr);

    if (argc < 3) {
        std::cerr << "Uso: " << argv[0]
                  << " table1.csv table2.csv ... query.sql\n";
        return 1;
    }

    try {
        std::string sql_path = argv[argc - 1];

        // Cargar CSVs
        std::unordered_map<std::string, Table> tables;
        for (int i = 1; i < argc - 1; ++i) {
            std::string csv_path = argv[i];
            Table t = load_csv(csv_path);
            tables[t.name] = std::move(t);
        }

        // Parsear SQL
        JoinQuery q = parse_sql_file(sql_path);

        // Ejecutar "optimización" y plan
        run_query_plan(tables, q);

    } catch (const std::exception &ex) {
        std::cerr << "Error: " << ex.what() << "\n";
        return 1;
    }

    return 0;
}
