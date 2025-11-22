// Optimizer.cpp
#include "Optimizer.h"
#include "Compass.h"

#include <vector>
#include <iomanip>
#include <numeric>
#include <iostream>
#include <unordered_map>
#include <unordered_set>
#include <stdexcept>
#include <queue>
#include <string>
#include <algorithm>
#include <limits>


// -----------------------------------------------------------------------------
// Helpers básicos: filtrado, sketches, etc.
// -----------------------------------------------------------------------------

static std::vector<int> filter_rows(const Table &t,
                                    const std::vector<Predicate> &preds)
{
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

    for (auto &p : preds) {
        int idx = t.getColumnIndex(p.column);
        if (idx < 0) {
            std::cerr << "Advertencia: columna " << p.column
                      << " no existe en tabla " << t.name
                      << ". Predicado ignorado.\n";
            continue;
        }

        Test tt;
        tt.col_idx = idx;
        tt.op = p.op;
        tt.val = p.value;

        // detectar si es número
        char* endptr = nullptr;
        double v = std::strtod(p.value.c_str(), &endptr);
        if (endptr != p.value.c_str() && *endptr == '\0') {
            tt.is_number = true;
            tt.num_val = v;
        } else {
            tt.is_number = false;
        }

        tests.push_back(tt);
    }

    for (int r = 0; r < (int)t.data.size(); ++r) {
        bool ok = true;

        for (auto &tt : tests) {
            const std::string &cell = t.data[r][tt.col_idx];

            if (tt.is_number) {
                // comparar numéricamente
                double cv = 0;
                char* e2 = nullptr;
                double tmp = std::strtod(cell.c_str(), &e2);
                if (e2 != cell.c_str() && *e2 == '\0') cv = tmp;
                else { ok = false; break; } // valor no numérico → falso

                if      (tt.op == "="  && !(cv == tt.num_val)) ok = false;
                else if (tt.op == "!=" && !(cv != tt.num_val)) ok = false;
                else if (tt.op == "<>" && !(cv != tt.num_val)) ok = false;
                else if (tt.op == "<"  && !(cv <  tt.num_val)) ok = false;
                else if (tt.op == ">"  && !(cv >  tt.num_val)) ok = false;
                else if (tt.op == "<=" && !(cv <= tt.num_val)) ok = false;
                else if (tt.op == ">=" && !(cv >= tt.num_val)) ok = false;
            }
            else {
                // comparar como string
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

// Key para cache de sketches: "tabla::col"
static inline std::string sk_key(const std::string &table, const std::string &col) {
    return table + "::" + col;
}

// Construye cache de sketches para todos los (table,column) usados en joins
static void build_sketch_cache(
    const std::unordered_map<std::string, Table> &tables,
    const std::vector<std::string> &table_order,
    const std::unordered_map<std::string, std::vector<int>> &rows_per_table,
    const JoinQuery &q,
    std::unordered_map<std::string, CMSketch> &cache)
{
    for (const auto &jc : q.joins) {
        // left
        {
            std::string key = sk_key(jc.leftTable, jc.leftKey);
            if (!cache.count(key)) {
                const Table &t = tables.at(jc.leftTable);
                const auto &rows = rows_per_table.at(jc.leftTable);
                cache.emplace(key, build_sketch_for_table(t, rows, jc.leftKey));
            }
        }
        // right
        {
            std::string key = sk_key(jc.rightTable, jc.rightKey);
            if (!cache.count(key)) {
                const Table &t = tables.at(jc.rightTable);
                const auto &rows = rows_per_table.at(jc.rightTable);
                cache.emplace(key, build_sketch_for_table(t, rows, jc.rightKey));
            }
        }
    }
}

// Grafo de joins: componentes conexas
static std::vector<std::vector<std::string>> compute_join_components(const JoinQuery &q) {
    std::unordered_map<std::string, std::vector<std::string>> adj;
    std::unordered_set<std::string> nodes;
    for (const auto &t : q.tables) nodes.insert(t);
    for (const auto &jc : q.joins) {
        adj[jc.leftTable].push_back(jc.rightTable);
        adj[jc.rightTable].push_back(jc.leftTable);
        nodes.insert(jc.leftTable);
        nodes.insert(jc.rightTable);
    }

    std::vector<std::vector<std::string>> comps;
    std::unordered_set<std::string> seen;
    for (auto &n : nodes) {
        if (seen.count(n)) continue;
        std::vector<std::string> comp;
        std::queue<std::string> qn;
        qn.push(n);
        seen.insert(n);
        while (!qn.empty()) {
            std::string u = qn.front(); qn.pop();
            comp.push_back(u);
            for (auto &v : adj[u]) {
                if (!seen.count(v)) {
                    seen.insert(v);
                    qn.push(v);
                }
            }
        }
        comps.push_back(comp);
    }
    return comps;
}

// Joins cuyo par de tablas esté completamente dentro de 'subset'
static std::vector<JoinCondition> joins_for_tables(
    const std::vector<JoinCondition> &all_joins,
    const std::vector<std::string> &subset)
{
    std::unordered_set<std::string> s(subset.begin(), subset.end());
    std::vector<JoinCondition> res;
    for (const auto &jc : all_joins) {
        if (s.count(jc.leftTable) && s.count(jc.rightTable)) {
            res.push_back(jc);
        }
    }
    return res;
}

// Estima cardinalidad multi-way para un conjunto de tablas usando sketches
static double estimate_component_with_sketches(
    const std::vector<std::string> &component_tables,
    const std::vector<JoinCondition> &component_joins,
    const std::unordered_map<std::string, CMSketch> &sketchCache)
{
    if (component_tables.empty()) return 0.0;
    if (component_joins.empty()) {
        // Sin joins: no tiene mucho sentido (producto de tamaños), pero para no explotar devolvemos 0.
        return 0.0;
    }

    std::unordered_set<std::string> in_set;
    std::vector<CMSketch> current_sketches;

    const JoinCondition &start = component_joins.front();
    in_set.insert(start.leftTable);
    in_set.insert(start.rightTable);
    current_sketches.push_back(sketchCache.at(sk_key(start.leftTable, start.leftKey)));
    current_sketches.push_back(sketchCache.at(sk_key(start.rightTable, start.rightKey)));
    double est = estimate_join_cardinality_multi(current_sketches);

    bool progress = true;
    std::unordered_set<size_t> used_join_idx;
    used_join_idx.insert(0);

    while (progress) {
        progress = false;
        for (size_t i = 0; i < component_joins.size(); ++i) {
            if (used_join_idx.count(i)) continue;
            const JoinCondition &jc = component_joins[i];

            bool left_in  = in_set.count(jc.leftTable);
            bool right_in = in_set.count(jc.rightTable);

            if (left_in ^ right_in) {
                // mete una tabla nueva
                std::string newTable = left_in ? jc.rightTable : jc.leftTable;
                std::string newCol   = left_in ? jc.rightKey   : jc.leftKey;
                std::string key      = sk_key(newTable, newCol);

                auto it = sketchCache.find(key);
                if (it == sketchCache.end()) {
                    continue;
                }
                current_sketches.push_back(it->second);
                in_set.insert(newTable);
                est = estimate_join_cardinality_multi(current_sketches);
                used_join_idx.insert(i);
                progress = true;
            } else if (left_in && right_in) {
                // join interno, ya cubierto por los sketches existentes
                used_join_idx.insert(i);
                progress = true;
            }
        }
    }

    return est;
}

// Join exacto "component-wise" (solo cardinalidad final, no depende del orden)
static std::uint64_t execute_exact_component_join(
    const std::vector<std::string> &component_tables,
    const std::vector<JoinCondition> &component_joins,
    const std::unordered_map<std::string,const Table*> &table_ptrs,
    const std::unordered_map<std::string,std::vector<int>> &rows_per_table)
{
    if (component_joins.empty()) {
        // Sin joins en el componente: producto de filas
        std::uint64_t prod = 1;
        for (auto &t : component_tables) {
            prod *= static_cast<std::uint64_t>(rows_per_table.at(t).size());
        }
        return prod;
    }

    JoinCondition start = component_joins.front();

    std::vector<std::pair<std::unordered_map<std::string,std::string>, std::uint64_t>> results;

    {
        const Table *L = table_ptrs.at(start.leftTable);
        const Table *R = table_ptrs.at(start.rightTable);
        const auto &Lrows = rows_per_table.at(start.leftTable);
        const auto &Rrows = rows_per_table.at(start.rightTable);
        int Li = L->getColumnIndex(start.leftKey);
        int Ri = R->getColumnIndex(start.rightKey);
        if (Li < 0 || Ri < 0) return 0;

        std::unordered_map<std::string, std::vector<int>> rmap;
        rmap.reserve(Rrows.size()*2+1);
        for (int r : Rrows) {
            rmap[R->data[r][Ri]].push_back(r);
        }
        for (int l : Lrows) {
            const std::string &lv = L->data[l][Li];
            auto it = rmap.find(lv);
            if (it == rmap.end()) continue;
            for (int rr : it->second) {
                (void)rr;
                std::unordered_map<std::string,std::string> tup;
                tup[start.leftTable + "." + start.leftKey]   = lv;
                tup[start.rightTable + "." + start.rightKey] = lv;
                results.emplace_back(std::move(tup), 1u);
            }
        }
    }

    if (results.empty()) return 0;

    std::unordered_set<size_t> used;
    used.insert(0);
    bool progress = true;
    while (progress) {
        progress = false;
        for (size_t i = 0; i < component_joins.size(); ++i) {
            if (used.count(i)) continue;
            const JoinCondition &jc = component_joins[i];

            bool left_present  = false;
            bool right_present = false;
            auto &sample_tup = results.front().first;
            if (sample_tup.find(jc.leftTable + "." + jc.leftKey) != sample_tup.end())  left_present  = true;
            if (sample_tup.find(jc.rightTable + "." + jc.rightKey) != sample_tup.end()) right_present = true;

            if (!left_present && !right_present) {
                continue;
            }

            const std::string presentTable = left_present ? jc.leftTable  : jc.rightTable;
            const std::string presentCol   = left_present ? jc.leftKey    : jc.rightKey;
            const std::string otherTable   = left_present ? jc.rightTable : jc.leftTable;
            const std::string otherCol     = left_present ? jc.rightKey   : jc.leftKey;

            const Table *Ot = table_ptrs.at(otherTable);
            const auto &OtherRows = rows_per_table.at(otherTable);
            int Oi = Ot->getColumnIndex(otherCol);
            if (Oi < 0) {
                used.insert(i);
                continue;
            }

            std::unordered_map<std::string, std::vector<int>> other_map;
            other_map.reserve(OtherRows.size()*2+1);
            for (int r : OtherRows) {
                other_map[Ot->data[r][Oi]].push_back(r);
            }

            std::vector<std::pair<std::unordered_map<std::string,std::string>, std::uint64_t>> new_results;
            new_results.reserve(results.size()*2 + 10);
            for (auto &entry : results) {
                auto tup = entry.first;
                std::uint64_t mult = entry.second;
                auto itv = tup.find(presentTable + "." + presentCol);
                if (itv == tup.end()) {
                    continue;
                }
                const std::string &val = itv->second;
                auto itrows = other_map.find(val);
                if (itrows == other_map.end()) {
                    continue;
                }
                for (int orow : itrows->second) {
                    (void)orow; // no la usamos explícitamente
                    auto ntup = tup;
                    ntup[otherTable + "." + otherCol] = val;
                    new_results.emplace_back(std::move(ntup), mult);
                }
            }
            results.swap(new_results);
            used.insert(i);
            progress = true;

            if (results.empty()) return 0;
        }
    }

    std::uint64_t total = 0;
    for (auto &e : results) {
        total += e.second;
    }
    return total;
}

// Pretty-print del plan de joins (secuencial, opción 2)
static void print_sequential_plan(const std::vector<std::string> &order,
                                  const std::vector<double> &step_cards,
                                  const std::string &title_prefix) {
    std::cout << title_prefix << " join order: ";
    for (size_t i = 0; i < order.size(); ++i) {
        if (i) std::cout << " -> ";
        std::cout << order[i];
    }
    std::cout << "\n";

    for (size_t i = 0; i + 1 < order.size(); ++i) {
        std::cout << "    Step " << (i+1) << ": [";
        for (size_t j = 0; j <= i; ++j) {
            if (j) std::cout << ",";
            std::cout << order[j];
        }
        std::cout << "] ⋈ [" << order[i+1] << "]  => est_card ~ "
                  << std::fixed << std::setprecision(2) << step_cards[i] << "\n";
    }
}

// -----------------------------------------------------------------------------
// OPCIÓN 2: Mejora incremental (left-deep sobre permutaciones)
// -----------------------------------------------------------------------------

static double evaluate_left_deep_order(
    const std::vector<std::string> &order,
    const std::vector<JoinCondition> &component_joins,
    const std::unordered_map<std::string, CMSketch> &sketchCache,
    std::vector<double> &step_cards)
{
    // Vamos agregando tablas de izquierda a derecha
    double total_cost = 0.0;
    step_cards.clear();

    std::vector<std::string> prefix;
    prefix.reserve(order.size());

    prefix.push_back(order[0]);
    // El costo para una tabla sola lo consideramos su cardinalidad estimada.
    // Como no tenemos sketches por tabla suelta (solo por columna de join),
    // lo dejamos en 0 (o podrías usar |rows| si quisieras incluir el scan).
    // Para simplificar, contamos costo solo desde el primer join.
    for (size_t i = 1; i < order.size(); ++i) {
        prefix.push_back(order[i]);
        auto subset_joins = joins_for_tables(component_joins, prefix);
        double est = estimate_component_with_sketches(prefix, subset_joins, sketchCache);
        step_cards.push_back(est);
        total_cost += est;
    }
    return total_cost;
}

void run_query_plan(const std::unordered_map<std::string, Table> &tables,
                    const JoinQuery &q) {
    // --------- 1. Verificar tablas ----------
    for (const auto &tbl : q.tables) {
        if (!tables.count(tbl)) {
            throw std::runtime_error("Tabla " + tbl + " no encontrada entre los CSV");
        }
    }

    // --------- 2. Predicados por tabla y filtrado ----------
    std::unordered_map<std::string, std::vector<Predicate>> preds_map;
    for (const auto &p : q.predicates) {
        preds_map[p.table].push_back(p);
    }

    std::vector<std::string> table_order = q.tables;
    std::unordered_map<std::string,const Table*> table_ptrs;
    std::unordered_map<std::string,std::vector<int>> rows_per_table;
    for (const auto &tbl_name : table_order) {
        const Table &t = tables.at(tbl_name);
        auto preds_for_table = preds_map.count(tbl_name)
            ? preds_map.at(tbl_name)
            : std::vector<Predicate>{};
        auto rows = filter_rows(t, preds_for_table);
        table_ptrs[tbl_name] = &t;
        rows_per_table[tbl_name] = std::move(rows);
    }

    // --------- 3. Construir sketches ----------
    std::unordered_map<std::string, CMSketch> sketchCache;
    build_sketch_cache(tables, table_order, rows_per_table, q, sketchCache);

    // --------- 4. Componentes del grafo de joins ----------
    auto components = compute_join_components(q);

    double total_estimate = 1.0;
    std::uint64_t total_real = 1;

    std::cout << "================= COMPASS-lite (Opción 2) =================\n";
    std::cout << "Query: FROM " << q.tables.front();
    for (size_t i = 1; i < q.tables.size(); ++i) {
        std::cout << " JOIN " << q.tables[i];
    }
    std::cout << "\n\n";

    std::cout << "Tablas (tamaño CSV original):\n";
    for (auto &name : q.tables) {
        const Table &t = tables.at(name);
        std::cout << "  " << name << " : " << t.data.size() << " filas\n";
    }
    std::cout << "\n";

    std::cout << "Predicados WHERE:\n";
    if (q.predicates.empty()) std::cout << "  (ninguno)\n";
    else {
        for (auto &p : q.predicates) {
            std::cout << "  " << p.table << "." << p.column
                      << " = '" << p.value << "'\n";
        }
    }
    std::cout << "\n";

    std::cout << "Join conditions:\n";
    if (q.joins.empty()) std::cout << "  (ninguno)\n";
    else {
        for (auto &jc : q.joins) {
            std::cout << "  " << jc.leftTable << "." << jc.leftKey << " = "
                      << jc.rightTable << "." << jc.rightKey << "\n";
        }
    }
    std::cout << "\n";

    std::cout << "Plan básico por tabla (scan + filtro):\n";
    for (auto &tbl_name : table_order) {
        std::cout << "  Scan " << tbl_name
                  << " (tras filtros: " << rows_per_table[tbl_name].size()
                  << " filas)\n";
    }
    std::cout << "\n";

    // --------- 5. Por cada componente, buscar mejor orden left-deep ----------
    int comp_id = 0;
    for (const auto &comp : components) {
        ++comp_id;
        // extraer joins de este componente
        auto comp_joins = joins_for_tables(q.joins, comp);

        std::cout << "---- Componente " << comp_id << " ----\n";
        std::cout << "Tablas en el componente: ";
        for (size_t i = 0; i < comp.size(); ++i) {
            if (i) std::cout << ", ";
            std::cout << comp[i];
        }
        std::cout << "\n";

        if (comp.size() <= 1 || comp_joins.empty()) {
            // trivial
            if (comp.size() == 1) {
                const std::string &tname = comp.front();
                double est = static_cast<double>(rows_per_table[tname].size());
                total_estimate *= est;
                total_real *= static_cast<std::uint64_t>(rows_per_table[tname].size());
                std::cout << "  (Sin joins; cardinalidad ~ " << est << ")\n\n";
            } else {
                std::cout << "  (Sin joins relevantes)\n\n";
            }
            continue;
        }

        // Generar todas las permutaciones de las tablas del componente
        std::vector<std::string> best_order = comp;
        std::sort(best_order.begin(), best_order.end());
        std::vector<std::string> perm = best_order;
        double best_cost = std::numeric_limits<double>::infinity();
        std::vector<double> best_step_cards;

        bool first_perm = true;
        do {
            std::vector<double> step_cards;
            double cost = evaluate_left_deep_order(perm, comp_joins, sketchCache, step_cards);
            if (first_perm || cost < best_cost) {
                first_perm = false;
                best_cost = cost;
                best_order = perm;
                best_step_cards = step_cards;
            }
        } while (std::next_permutation(perm.begin(), perm.end()));

        // Estimación final para el componente completo
        double est_comp =
            estimate_component_with_sketches(best_order,
                                             joins_for_tables(comp_joins, best_order),
                                             sketchCache);
        std::uint64_t real_comp =
            execute_exact_component_join(comp, comp_joins, table_ptrs, rows_per_table);

        std::cout << "Mejor plan (Opción 2, left-deep sobre permutaciones):\n";
        print_sequential_plan(best_order, best_step_cards, "  ");
        std::cout << "  Costo estimado (suma de tamaños intermedios) : "
                  << best_cost << "\n";
        std::cout << "  Cardinalidad estimada final del componente  : "
                  << std::fixed << std::setprecision(2) << est_comp << "\n";
        std::cout << "  Cardinalidad REAL final del componente      : "
                  << real_comp << "\n\n";

        total_estimate *= (est_comp > 0.0 ? est_comp : 0.0);
        total_real     *= real_comp;
    }

    std::cout << "================= Resultados globales =================\n";
    std::cout << "  Cardinalidad REAL total (producto de componentes)      : "
              << total_real << "\n";
    std::cout << "  Cardinalidad ESTIMADA total (producto de componentes)  : "
              << std::fixed << std::setprecision(2) << total_estimate << "\n";
    std::cout << "=======================================================\n";
}

// -----------------------------------------------------------------------------
// OPCIÓN 1: Planner tipo COMPASS (bushy + left + right) con costo tipo paper
// -----------------------------------------------------------------------------

struct JoinTreeNode {
    std::vector<std::string> tables;          // tablas en este subárbol
    double est_card = 0.0;                    // cardinalidad estimada del subárbol
    double cost = 0.0;                        // costo acumulado del subárbol (incluye hijos)
    bool is_leaf = false;
    std::string leaf_table;                   // válido si es hoja
    std::unique_ptr<JoinTreeNode> left;
    std::unique_ptr<JoinTreeNode> right;
};

struct DPEntry {
    bool valid = false;
    double cost = 0.0;
    double est_card = 0.0;
    std::unique_ptr<JoinTreeNode> tree;
};

// Convierte bitmask -> vector<string> con nombres de tablas
static std::vector<std::string> subset_to_tables(
    int mask,
    const std::vector<std::string> &all_tables)
{
    std::vector<std::string> out;
    for (size_t i = 0; i < all_tables.size(); ++i) {
        if (mask & (1 << i)) out.push_back(all_tables[i]);
    }
    return out;
}

// Impresión visual del árbol de joins
static void print_join_tree(const JoinTreeNode *node,
                            const std::string &indent = "",
                            bool is_left = true)
{
    if (!node) return;
    std::string branch = is_left ? "└─L " : "└─R ";

    if (node->is_leaf) {
        std::cout << indent << branch
                  << "Scan " << node->leaf_table
                  << "   (subtree est_card = " << node->est_card << ")\n";
    } else {
        std::cout << indent << branch
                  << "Join(";
        for (size_t i = 0; i < node->tables.size(); ++i) {
            if (i) std::cout << ",";
            std::cout << node->tables[i];
        }
        std::cout << ")  est_card = " << node->est_card
                  << ", cost_subtree = " << node->cost << "\n";

        std::string next_indent = indent + "   ";
        print_join_tree(node->left.get(),  next_indent, true);
        print_join_tree(node->right.get(), next_indent, false);
    }
}

// -----------------------------------------------------------------------------
// Deep copy de árboles para poder copiar JoinTreeNode sin copiar unique_ptr
// -----------------------------------------------------------------------------
static std::unique_ptr<JoinTreeNode> clone_tree(const JoinTreeNode *src) {
    if (!src) return nullptr;

    auto node = std::make_unique<JoinTreeNode>();
    node->tables = src->tables;
    node->est_card = src->est_card;
    node->cost = src->cost;
    node->is_leaf = src->is_leaf;
    node->leaf_table = src->leaf_table;

    if (!src->is_leaf) {
        node->left  = clone_tree(src->left.get());
        node->right = clone_tree(src->right.get());
    }

    return node;
}


void run_query_plan_compass(const std::unordered_map<std::string, Table> &tables,
                            const JoinQuery &q) {
    // 1. Verificar tablas
    for (const auto &tbl : q.tables) {
        if (!tables.count(tbl)) {
            throw std::runtime_error("Tabla " + tbl + " no encontrada");
        }
    }

    // 2. Predicados y filas filtradas
    std::unordered_map<std::string, std::vector<Predicate>> preds_map;
    for (const auto &p : q.predicates) {
        preds_map[p.table].push_back(p);
    }

    std::vector<std::string> table_order = q.tables;
    std::unordered_map<std::string,const Table*> table_ptrs;
    std::unordered_map<std::string,std::vector<int>> rows_per_table;
    for (const auto &tbl_name : table_order) {
        const Table &t = tables.at(tbl_name);
        auto preds_for_table = preds_map.count(tbl_name)
            ? preds_map.at(tbl_name)
            : std::vector<Predicate>{};
        auto rows = filter_rows(t, preds_for_table);
        table_ptrs[tbl_name] = &t;
        rows_per_table[tbl_name] = std::move(rows);
    }

    // 3. Sketch cache
    std::unordered_map<std::string, CMSketch> sketchCache;
    build_sketch_cache(tables, table_order, rows_per_table, q, sketchCache);

    // 4. Componentes de join
    auto components = compute_join_components(q);

    double total_estimate = 1.0;
    std::uint64_t total_real = 1;

    std::cout << "================= COMPASS-style Planner (Opción 1) =================\n";
    std::cout << "Query: FROM " << q.tables.front();
    for (size_t i = 1; i < q.tables.size(); ++i) {
        std::cout << " JOIN " << q.tables[i];
    }
    std::cout << "\n\n";

    std::cout << "Tablas (tamaño CSV original):\n";
    for (auto &name : q.tables) {
        const Table &t = tables.at(name);
        std::cout << "  " << name << " : " << t.data.size() << " filas\n";
    }
    std::cout << "\n";

    std::cout << "Predicados WHERE:\n";
    if (q.predicates.empty()) std::cout << "  (ninguno)\n";
    else {
        for (auto &p : q.predicates) {
            std::cout << "  " << p.table << "." << p.column
                      << " = '" << p.value << "'\n";
        }
    }
    std::cout << "\n";

    std::cout << "Join conditions:\n";
    if (q.joins.empty()) std::cout << "  (ninguno)\n";
    else {
        for (auto &jc : q.joins) {
            std::cout << "  " << jc.leftTable << "." << jc.leftKey << " = "
                      << jc.rightTable << "." << jc.rightKey << "\n";
        }
    }
    std::cout << "\n";

    std::cout << "Plan básico por tabla (scan + filtro):\n";
    for (auto &tbl_name : table_order) {
        std::cout << "  Scan " << tbl_name
                  << " (tras filtros: " << rows_per_table[tbl_name].size()
                  << " filas)\n";
    }
    std::cout << "\n";

    int comp_id = 0;
    for (const auto &comp : components) {
        ++comp_id;
        auto comp_joins = joins_for_tables(q.joins, comp);

        std::cout << "---- Componente " << comp_id << " (COMPASS DP bushy/left/right) ----\n";
        std::cout << "Tablas en el componente: ";
        for (size_t i = 0; i < comp.size(); ++i) {
            if (i) std::cout << ", ";
            std::cout << comp[i];
        }
        std::cout << "\n";

        if (comp.size() <= 1 || comp_joins.empty()) {
            if (comp.size() == 1) {
                const std::string &tname = comp.front();
                double est = static_cast<double>(rows_per_table[tname].size());
                total_estimate *= est;
                total_real *= static_cast<std::uint64_t>(rows_per_table[tname].size());
                std::cout << "  (Sin joins; cardinalidad ~ " << est << ")\n\n";
            } else {
                std::cout << "  (Sin joins relevantes)\n\n";
            }
            continue;
        }

        const int n = static_cast<int>(comp.size());
        const int full_mask = (1 << n) - 1;
        std::vector<DPEntry> dp(1 << n);

        // Base: subsets de tamaño 1
        for (int i = 0; i < n; ++i) {
            int mask = (1 << i);
            DPEntry &e = dp[mask];
            e.valid = true;
            e.tree.reset(new JoinTreeNode());
            e.tree->is_leaf = true;
            e.tree->leaf_table = comp[i];
            e.tree->tables = { comp[i] };
            // Estimación: tamaño tras filtro
            double est = static_cast<double>(rows_per_table[comp[i]].size());
            e.est_card = est;
            e.tree->est_card = est;
            // En el paper, el costo incluye también el scan de cada tabla;
            // aquí lo consideramos como ese mismo est.
            e.cost = est;
            e.tree->cost = est;
        }

        // DP sobre subsets
        for (int mask = 1; mask <= full_mask; ++mask) {
            // ya llenamos singles
            if (__builtin_popcount(mask) == 1) continue;

            DPEntry best;
            best.valid = false;

            // particionar mask en left y right no vacíos
            for (int left_mask = (mask - 1) & mask; left_mask > 0; left_mask = (left_mask - 1) & mask) {
                int right_mask = mask ^ left_mask;
                if (right_mask == 0) continue;

                DPEntry &L = dp[left_mask];
                DPEntry &R = dp[right_mask];
                if (!L.valid || !R.valid) continue;

                // tablas en este subárbol
                auto tables_here = subset_to_tables(mask, comp);
                auto joins_here  = joins_for_tables(comp_joins, tables_here);
                double est_here  = estimate_component_with_sketches(tables_here, joins_here, sketchCache);

                double cost_here = L.cost + R.cost + est_here; // costo tipo paper

                if (!best.valid || cost_here < best.cost) {
                    best.valid = true;
                    best.cost = cost_here;
                    best.est_card = est_here;
                    best.tree.reset(new JoinTreeNode());
                    best.tree->is_leaf = false;
                    best.tree->leaf_table.clear();
                    best.tree->tables = tables_here;
                    best.tree->est_card = est_here;
                    best.tree->cost = cost_here;
                    best.tree->left  = clone_tree(L.tree.get());
                    best.tree->right = clone_tree(R.tree.get());
                }
            }

            if (best.valid) {
                dp[mask] = std::move(best);
            }
        }

        const DPEntry &best_full = dp[full_mask];
        if (!best_full.valid) {
            std::cout << "  [WARNING] No se pudo construir un plan DP válido.\n\n";
            continue;
        }

        double est_comp = best_full.est_card;
        std::uint64_t real_comp =
            execute_exact_component_join(comp, comp_joins, table_ptrs, rows_per_table);

        std::cout << "Mejor plan COMPASS (bushy/left/right considerados):\n";
        std::cout << "  Costo total estimado (suma de tamaños intermedios): "
                  << best_full.cost << "\n";
        std::cout << "  Cardinalidad estimada del resultado final        : "
                  << est_comp << "\n";
        std::cout << "  Cardinalidad REAL del resultado final           : "
                  << real_comp << "\n";
        std::cout << "\n  Árbol de ejecución (filtrado previo ya aplicado por tabla):\n";
        print_join_tree(best_full.tree.get(), "  ", true);
        std::cout << "\n";

        total_estimate *= (est_comp > 0.0 ? est_comp : 0.0);
        total_real     *= real_comp;
    }

    std::cout << "================= Resultados globales =================\n";
    std::cout << "  Cardinalidad REAL total (producto de componentes)      : "
              << total_real << "\n";
    std::cout << "  Cardinalidad ESTIMADA total (producto de componentes)  : "
              << std::fixed << std::setprecision(2) << total_estimate << "\n";
    std::cout << "=======================================================\n";
}
