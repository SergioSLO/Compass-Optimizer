#include "Optimizer.h"

#include "Compass.h"

#include <algorithm>
#include <cstdlib>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <limits>
#include <numeric>
#include <queue>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace {

struct QueryContext {
    std::vector<std::string> table_order;
    std::unordered_map<std::string, const Table*> table_ptrs;
    std::unordered_map<std::string, std::vector<int>> rows_per_table;
    std::unordered_map<std::string, CMSketch> sketchCache;
    std::vector<std::vector<std::string>> components;
};

struct DPEntry {
    bool valid = false;
    double cost = 0.0;
    double est_card = 0.0;
    std::unique_ptr<JoinTreeNode> tree;
};

std::string sk_key(const std::string &table, const std::string &col) {
    return table + "::" + col;
}

std::vector<int> filter_rows(const Table &t, const std::vector<Predicate> &preds) {
    std::vector<int> rows;
    if (preds.empty()) {
        rows.resize(t.data.size());
        std::iota(rows.begin(), rows.end(), 0);
        return rows;
    }

    struct Test {
        int col_idx;
        std::string op;
        std::string val;
        bool is_number;
        double num_val;
    };

    std::vector<Test> tests;
    for (const auto &p : preds) {
        int idx = t.getColumnIndex(p.column);
        if (idx < 0) {
            std::cerr << "Advertencia: columna " << p.column
                      << " no existe en tabla " << t.name << ". Se ignora predicado.\n";
            continue;
        }

        Test tt;
        tt.col_idx = idx;
        tt.op = p.op;
        tt.val = p.value;
        char *endptr = nullptr;
        double v = std::strtod(p.value.c_str(), &endptr);
        if (endptr != p.value.c_str() && *endptr == '\0') {
            tt.is_number = true;
            tt.num_val = v;
        } else {
            tt.is_number = false;
        }
        tests.push_back(tt);
    }

    for (int r = 0; r < static_cast<int>(t.data.size()); ++r) {
        bool ok = true;
        for (const auto &tt : tests) {
            const std::string &cell = t.data[r][tt.col_idx];
            if (tt.is_number) {
                char *e2 = nullptr;
                double cell_v = std::strtod(cell.c_str(), &e2);
                if (e2 == cell.c_str() || *e2 != '\0') {
                    ok = false;
                    break;
                }
                if      (tt.op == "="  && !(cell_v == tt.num_val)) ok = false;
                else if (tt.op == "!=" && !(cell_v != tt.num_val)) ok = false;
                else if (tt.op == "<>" && !(cell_v != tt.num_val)) ok = false;
                else if (tt.op == "<"  && !(cell_v <  tt.num_val)) ok = false;
                else if (tt.op == ">"  && !(cell_v >  tt.num_val)) ok = false;
                else if (tt.op == "<=" && !(cell_v <= tt.num_val)) ok = false;
                else if (tt.op == ">=" && !(cell_v >= tt.num_val)) ok = false;
            } else {
                if      (tt.op == "="  && !(cell == tt.val)) ok = false;
                else if (tt.op == "!=" && !(cell != tt.val)) ok = false;
                else if (tt.op == "<>" && !(cell != tt.val)) ok = false;
                else if (tt.op == "<"  && !(cell <  tt.val)) ok = false;
                else if (tt.op == ">"  && !(cell >  tt.val)) ok = false;
                else if (tt.op == "<=" && !(cell <= tt.val)) ok = false;
                else if (tt.op == ">=" && !(cell >= tt.val)) ok = false;
            }
            if (!ok) break;
        }
        if (ok) rows.push_back(r);
    }
    return rows;
}

CMSketch build_sketch_for_table(const Table &t,
                                const std::vector<int> &rows,
                                const std::string &joinCol) {
    int col_idx = t.getColumnIndex(joinCol);
    if (col_idx < 0) {
        throw std::runtime_error("Columna " + joinCol + " no existe en tabla " + t.name);
    }
    CMSketch sk(4, 1021);
    for (int r : rows) {
        sk.add(t.data[r][col_idx], 1);
    }
    return sk;
}

