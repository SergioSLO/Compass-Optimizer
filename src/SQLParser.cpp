// SQLParser.cpp
#include "SQLParser.h"

#include <fstream>
#include <sstream>
#include <stdexcept>
#include <algorithm>
#include <cctype>
#include <iostream>
#include <unordered_map>

static std::string trim(const std::string &s) {
    size_t start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    size_t end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

static std::string to_upper_copy(std::string s) {
    for (char &c : s) c = static_cast<char>(std::toupper((unsigned char)c));
    return s;
}

static std::vector<std::string> split(const std::string &s, char delim) {
    std::vector<std::string> res;
    std::stringstream ss(s);
    std::string cur;
    while (std::getline(ss, cur, delim))
        res.push_back(cur);
    return res;
}

static std::string strip_quotes(const std::string &s) {
    std::string t = trim(s);
    if (t.size() >= 2) {
        char c1 = t.front(), c2 = t.back();
        if ((c1 == '\'' && c2 == '\'') || (c1 == '"' && c2 == '"'))
            return t.substr(1, t.size() - 2);
    }
    return t;
}

// Parse "tbl.col" -> ("tbl","col")
static std::pair<std::string,std::string> parse_qualified(const std::string &s) {
    std::string tmp = trim(s);
    size_t dot = tmp.find('.');
    if (dot == std::string::npos)
        return {"", tmp};
    return { trim(tmp.substr(0,dot)), trim(tmp.substr(dot+1)) };
}

// Detecta si el token tiene alias ("table AS t", "table t")
static std::pair<std::string,std::string> parse_table_alias(const std::string &s) {
    // Normaliza espacios
    std::vector<std::string> parts = split(trim(s), ' ');
    if (parts.empty())
        return {"",""};

    if (parts.size() == 1) {
        // Solo tabla
        return { parts[0], "" };
    }
    if (parts.size() == 2) {
        // "table alias"
        if (to_upper_copy(parts[0]) == "AS")
            throw std::runtime_error("AS sin nombre de tabla");

        if (to_upper_copy(parts[1]) == "AS")
            throw std::runtime_error("Falta alias tras AS");

        return { parts[0], parts[1] };
    }
    if (parts.size() == 3) {
        // "table AS alias"
        if (to_upper_copy(parts[1]) != "AS")
            throw std::runtime_error("Sintaxis desconocida en alias: " + s);
        return { parts[0], parts[2] };
    }

    throw std::runtime_error("Alias demasiado complejo: " + s);
}

JoinQuery parse_sql_file(const std::string &path) {
    std::ifstream in(path);
    if (!in) throw std::runtime_error("No se pudo abrir SQL: " + path);

    std::string sql, line;
    while (std::getline(in, line))
        sql += line + " ";

    sql = trim(sql);
    if (!sql.empty() && sql.back() == ';')
        sql.pop_back();

    // Make an uppercase copy for token positions
    std::string upper = to_upper_copy(sql);

<<<<<<< Updated upstream
    // Find FROM pos
    size_t pos_from = upper.find(" FROM ");
    if (pos_from == std::string::npos) {
        throw std::runtime_error("SQL no soportado: falta FROM");
    }

    JoinQuery q;

    // Extract left (FROM) table (allow optional alias, take first token)
    {
        size_t start = pos_from + 6; // after " FROM "
        // find next " JOIN " or " WHERE " or end
        size_t next_join = upper.find(" JOIN ", start);
        size_t next_where = upper.find(" WHERE ", start);
        size_t end = std::min(next_join == std::string::npos ? upper.size() : next_join,
                              next_where == std::string::npos ? upper.size() : next_where);
        std::string t = trim(sql.substr(start, end - start));
        q.leftTable = trim(split(t, ' ')[0]);
        q.tables.push_back(q.leftTable);
    }

    // Now iterate over JOIN ... ON ... blocks
    size_t scan_pos = pos_from;
    while (true) {
        size_t pos_join = upper.find(" JOIN ", scan_pos == std::string::npos ? 0 : scan_pos);
        if (pos_join == std::string::npos) break;

        // find ON after this JOIN
        size_t pos_on = upper.find(" ON ", pos_join);
        if (pos_on == std::string::npos) {
            throw std::runtime_error("JOIN sin ON");
        }

        // find end of this JOIN clause: next " JOIN " or " WHERE " or end
        size_t next_join = upper.find(" JOIN ", pos_on);
        size_t next_where = upper.find(" WHERE ", pos_on);
        size_t end_clause = std::min(next_join == std::string::npos ? upper.size() : next_join,
                                     next_where == std::string::npos ? upper.size() : next_where);

        // Table name between "JOIN" and "ON"
        {
            size_t start_table = pos_join + 6; // after " JOIN "
            size_t end_table = pos_on;
            std::string t = trim(sql.substr(start_table, end_table - start_table));
            std::string tblname = trim(split(t, ' ')[0]);
            q.tables.push_back(tblname);
        }

        // ON clause between pos_on+4 and end_clause
        std::string on_clause = trim(sql.substr(pos_on + 4, end_clause - (pos_on + 4)));
        // Only support a single equality in ON (A.col = B.col)
=======
    // Mapa alias -> tabla real
    std::unordered_map<std::string,std::string> aliasMap;

    JoinQuery q;

    // === FROM ===
    size_t pos_from = upper.find(" FROM ");
    if (pos_from == std::string::npos)
        throw std::runtime_error("SQL no soportado: falta FROM");

    size_t start_from = pos_from + 6;
    size_t next_join  = upper.find(" JOIN ", start_from);
    size_t next_where = upper.find(" WHERE ", start_from);

    size_t end_from = std::min(
        next_join == std::string::npos ? upper.size() : next_join,
        next_where == std::string::npos ? upper.size() : next_where
    );

    {
        std::string from_block = trim(sql.substr(start_from, end_from - start_from));

        auto [realTbl, alias] = parse_table_alias(from_block);
        if (realTbl.empty())
            throw std::runtime_error("No se pudo parsear tabla en FROM");

        q.leftTable = realTbl;
        q.tables.push_back(realTbl);

        if (!alias.empty())
            aliasMap[to_upper_copy(alias)] = realTbl;
    }

    // === JOIN chain ===
    size_t scan_pos = pos_from;
    while (true) {
        size_t pos_join = upper.find(" JOIN ", scan_pos);
        if (pos_join == std::string::npos) break;

        size_t pos_on = upper.find(" ON ", pos_join);
        if (pos_on == std::string::npos)
            throw std::runtime_error("JOIN sin ON");

        size_t next_join  = upper.find(" JOIN ", pos_on);
        size_t next_where = upper.find(" WHERE ", pos_on);

        size_t end_clause = std::min(
            next_join == std::string::npos ? upper.size() : next_join,
            next_where == std::string::npos ? upper.size() : next_where
        );

        // Tabla del JOIN
        std::string join_table_block =
            trim(sql.substr(pos_join + 6, pos_on - (pos_join + 6)));

        auto [realTbl, alias] = parse_table_alias(join_table_block);
        if (realTbl.empty())
            throw std::runtime_error("No se pudo parsear tabla en JOIN");

        q.tables.push_back(realTbl);
        if (!alias.empty())
            aliasMap[to_upper_copy(alias)] = realTbl;

        // ON clause
        std::string on_clause = trim(sql.substr(pos_on + 4, end_clause - (pos_on + 4)));
>>>>>>> Stashed changes
        size_t eqpos = on_clause.find('=');
        if (eqpos == std::string::npos)
            throw std::runtime_error("ON sin '='");

        std::string left_expr  = trim(on_clause.substr(0, eqpos));
        std::string right_expr = trim(on_clause.substr(eqpos + 1));

        auto [lt, lk] = parse_qualified(left_expr);
        auto [rt, rk] = parse_qualified(right_expr);

<<<<<<< Updated upstream
        // If table qualifiers missing, assume left is previously mentioned table and right is newly joined table.
        // But prefer explicit qualifiers if present.
        if (lt.empty()) {
            // conservative: try to use the last table before this JOIN (the one just before current JOIN)
            // That table is q.tables[q.tables.size() - 2] because we already pushed this JOIN's right table above.
            if (q.tables.size() >= 2) lt = q.tables[q.tables.size() - 2];
        }
        if (rt.empty()) {
            // assume the right table we just pushed
            rt = q.tables.back();
        }

=======
        // Resolver alias
        auto resolve = [&](std::string &t) {
            std::string T = to_upper_copy(t);
            if (aliasMap.count(T))
                t = aliasMap[T];
        };
        if (!lt.empty()) resolve(lt);
        if (!rt.empty()) resolve(rt);

        if (lt.empty())
            lt = q.tables[q.tables.size() - 2]; // previous table
        if (rt.empty())
            rt = q.tables.back();              // right table

>>>>>>> Stashed changes
        JoinCondition jc;
        jc.leftTable  = lt;
        jc.leftKey    = lk;
        jc.rightTable = rt;
        jc.rightKey   = rk;
        q.joins.push_back(jc);

        scan_pos = end_clause;
    }

<<<<<<< Updated upstream
    // WHERE clause (optional)
    size_t pos_where = upper.find(" WHERE ");
    if (pos_where != std::string::npos) {
        size_t start = pos_where + 7; // after " WHERE "
        size_t end   = sql.size();
        std::string where_clause = trim(sql.substr(start, end - start));

        // Split by top-level AND (case-insensitive)
        std::vector<std::string> parts;
        std::string tmp = where_clause;
        size_t pos = 0;
        while (true) {
            // find " AND " in upper-case copy of the substring
            std::string tmp_upper = to_upper_copy(tmp.substr(pos));
            size_t p = tmp_upper.find(" AND ");
            if (p == std::string::npos) {
                parts.push_back(trim(tmp.substr(pos)));
                break;
            } else {
                parts.push_back(trim(tmp.substr(pos, p)));
                pos = pos + p + 5;
=======
    // === WHERE === 
    size_t pos_where = upper.find(" WHERE ");
    if (pos_where != std::string::npos) {

        std::string where_block = trim(sql.substr(pos_where + 7));

        size_t pos = 0;
        while (pos < where_block.size()) {

            size_t and_pos = to_upper_copy(where_block).find(" AND ", pos);
            std::string cond;

            if (and_pos == std::string::npos) {
                cond = trim(where_block.substr(pos));
                pos = where_block.size();
            } else {
                cond = trim(where_block.substr(pos, and_pos - pos));
                pos  = and_pos + 5;
>>>>>>> Stashed changes
            }

            if (cond.empty()) continue;

            // operadores soportados (orden importa)
            std::string op;
            size_t opos = std::string::npos;

            const std::vector<std::string> ops = {">=", "<=", "!=", "<>", "=", ">", "<"};
            for (auto &candidate : ops) {
                opos = to_upper_copy(cond).find(candidate);
                if (opos != std::string::npos) {
                    op = candidate;
                    break;
                }
            }

            if (opos == std::string::npos) {
                std::cerr << "Warning: condición ignorada (operador no soportado): "
                        << cond << "\n";
                continue;
            }

            std::string left_part  = trim(cond.substr(0, opos));
            std::string right_part = trim(cond.substr(opos + op.size()));
            std::string value = strip_quotes(right_part);

            auto [t, col] = parse_qualified(left_part);

            if (t.empty() || col.empty()) {
                std::cerr << "Warning: condición ignorada: " << cond << "\n";
                continue;
            }

            // Resolver alias
            std::string T = to_upper_copy(t);
            if (aliasMap.count(T))
                t = aliasMap[T];

            Predicate p;
            p.table  = t;
            p.column = col;
            p.op     = op;
            p.value  = value;
            q.predicates.push_back(p);
        }
    }


    return q;
}