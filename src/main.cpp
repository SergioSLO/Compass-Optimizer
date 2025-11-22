#include <exception>
#include <iostream>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "BDReader.h"
#include "Optimizer.h"
#include "PlanVisualizer.h"
#include "SQLParser.h"

namespace {

enum class PlannerMode { Greedy, Compass, Both };

PlannerMode parse_mode(const std::string &value) {
    if (value == "greedy") return PlannerMode::Greedy;
    if (value == "compass") return PlannerMode::Compass;
    if (value == "both") return PlannerMode::Both;
    throw std::runtime_error("Modo desconocido: " + value +
                             " (valores válidos: greedy, compass, both)");
}

}  // namespace

int main(int argc, char **argv) {
    std::ios::sync_with_stdio(false);
    std::cin.tie(nullptr);

    try {
        PlannerMode mode = PlannerMode::Both;
        std::string dot_path;
        std::string png_path;
        std::vector<std::string> inputs;

        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];
            if (arg.rfind("--mode=", 0) == 0) {
                mode = parse_mode(arg.substr(7));
            } else if (arg.rfind("--plan-dot=", 0) == 0) {
                dot_path = arg.substr(11);
            } else if (arg.rfind("--plan-png=", 0) == 0) {
                png_path = arg.substr(11);
            } else {
                inputs.push_back(arg);
            }
        }

        if (inputs.size() < 2) {
            std::cerr << "Uso: " << argv[0]
                      << " [--mode=greedy|compass|both] [--plan-dot=path.dot]"
                         " [--plan-png=path.png] table1.csv ... query.sql\n";
            return 1;
        }

        std::string sql_path = inputs.back();
        inputs.pop_back();

        std::unordered_map<std::string, Table> tables;
        for (const auto &csv_path : inputs) {
            Table t = load_csv(csv_path);
            tables[t.name] = std::move(t);
        }

        JoinQuery q = parse_sql_file(sql_path);

        if (mode == PlannerMode::Greedy || mode == PlannerMode::Both) {
            run_query_plan(tables, q);
        }

        std::unique_ptr<JoinTreeNode> compass_tree;
        if (mode == PlannerMode::Compass || mode == PlannerMode::Both) {
            compass_tree = run_query_plan_compass(tables, q);
        }

        if (!dot_path.empty() && compass_tree) {
            export_plan_to_dot(compass_tree.get(), dot_path);
            std::cout << "Plan exportado a " << dot_path << "\n";
            if (!png_path.empty()) {
                if (dot_to_png(dot_path, png_path)) {
                    std::cout << "PNG generado en " << png_path << "\n";
                } else {
                    std::cout << "No se pudo ejecutar dot para generar PNG.\n";
                }
            }
        } else if (!png_path.empty()) {
            std::cout << "Se solicitó PNG pero no hay archivo DOT. "
                         "Use --plan-dot primero.\n";
        }

    } catch (const std::exception &ex) {
        std::cerr << "Error: " << ex.what() << "\n";
        return 1;
    }

    return 0;
}