void build_sketch_cache(const std::unordered_map<std::string, Table> &tables,
                        const std::unordered_map<std::string, std::vector<int>> &rows_per_table,
                        const JoinQuery &q,
                        std::unordered_map<std::string, CMSketch> &cache) {
    for (const auto &jc : q.joins) {
        auto add = [&](const std::string &tbl, const std::string &col) {
            std::string key = sk_key(tbl, col);
            if (cache.count(key)) return;
            const Table &t = tables.at(tbl);
            cache.emplace(key, build_sketch_for_table(t, rows_per_table.at(tbl), col));
        };
        add(jc.leftTable, jc.leftKey);
        add(jc.rightTable, jc.rightKey);
    }
}

std::vector<std::vector<std::string>> compute_join_components(const JoinQuery &q) {
    std::unordered_map<std::string, std::vector<std::string>> adj;
    for (const auto &tbl : q.tables) adj[tbl];  // asegurar nodo
    for (const auto &jc : q.joins) {
        adj[jc.leftTable].push_back(jc.rightTable);
        adj[jc.rightTable].push_back(jc.leftTable);
    }

    std::unordered_set<std::string> visited;
    std::vector<std::vector<std::string>> comps;
    for (const auto &entry : adj) {
        const std::string &start = entry.first;
        if (visited.count(start)) continue;
        std::vector<std::string> comp;
        std::queue<std::string> qn;
        qn.push(start);
        visited.insert(start);
        while (!qn.empty()) {
            std::string cur = qn.front();
            qn.pop();
            comp.push_back(cur);
            for (const auto &nxt : adj[cur]) {
                if (!visited.count(nxt)) {
                    visited.insert(nxt);
                    qn.push(nxt);
                }
            }
        }
        comps.push_back(comp);
    }
    return comps;
}

std::vector<JoinCondition> joins_for_tables(const std::vector<JoinCondition> &all,
                                            const std::vector<std::string> &tables) {
    std::unordered_set<std::string> set(tables.begin(), tables.end());
    std::vector<JoinCondition> res;
    for (const auto &jc : all) {
        if (set.count(jc.leftTable) && set.count(jc.rightTable)) {
            res.push_back(jc);
        }
    }
    return res;
}

double normalize_join_estimate(double left_rows, double right_rows, double est) {
    if (est > 0.0) return est;
    double prod = left_rows * right_rows;
    if (prod <= 0.0) return 1.0;
    double fallback = std::sqrt(prod);
    return std::max(1.0, fallback);
}

double compute_join_cost(double left_rows, double right_rows, double join_rows) {
    double left = std::max(1.0, left_rows);
    double right = std::max(1.0, right_rows);
    double joinv = std::max(1.0, join_rows);
    const double overhead_factor = 0.5;
    return joinv + overhead_factor * (left + right);
}

double estimate_component_with_sketches(
    const std::vector<std::string> &component_tables,
    const std::vector<JoinCondition> &component_joins,
    const std::unordered_map<std::string, CMSketch> &cache) {
    if (component_joins.empty()) return 0.0;

    std::unordered_set<std::string> used_tables;
    std::unordered_set<std::string> used_keys;
    std::vector<CMSketch> sketches;

    for (const auto &jc : component_joins) {
        std::string kl = sk_key(jc.leftTable, jc.leftKey);
        if (!used_keys.count(kl)) {
            auto itL = cache.find(kl);
            if (itL != cache.end()) {
                sketches.push_back(itL->second);
                used_keys.insert(kl);
                used_tables.insert(jc.leftTable);
            }
        }
        std::string kr = sk_key(jc.rightTable, jc.rightKey);
        if (!used_keys.count(kr)) {
            auto itR = cache.find(kr);
            if (itR != cache.end()) {
                sketches.push_back(itR->second);
                used_keys.insert(kr);
                used_tables.insert(jc.rightTable);
            }
        }
    }

    if (sketches.empty()) return 0.0;

    double est = estimate_join_cardinality_multi(sketches);

    if (used_tables.size() < component_tables.size()) {
        // Falta al menos una tabla (por ejemplo, no había CSV), retornar 0 para que
        // el caller detecte que la estimación no está completa.
        return 0.0;
    }

    return est;
}

