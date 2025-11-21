// Optimizer.cpp
#include "Optimizer.h"
#include "Compass.h"

#include <vector>
#include <iomanip>

#include <numeric>
#include <iostream>
#include <unordered_map>
#include <stdexcept>

// (keep filter_rows, build_sketch_for_table, execute_hash_join helpers as before)
// I will reuse them from your previous file; assume they're present here.
// For completeness, include them (copy-paste from your current file) if not already present.

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

// Build a frequency map for a specific column and a set of rows
static std::unordered_map<std::string, std::uint64_t> build_freq_map(
    const Table &t, const std::vector<int> &rows, const std::string &col) {
    int ci = t.getColumnIndex(col);
    if (ci < 0) {
        throw std::runtime_error("Columna " + col + " no existe en tabla " + t.name);
    }
    std::unordered_map<std::string, std::uint64_t> freq;
    freq.reserve(rows.size() * 2);
    for (int r : rows) {
        freq[t.data[r][ci]]++;
    }
    return freq;
}

// Progressive exact multi-way join count assuming all joins equate the keys' values (same domain)
static std::uint64_t execute_multi_hash_join(
    const std::vector<const Table*> &tables,
    const std::vector<std::vector<int>> &rows_per_table,
    const std::vector<std::string> &join_cols // aligned with tables
) {
    if (tables.empty()) return 0;
    // Build freq for the first table
    auto cur_freq = build_freq_map(*tables[0], rows_per_table[0], join_cols[0]);

    // For each next table, multiply counts per key
    for (size_t t = 1; t < tables.size(); ++t) {
        auto next_freq = build_freq_map(*tables[t], rows_per_table[t], join_cols[t]);
        std::unordered_map<std::string, std::uint64_t> new_freq;
        new_freq.reserve(std::min(cur_freq.size(), next_freq.size()) * 2 + 1);
        for (auto &kv : cur_freq) {
            const std::string &key = kv.first;
            auto it = next_freq.find(key);
            if (it != next_freq.end()) {
                // multiply counts
                // careful about overflow: use 128-bit or check application domain; here we use 64-bit
                new_freq[key] = kv.second * it->second;
            }
        }
        cur_freq.swap(new_freq);
        if (cur_freq.empty()) return 0;
    }

    // Sum remaining frequencies
    std::uint64_t total = 0;
    for (auto &kv : cur_freq) total += kv.second;
    return total;
}

