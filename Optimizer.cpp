#include "Optimizer.h"
#include "Compass.h"

#include <vector>
#include <iomanip>

#include <numeric>
#include <iostream>
#include <unordered_map>
#include <stdexcept>

// Filtra filas de una tabla según sus predicados (solo '=' y AND)
static std::vector<int> filter_rows(const Table &t,
                                    const std::vector<Predicate> &preds_for_table) {
    std::vector<int> rows;
    if (preds_for_table.empty()) {
        rows.resize(t.data.size());
        std::iota(rows.begin(), rows.end(), 0);
        return rows;
    }

    struct ColTest {
        int col_idx;
        std::string value;
    };
    std::vector<ColTest> tests;
    for (const auto &p : preds_for_table) {
        int idx = t.getColumnIndex(p.column);
        if (idx < 0) {
            std::cerr << "Advertencia: columna " << p.column
                      << " no existe en tabla " << t.name
                      << ". Predicado ignorado.\n";
            continue;
        }
        tests.push_back({idx, p.value});
    }

    for (int r = 0; r < static_cast<int>(t.data.size()); ++r) {
        bool ok = true;
        for (auto &ct : tests) {
            if (t.data[r][ct.col_idx] != ct.value) {
                ok = false;
                break;
            }
        }
        if (ok) rows.push_back(r);
    }
    return rows;
}

// Construye sketch sobre una columna de join para las filas filtradas
static CMSketch build_sketch_for_table(const Table &t,
                                       const std::vector<int> &rows,
                                       const std::string &joinCol) {
    int col_idx = t.getColumnIndex(joinCol);
    if (col_idx < 0) {
        throw std::runtime_error("Columna de join " + joinCol +
                                 " no existe en tabla " + t.name);
    }
    CMSketch sk(4, 1021);
    for (int r : rows) {
        const std::string &key = t.data[r][col_idx];
        sk.add(key, 1);
    }
    return sk;
}

// Ejecuta join exacto por hash para obtener la cardinalidad real
static std::uint64_t execute_hash_join(
    const Table &left, const std::vector<int> &leftRows, const std::string &leftKey,
    const Table &right, const std::vector<int> &rightRows, const std::string &rightKey
) {
    int li = left.getColumnIndex(leftKey);
    int ri = right.getColumnIndex(rightKey);
    if (li < 0 || ri < 0) {
        throw std::runtime_error("Columnas de join no encontradas");
    }

    std::unordered_map<std::string, std::uint64_t> freq;
    freq.reserve(leftRows.size() * 2);

    for (int r : leftRows) {
        const std::string &k = left.data[r][li];
        freq[k]++;
    }

    std::uint64_t join_count = 0;
    for (int r : rightRows) {
        const std::string &k = right.data[r][ri];
        auto it = freq.find(k);
        if (it != freq.end()) {
            join_count += it->second;
        }
    }

    return join_count;
}

void run_query_plan(const std::unordered_map<std::string, Table> &tables,
                    const JoinQuery &q) {
    if (!tables.count(q.leftTable) || !tables.count(q.rightTable)) {
        throw std::runtime_error("Alguna tabla del SQL no coincide con los CSV cargados");
    }

    const Table &left  = tables.at(q.leftTable);
    const Table &right = tables.at(q.rightTable);

    // separar predicados por tabla
    std::vector<Predicate> preds_left, preds_right;
    for (const auto &p : q.predicates) {
        if (p.table == q.leftTable) {
            preds_left.push_back(p);
        } else if (p.table == q.rightTable) {
            preds_right.push_back(p);
        } else {
            std::cerr << "Advertencia: predicado con tabla desconocida: "
                      << p.table << ". Ignorado.\n";
        }
    }

    // Filtrar filas
    std::vector<int> leftRows  = filter_rows(left,  preds_left);
    std::vector<int> rightRows = filter_rows(right, preds_right);

    // Sketches
    CMSketch skLeft  = build_sketch_for_table(left,  leftRows,  q.leftKey);
    CMSketch skRight = build_sketch_for_table(right, rightRows, q.rightKey);

    // Estimación
    double est_join = estimate_join_cardinality(skLeft, skRight);

    // Join real para comparar
    std::uint64_t real_join = execute_hash_join(left, leftRows, q.leftKey,
                                                right, rightRows, q.rightKey);

    // --------- salida tipo query plan ---------

    std::cout << "================= COMPASS-lite Query Plan =================\n";
    std::cout << "Query: JOIN " << q.leftTable << " (" << q.leftKey << ")"
              << "  ⨝  " << q.rightTable << " (" << q.rightKey << ")\n\n";

    std::cout << "Tables loaded:\n";
    std::cout << "  " << left.name  << " : " << left.data.size()  << " rows\n";
    std::cout << "  " << right.name << " : " << right.data.size() << " rows\n\n";

    std::cout << "Predicates:\n";
    if (q.predicates.empty()) {
        std::cout << "  (none)\n";
    } else {
        for (auto &p : q.predicates) {
            std::cout << "  " << p.table << "." << p.column
                      << " = '" << p.value << "'\n";
        }
    }
    std::cout << "\n";

    std::cout << "Plan:\n";
    std::cout << "  1) Scan " << q.leftTable
              << " (after filters: " << leftRows.size()  << " rows)\n";
    std::cout << "  2) Scan " << q.rightTable
              << " (after filters: " << rightRows.size() << " rows)\n";
    std::cout << "  3) Build sketches on join keys ("
              << q.leftTable << "." << q.leftKey << " , "
              << q.rightTable << "." << q.rightKey << ")\n";
    std::cout << "  4) Estimate join cardinality via bucket-wise sketch join\n";
    std::cout << "  5) Execute hash join to get exact cardinality (for comparison)\n\n";

    std::cout << "Results:\n";
    std::cout << "  Real join cardinality       : " << real_join << "\n";
    std::cout << "  Estimated join cardinality  : " << std::fixed
              << std::setprecision(2) << est_join << "\n";
    std::cout << "===========================================================\n";
}