std::uint64_t execute_exact_component_join(
    const std::vector<std::string> &component_tables,
    const std::vector<JoinCondition> &component_joins,
    const std::unordered_map<std::string, const Table*> &table_ptrs,
    const std::unordered_map<std::string, std::vector<int>> &rows_per_table) {
    if (component_joins.empty()) {
        std::uint64_t prod = 1;
        for (const auto &tbl : component_tables) {
            prod *= static_cast<std::uint64_t>(rows_per_table.at(tbl).size());
        }
        return prod;
    }

    JoinCondition seed = component_joins.front();
    std::vector<std::pair<std::unordered_map<std::string, std::string>, std::uint64_t>> tuples;

    const Table *L = table_ptrs.at(seed.leftTable);
    const Table *R = table_ptrs.at(seed.rightTable);
    const auto &Lrows = rows_per_table.at(seed.leftTable);
    const auto &Rrows = rows_per_table.at(seed.rightTable);
    int Li = L->getColumnIndex(seed.leftKey);
    int Ri = R->getColumnIndex(seed.rightKey);
    if (Li < 0 || Ri < 0) return 0;

    std::unordered_map<std::string, std::vector<int>> right_map;
    for (int r : Rrows) {
        right_map[R->data[r][Ri]].push_back(r);
    }
    for (int l : Lrows) {
        const std::string &lv = L->data[l][Li];
        auto it = right_map.find(lv);
        if (it == right_map.end()) continue;
        for (int rr : it->second) {
            (void)rr;
            std::unordered_map<std::string, std::string> tup;
            tup[seed.leftTable + "." + seed.leftKey] = lv;
            tup[seed.rightTable + "." + seed.rightKey] = lv;
            tuples.emplace_back(std::move(tup), 1);
        }
    }

    if (tuples.empty()) return 0;

    std::unordered_set<size_t> used;
    used.insert(0);
    bool progress = true;
    while (progress) {
        progress = false;
        for (size_t i = 0; i < component_joins.size(); ++i) {
            if (used.count(i)) continue;
            const auto &jc = component_joins[i];
            std::string left_key = jc.leftTable + "." + jc.leftKey;
            std::string right_key = jc.rightTable + "." + jc.rightKey;

            bool left_present = tuples.front().first.count(left_key);
            bool right_present = tuples.front().first.count(right_key);
            if (!left_present && !right_present) continue;

            const std::string present_table = left_present ? jc.leftTable : jc.rightTable;
            const std::string present_col = left_present ? jc.leftKey : jc.rightKey;
            const std::string missing_table = left_present ? jc.rightTable : jc.leftTable;
            const std::string missing_col = left_present ? jc.rightKey : jc.leftKey;

            const Table *missing = table_ptrs.at(missing_table);
            const auto &missing_rows = rows_per_table.at(missing_table);
            int miss_idx = missing->getColumnIndex(missing_col);
            if (miss_idx < 0) {
                used.insert(i);
                continue;
            }
            std::unordered_map<std::string, std::vector<int>> miss_map;
            for (int r : missing_rows) {
                miss_map[missing->data[r][miss_idx]].push_back(r);
            }

            std::vector<std::pair<std::unordered_map<std::string, std::string>, std::uint64_t>> next;
            for (auto &entry : tuples) {
                auto tuple_vals = entry.first;
                std::uint64_t mult = entry.second;
                auto itv = tuple_vals.find(present_table + "." + present_col);
                if (itv == tuple_vals.end()) continue;
                auto itlist = miss_map.find(itv->second);
                if (itlist == miss_map.end()) continue;
                for (int row : itlist->second) {
                    (void)row;
                    auto new_tuple = tuple_vals;
                    new_tuple[missing_table + "." + missing_col] = itv->second;
                    next.emplace_back(std::move(new_tuple), mult);
                }
            }
            tuples.swap(next);
            used.insert(i);
            progress = true;
            if (tuples.empty()) return 0;
        }
    }

    std::uint64_t total = 0;
    for (auto &entry : tuples) total += entry.second;
    return total;
}

void print_sequential_plan(const std::vector<std::string> &order,
                           const std::vector<double> &step_cards,
                           const std::string &label) {
    std::cout << label << " ";
    for (size_t i = 0; i < order.size(); ++i) {
        if (i) std::cout << " -> ";
        std::cout << order[i];
    }
    std::cout << "\n";
    for (size_t i = 0; i < step_cards.size(); ++i) {
        std::cout << "   Step " << (i + 1) << ": join prefix("
                  << order[i] << ") with " << order[i + 1]
                  << "  est = " << std::fixed << std::setprecision(2)
                  << step_cards[i] << "\n";
    }
}