void run_query_plan(const std::unordered_map<std::string, Table> &tables,
                    const JoinQuery &q) {
    // Verify tables exist
    for (const auto &tbl : q.tables) {
        if (!tables.count(tbl)) {
            throw std::runtime_error("Tabla " + tbl + " no encontrada entre los CSV cargados");
        }
    }

    // Build predicates per table
    std::unordered_map<std::string, std::vector<Predicate>> preds_map;
    for (const auto &p : q.predicates) {
        preds_map[p.table].push_back(p);
    }

    // Filter rows per table
    std::vector<const Table*> table_ptrs;
    std::vector<std::vector<int>> rows_per_table;
    for (const auto &tbl : q.tables) {
        const Table &t = tables.at(tbl);
        auto preds_for_table = preds_map.count(tbl) ? preds_map.at(tbl) : std::vector<Predicate>{};
        auto rows = filter_rows(t, preds_for_table);
        table_ptrs.push_back(&t);
        rows_per_table.push_back(std::move(rows));
    }

    // Determine join column to use per table.
    // A table may appear in multiple join conditions; ensure all references for the same table will be used consistently.
    std::unordered_map<std::string, std::string> table_join_col;
    for (const auto &jc : q.joins) {
        // if a table already has a join column assigned, it must match (if not, we still allow different column names;
        // the correctness depends on the domain: we will simply use the column each join condition referenced for that table).
        // For multi-way sketch / multi-way exact join below we need one join column per table; if a table appears multiple times
        // with different join columns, we require that those different join columns are actually referring to the same logical values.
        if (!table_join_col.count(jc.leftTable)) table_join_col[jc.leftTable] = jc.leftKey;
        else if (table_join_col[jc.leftTable] != jc.leftKey) {
            // warn but accept: prefer the previously chosen col; user should ensure logical equivalence.
            std::cerr << "Advertencia: tabla " << jc.leftTable << " aparece en varios JOINs con columnas distintas ("
                      << table_join_col[jc.leftTable] << " vs " << jc.leftKey << "). Usando "
                      << table_join_col[jc.leftTable] << ".\n";
        }
        if (!table_join_col.count(jc.rightTable)) table_join_col[jc.rightTable] = jc.rightKey;
        else if (table_join_col[jc.rightTable] != jc.rightKey) {
            std::cerr << "Advertencia: tabla " << jc.rightTable << " aparece en varios JOINs con columnas distintas ("
                      << table_join_col[jc.rightTable] << " vs " << jc.rightKey << "). Usando "
                      << table_join_col[jc.rightTable] << ".\n";
        }
    }

    // For tables that never appear in join conditions (rare), we can't build a join sketch; pick first column? We'll skip they won't participate in join.
    // Build sketches for tables that have a join column mapping
    std::vector<CMSketch> sketches;
    std::vector<std::string> join_cols_for_sketch;
    std::vector<const Table*> sketch_tables;
    std::vector<std::vector<int>> sketch_rows;
    for (size_t i = 0; i < q.tables.size(); ++i) {
        const std::string &tbl = q.tables[i];
        if (table_join_col.count(tbl)) {
            const Table &t = *table_ptrs[i];
            const auto &rows = rows_per_table[i];
            CMSketch sk = build_sketch_for_table(t, rows, table_join_col[tbl]);
            sketches.push_back(std::move(sk));
            join_cols_for_sketch.push_back(table_join_col[tbl]);
            sketch_tables.push_back(table_ptrs[i]);
            sketch_rows.push_back(rows);
        } else {
            std::cerr << "Nota: tabla " << tbl << " no participa en JOINs (no se construye sketch)\n";
        }
    }

    // Estimate multi-way join if we have at least 2 sketches
    double est_join = 0.0;
    if (sketches.size() >= 2) {
        est_join = estimate_join_cardinality_multi(sketches);
    } else if (sketches.size() == 1) {
        // trivial: estimated join equal to sum of frequencies (i.e., number of rows in that table after filters)
        est_join = static_cast<double>(sketch_rows[0].size());
    } else {
        est_join = 0.0;
    }

    // Execute exact multi-way join (progressive hash) only across the tables that have join columns.
    std::uint64_t real_join = 0;
    if (sketch_tables.size() >= 1) {
        real_join = execute_multi_hash_join(sketch_tables, sketch_rows, join_cols_for_sketch);
    }

    // --------- output (query plan) ---------
    std::cout << "================= COMPASS-lite Query Plan =================\n";
    // Show a compact representation of the multi-join
    std::cout << "Query: ";
    std::cout << "FROM " << q.tables.front();
    for (size_t i = 1; i < q.tables.size(); ++i) {
        std::cout << " JOIN " << q.tables[i];
    }
    std::cout << "\n\n";

    std::cout << "Tables loaded (original CSV sizes):\n";
    for (const auto &tbl : q.tables) {
        const Table &t = tables.at(tbl);
        std::cout << "  " << t.name << " : " << t.data.size() << " rows\n";
    }
    std::cout << "\n";

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

    std::cout << "Join conditions:\n";
    for (auto &jc : q.joins) {
        std::cout << "  " << jc.leftTable << "." << jc.leftKey << " = "
                  << jc.rightTable << "." << jc.rightKey << "\n";
    }
    std::cout << "\n";

    std::cout << "Plan:\n";
    for (size_t i = 0; i < q.tables.size(); ++i) {
        std::cout << "  " << (i+1) << ") Scan " << q.tables[i]
                  << " (after filters: " << rows_per_table[i].size() << " rows)\n";
    }
    std::cout << "  Build sketches on join keys for participating tables\n";
    std::cout << "  Estimate multi-way join via bucket-wise product across sketches\n";
    std::cout << "  Execute progressive hash multi-way join to get exact cardinality (for comparison)\n\n";

    std::cout << "Results:\n";
    std::cout << "  Real join cardinality       : " << real_join << "\n";
    std::cout << "  Estimated join cardinality  : " << std::fixed
              << std::setprecision(2) << est_join << "\n";
    std::cout << "===========================================================\n";
}
