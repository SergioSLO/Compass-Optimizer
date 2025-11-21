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
                  << " [--compass] table1.csv table2.csv ... query.sql\n";
        return 1;
    }

    bool use_compass = false;
    int first_csv = 1;

    if (std::string(argv[1]) == "--compass") {
        use_compass = true;
        first_csv = 2;
        if (argc < 4) {
            std::cerr << "Uso: " << argv[0]
                      << " --compass table1.csv table2.csv ... query.sql\n";
            return 1;
        }
    }

    try {
        std::string sql_path = argv[argc - 1];

        std::unordered_map<std::string, Table> tables;
        for (int i = first_csv; i < argc - 1; ++i) {
            std::string csv_path = argv[i];
            Table t = load_csv(csv_path);
            tables[t.name] = std::move(t);
        }

        JoinQuery q = parse_sql_file(sql_path);

        run_query_plan_compass(tables, q);  // OPCIÓN 1
        //run_query_plan(tables, q);          // OPCIÓN 2


    } catch (const std::exception &ex) {
        std::cerr << "Error: " << ex.what() << "\n";
        return 1;
    }

    return 0;
}