bool find_join_estimate(const std::string &newTable,
                        const std::unordered_set<std::string> &prefix,
                        const std::vector<JoinCondition> &joins,
                        const std::unordered_map<std::string, CMSketch> &cache,
                        double &estimate) {
    double best = std::numeric_limits<double>::infinity();
    bool found = false;
    for (const auto &jc : joins) {
        auto get_est = [&](const std::string &leftTbl,
                           const std::string &leftCol,
                           const std::string &rightTbl,
                           const std::string &rightCol) -> bool {
            std::string kl = sk_key(leftTbl, leftCol);
            std::string kr = sk_key(rightTbl, rightCol);
            auto itL = cache.find(kl);
            auto itR = cache.find(kr);
            if (itL == cache.end() || itR == cache.end()) return false;
            double est = estimate_join_cardinality(itL->second, itR->second);
            best = std::min(best, est);
            return true;
        };

        if (jc.leftTable == newTable && prefix.count(jc.rightTable)) {
            if (get_est(jc.leftTable, jc.leftKey, jc.rightTable, jc.rightKey)) {
                found = true;
            }
        } else if (jc.rightTable == newTable && prefix.count(jc.leftTable)) {
            if (get_est(jc.rightTable, jc.rightKey, jc.leftTable, jc.leftKey)) {
                found = true;
            }
        }
    }
    if (!found) return false;
    estimate = best;
    return true;
}

double evaluate_left_deep_order(
    const std::vector<std::string> &order,
    const std::vector<JoinCondition> &joins,
    const std::unordered_map<std::string, CMSketch> &cache,
    const std::unordered_map<std::string, std::vector<int>> &rows_per_table,
    std::vector<double> &step_cards) {
    if (order.empty()) return 0.0;
    std::unordered_set<std::string> prefix;
    prefix.insert(order[0]);
    double total = rows_per_table.at(order[0]).size();
    double prefix_rows = total;
    step_cards.clear();
    for (size_t i = 1; i < order.size(); ++i) {
        double est = 0.0;
        if (!find_join_estimate(order[i], prefix, joins, cache, est)) {
            return std::numeric_limits<double>::infinity();
        }
        double new_rows = rows_per_table.at(order[i]).size();
        est = normalize_join_estimate(prefix_rows, new_rows, est);
        step_cards.push_back(est);
        total += compute_join_cost(prefix_rows, new_rows, est);
        prefix_rows = est;
        prefix.insert(order[i]);
    }
    return total;
}

std::vector<std::string> greedy_left_deep_order(
    const std::vector<std::string> &tables,
    const std::vector<JoinCondition> &joins,
    const std::unordered_map<std::string, std::vector<int>> &rows_per_table,
    const std::unordered_map<std::string, CMSketch> &cache,
    std::vector<double> &step_cards,
    double &total_cost) {
    std::vector<std::string> remaining = tables;
    std::sort(remaining.begin(), remaining.end(),
              [&](const std::string &a, const std::string &b) {
                  return rows_per_table.at(a).size() < rows_per_table.at(b).size();
              });

    if (remaining.empty()) return {};

    std::vector<std::string> order;
    order.push_back(remaining.front());
    remaining.erase(remaining.begin());
    std::unordered_set<std::string> prefix(order.begin(), order.end());

    step_cards.clear();
    total_cost = rows_per_table.at(order[0]).size();
    double prefix_rows = total_cost;

    while (!remaining.empty()) {
        double best_cost_delta = std::numeric_limits<double>::infinity();
        size_t best_idx = 0;
        bool found = false;
        for (size_t i = 0; i < remaining.size(); ++i) {
            double est = 0.0;
            if (!find_join_estimate(remaining[i], prefix, joins, cache, est)) {
                continue;
            }
            double candidate_rows = rows_per_table.at(remaining[i]).size();
            est = normalize_join_estimate(prefix_rows, candidate_rows, est);
            double cand_cost = compute_join_cost(prefix_rows, candidate_rows, est);
            if (cand_cost < best_cost_delta) {
                best_cost_delta = cand_cost;
                best_idx = i;
                found = true;
            }
        }
        if (!found) {
            return {};
        }
        const std::string chosen = remaining[best_idx];
        double chosen_rows = rows_per_table.at(chosen).size();
        double est = 0.0;
        find_join_estimate(chosen, prefix, joins, cache, est);
        est = normalize_join_estimate(prefix_rows, chosen_rows, est);
        order.push_back(chosen);
        prefix.insert(remaining[best_idx]);
        remaining.erase(remaining.begin() + best_idx);
        step_cards.push_back(est);
        total_cost += compute_join_cost(prefix_rows, chosen_rows, est);
        prefix_rows = est;
    }
    return order;
}

std::vector<std::string> best_left_deep_order(
    const std::vector<std::string> &tables,
    const std::vector<JoinCondition> &joins,
    const std::unordered_map<std::string, std::vector<int>> &rows_per_table,
    const std::unordered_map<std::string, CMSketch> &cache,
    std::vector<double> &step_cards,
    double &best_cost) {
    if (tables.size() <= 7) {
        std::vector<std::string> perm = tables;
        std::sort(perm.begin(), perm.end());
        std::vector<std::string> best = perm;
        best_cost = std::numeric_limits<double>::infinity();
        std::vector<double> tmp;
        do {
            double cost = evaluate_left_deep_order(perm, joins, cache, rows_per_table, tmp);
            if (cost < best_cost) {
                best_cost = cost;
                best = perm;
                step_cards = tmp;
            }
        } while (std::next_permutation(perm.begin(), perm.end()));
        if (!std::isfinite(best_cost)) {
            step_cards.clear();
            return {};
        }
        return best;
    }

    return greedy_left_deep_order(tables, joins, rows_per_table, cache, step_cards, best_cost);
}

std::vector<std::string> subset_to_tables(int mask,
                                          const std::vector<std::string> &names) {
    std::vector<std::string> res;
    for (size_t i = 0; i < names.size(); ++i) {
        if (mask & (1 << i)) res.push_back(names[i]);
    }
    return res;
}

std::unique_ptr<JoinTreeNode> clone_tree(const JoinTreeNode *node) {
    if (!node) return nullptr;
    auto copy = std::make_unique<JoinTreeNode>();
    copy->tables = node->tables;
    copy->est_card = node->est_card;
    copy->cost = node->cost;
    copy->is_leaf = node->is_leaf;
    copy->leaf_table = node->leaf_table;
    copy->left = clone_tree(node->left.get());
    copy->right = clone_tree(node->right.get());
    return copy;
}

bool estimate_join_between_masks(
    int leftMask,
    int rightMask,
    const std::unordered_map<std::string, int> &index_map,
    const std::vector<JoinCondition> &joins,
    const std::unordered_map<std::string, CMSketch> &cache,
    double &estimate) {
    for (const auto &jc : joins) {
        int li = index_map.at(jc.leftTable);
        int ri = index_map.at(jc.rightTable);
        bool left_in_left = leftMask & (1 << li);
        bool right_in_left = leftMask & (1 << ri);
        bool left_in_right = rightMask & (1 << li);
        bool right_in_right = rightMask & (1 << ri);

        if ((left_in_left && right_in_right) || (right_in_left && left_in_right)) {
            std::string kl = sk_key(jc.leftTable, jc.leftKey);
            std::string kr = sk_key(jc.rightTable, jc.rightKey);
            auto itL = cache.find(kl);
            auto itR = cache.find(kr);
            if (itL == cache.end() || itR == cache.end()) continue;
            estimate = estimate_join_cardinality(itL->second, itR->second);
            return true;
        }
    }
    return false;
}

void print_join_tree(const JoinTreeNode *node,
                     const std::string &indent = "",
                     bool is_left = true) {
    if (!node) return;
    std::string branch = is_left ? "└─L " : "└─R ";
    if (node->is_leaf) {
        std::cout << indent << branch << "Scan " << node->leaf_table
                  << "  (rows=" << node->est_card << ")\n";
    } else {
        std::cout << indent << branch << "Join(";
        for (size_t i = 0; i < node->tables.size(); ++i) {
            if (i) std::cout << ",";
            std::cout << node->tables[i];
        }
        std::cout << ")  est=" << std::fixed << std::setprecision(2)
                  << node->est_card << "  cost=" << node->cost << "\n";
        std::string next = indent + "   ";
        print_join_tree(node->left.get(), next, true);
        print_join_tree(node->right.get(), next, false);
    }
}

QueryContext prepare_context(const std::unordered_map<std::string, Table> &tables,
                             const JoinQuery &q) {
    QueryContext ctx;
    std::unordered_map<std::string, std::vector<Predicate>> preds_map;
    for (const auto &p : q.predicates) {
        preds_map[p.table].push_back(p);
    }

    ctx.table_order = q.tables;
    for (const auto &tbl_name : q.tables) {
        auto it = tables.find(tbl_name);
        if (it == tables.end()) {
            throw std::runtime_error("Tabla " + tbl_name + " no encontrada entre los CSV");
        }
        ctx.table_ptrs[tbl_name] = &it->second;
        auto preds = preds_map.count(tbl_name) ? preds_map.at(tbl_name)
                                               : std::vector<Predicate>{};
        ctx.rows_per_table[tbl_name] = filter_rows(it->second, preds);
    }

    build_sketch_cache(tables, ctx.rows_per_table, q, ctx.sketchCache);
    ctx.components = compute_join_components(q);
    return ctx;
}

}  // namespace

void run_query_plan(const std::unordered_map<std::string, Table> &tables,
                    const JoinQuery &q) {
    QueryContext ctx = prepare_context(tables, q);

    std::cout << "================= COMPASS-lite (Left-deep) =================\n";
    std::cout << "Tablas cargadas:\n";
    for (const auto &tbl : q.tables) {
        std::cout << "  " << tbl << " -> " << tables.at(tbl).data.size() << " filas, "
                  << ctx.rows_per_table[tbl].size() << " tras filtros\n";
    }
    std::cout << "\n";

    double total_estimate = 1.0;
    std::uint64_t total_real = 1;

    for (const auto &component : ctx.components) {
        if (component.size() == 1) {
            const auto &name = component.front();
            std::uint64_t rows = ctx.rows_per_table[name].size();
            std::cout << "Componente aislado: " << name << " (" << rows << " filas)\n\n";
            total_estimate *= rows;
            total_real *= rows;
            continue;
        }

        auto comp_joins = joins_for_tables(q.joins, component);
        std::vector<double> step_cards;
        double cost = 0.0;
        auto order = best_left_deep_order(component, comp_joins,
                                          ctx.rows_per_table, ctx.sketchCache,
                                          step_cards, cost);
        if (order.empty()) {
            std::cout << "No se pudo construir plan left-deep para componente.\n";
            continue;
        }

        double est_component = estimate_component_with_sketches(component, comp_joins,
                                                                ctx.sketchCache);
        if (est_component == 0.0 && !step_cards.empty()) {
            est_component = step_cards.back();
        }
        auto real = execute_exact_component_join(component, comp_joins,
                                                 ctx.table_ptrs, ctx.rows_per_table);

        print_sequential_plan(order, step_cards, "Orden elegido:");
        std::cout << "  Costo acumulado estimado: " << std::fixed << std::setprecision(2)
                  << cost << "\n";
        std::cout << "  Cardinalidad estimada componente: "
                  << std::fixed << std::setprecision(2) << est_component << "\n";
        std::cout << "  Cardinalidad real componente: " << real << "\n\n";

        total_estimate *= (est_component > 0.0 ? est_component : 0.0);
        total_real *= real;
    }

    std::cout << "================= Totales (producto componentes) ================\n";
    std::cout << "  Cardinalidad estimada total : "
              << std::fixed << std::setprecision(2) << total_estimate << "\n";
    std::cout << "  Cardinalidad real total     : " << total_real << "\n";
    std::cout << "=================================================================\n\n";
}

std::unique_ptr<JoinTreeNode> run_query_plan_compass(
    const std::unordered_map<std::string, Table> &tables,
    const JoinQuery &q) {
    QueryContext ctx = prepare_context(tables, q);

    std::cout << "================= Planner estilo COMPASS =================\n";
    double global_est = 1.0;
    std::uint64_t global_real = 1;
    double global_cost = 0.0;
    std::vector<std::unique_ptr<JoinTreeNode>> component_trees;

    for (const auto &component : ctx.components) {
        std::cout << "Componente con tablas: ";
        for (size_t i = 0; i < component.size(); ++i) {
            if (i) std::cout << ", ";
            std::cout << component[i];
        }
        std::cout << "\n";

        auto comp_joins = joins_for_tables(q.joins, component);
        int n = static_cast<int>(component.size());
        int total_mask = 1 << n;
        std::unordered_map<std::string, int> index_map;
        for (int i = 0; i < n; ++i) index_map[component[i]] = i;

        std::vector<DPEntry> dp(total_mask);
        for (int i = 0; i < n; ++i) {
            int mask = 1 << i;
            dp[mask].valid = true;
            dp[mask].est_card = ctx.rows_per_table[component[i]].size();
            dp[mask].cost = dp[mask].est_card;
            dp[mask].tree = std::make_unique<JoinTreeNode>();
            dp[mask].tree->is_leaf = true;
            dp[mask].tree->leaf_table = component[i];
            dp[mask].tree->tables = {component[i]};
            dp[mask].tree->est_card = dp[mask].est_card;
            dp[mask].tree->cost = dp[mask].cost;
        }

        for (int mask = 1; mask < total_mask; ++mask) {
            if ((mask & (mask - 1)) == 0) continue;  // singleton

            DPEntry best;
            for (int left = (mask - 1) & mask; left; left = (left - 1) & mask) {
                int right = mask ^ left;
                if (!dp[left].valid || !dp[right].valid) continue;
                double join_est = 0.0;
                if (!estimate_join_between_masks(left, right, index_map,
                                                 comp_joins, ctx.sketchCache, join_est)) {
                    continue;
                }
                double norm_est = normalize_join_estimate(dp[left].est_card,
                                                          dp[right].est_card,
                                                          join_est);
                double join_cost = compute_join_cost(dp[left].est_card,
                                                     dp[right].est_card,
                                                     norm_est);
                double cand_cost = dp[left].cost + dp[right].cost + join_cost;
                auto subset_tables = subset_to_tables(mask, component);
                auto subset_joins = joins_for_tables(comp_joins, subset_tables);
                double subset_est = estimate_component_with_sketches(subset_tables,
                                                                     subset_joins,
                                                                     ctx.sketchCache);
                if (subset_est == 0.0) subset_est = norm_est;

                if (!best.valid || cand_cost < best.cost) {
                    best.valid = true;
                    best.cost = cand_cost;
                    best.est_card = subset_est;
                    best.tree = std::make_unique<JoinTreeNode>();
                    best.tree->tables = subset_tables;
                    best.tree->est_card = subset_est;
                    best.tree->cost = cand_cost;
                    best.tree->left = clone_tree(dp[left].tree.get());
                    best.tree->right = clone_tree(dp[right].tree.get());
                }
            }
            if (best.valid) {
                dp[mask].valid = true;
                dp[mask].cost = best.cost;
                dp[mask].est_card = best.est_card;
                dp[mask].tree = std::move(best.tree);
            }
        }

        int full_mask = total_mask - 1;
        if (!dp[full_mask].valid) {
            std::cout << "  No se encontró plan válido para este componente.\n\n";
            continue;
        }

        auto real = execute_exact_component_join(component, comp_joins,
                                                 ctx.table_ptrs, ctx.rows_per_table);
        std::cout << "  Mejor costo estimado: " << std::fixed << std::setprecision(2)
                  << dp[full_mask].cost << "\n";
        std::cout << "  Cardinalidad estimada: "
                  << std::fixed << std::setprecision(2) << dp[full_mask].est_card << "\n";
        std::cout << "  Cardinalidad real: " << real << "\n";
        print_join_tree(dp[full_mask].tree.get());
        std::cout << "\n";

        component_trees.push_back(clone_tree(dp[full_mask].tree.get()));
        global_cost += dp[full_mask].cost;
        global_est *= (dp[full_mask].est_card > 0.0 ? dp[full_mask].est_card : 0.0);
        global_real *= real;
    }

    std::cout << "================= Resumen global =================\n";
    std::cout << "  Costo acumulado: " << std::fixed << std::setprecision(2)
              << global_cost << "\n";
    std::cout << "  Cardinalidad estimada total: "
              << std::fixed << std::setprecision(2) << global_est << "\n";
    std::cout << "  Cardinalidad real total: " << global_real << "\n";
    std::cout << "==================================================\n\n";

    if (component_trees.empty()) return nullptr;
    if (component_trees.size() == 1) {
        return clone_tree(component_trees.front().get());
    }

    auto root = std::make_unique<JoinTreeNode>();
    root->tables = q.tables;
    root->is_leaf = false;
    root->est_card = global_est;
    root->cost = global_cost;

    std::unique_ptr<JoinTreeNode> current = std::move(component_trees.front());
    for (size_t i = 1; i < component_trees.size(); ++i) {
        auto node = std::make_unique<JoinTreeNode>();
        node->tables = q.tables;
        node->is_leaf = false;
        node->left = std::move(current);
        node->right = clone_tree(component_trees[i].get());
        node->est_card = global_est;
        node->cost = global_cost;
        current = std::move(node);
    }
    root->left = clone_tree(current.get());
    return root;
}
