#include "SQLParser.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <unordered_map>

namespace {

std::string trim(const std::string &s) {
    size_t start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    size_t end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

std::string to_upper_copy(std::string s) {
    for (char &c : s) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    return s;
}

std::pair<std::string, std::string> parse_table_alias(const std::string &expr) {
    std::istringstream iss(expr);
    std::vector<std::string> tokens;
    std::string tok;
    while (iss >> tok) tokens.push_back(tok);
    if (tokens.empty()) return {"", ""};
    if (tokens.size() == 1) return {tokens[0], ""};
    if (tokens.size() == 2) {
        if (to_upper_copy(tokens[0]) == "AS" || to_upper_copy(tokens[1]) == "AS") {
            throw std::runtime_error("Sintaxis inválida de alias: " + expr);
        }
        return {tokens[0], tokens[1]};
    }
    if (tokens.size() == 3 && to_upper_copy(tokens[1]) == "AS") {
        return {tokens[0], tokens[2]};
    }
    throw std::runtime_error("Alias demasiado complejo: " + expr);
}

std::pair<std::string, std::string> parse_qualified(const std::string &expr) {
    std::string t = trim(expr);
    size_t dot = t.find('.');
    if (dot == std::string::npos) return {"", t};
    std::string table = trim(t.substr(0, dot));
    std::string col = trim(t.substr(dot + 1));
    return {table, col};
}

std::string strip_quotes(const std::string &s) {
    std::string t = trim(s);
    if (t.size() >= 2) {
        char c1 = t.front();
        char c2 = t.back();
        if ((c1 == '\'' && c2 == '\'') || (c1 == '"' && c2 == '"')) {
            return t.substr(1, t.size() - 2);
        }
    }
    return t;
}

std::vector<std::string> split_conjuncts(const std::string &expr) {
    std::vector<std::string> parts;
    std::string upper = to_upper_copy(expr);
    size_t start = 0;
    while (start < expr.size()) {
        size_t pos = upper.find(" AND ", start);
        if (pos == std::string::npos) {
            std::string part = trim(expr.substr(start));
            if (!part.empty()) parts.push_back(part);
            break;
        }
        std::string part = trim(expr.substr(start, pos - start));
        if (!part.empty()) parts.push_back(part);
        start = pos + 5;
    }
    return parts;
}

std::string resolve_table_name(const std::unordered_map<std::string, std::string> &aliases,
                               const std::string &token) {
    std::string upper = to_upper_copy(token);
    auto it = aliases.find(upper);
    if (it != aliases.end()) return it->second;
    return token;
}

}  // namespace

JoinQuery parse_sql_file(const std::string &path) {
    std::ifstream in(path);
    if (!in) throw std::runtime_error("No se pudo abrir SQL: " + path);

    std::string sql, line;
    while (std::getline(in, line)) {
        sql += line;
        sql.push_back(' ');
    }

    sql = trim(sql);
    if (!sql.empty() && sql.back() == ';') sql.pop_back();
    if (sql.empty()) throw std::runtime_error("Archivo SQL vacío");

    std::string upper = to_upper_copy(sql);
    size_t pos_from = upper.find(" FROM ");
    if (pos_from == std::string::npos) {
        throw std::runtime_error("SQL no soportado: falta FROM");
    }

    size_t pos_where = upper.find(" WHERE ", pos_from);
    JoinQuery q;
    std::unordered_map<std::string, std::string> aliasMap;

    auto register_alias = [&](const std::string &real, const std::string &alias) {
        aliasMap[to_upper_copy(real)] = real;
        if (!alias.empty()) aliasMap[to_upper_copy(alias)] = real;
    };

    size_t from_start = pos_from + 6;
    size_t first_join = upper.find(" JOIN ", from_start);
    size_t from_end = std::min(first_join == std::string::npos ? upper.size() : first_join,
                               pos_where == std::string::npos ? upper.size() : pos_where);

    std::string from_block = trim(sql.substr(from_start, from_end - from_start));
    auto [from_table, from_alias] = parse_table_alias(from_block);
    if (from_table.empty()) throw std::runtime_error("FROM sin tabla");
    q.leftTable = from_table;
    q.tables.push_back(from_table);
    register_alias(from_table, from_alias);

    size_t scan_pos = pos_from;
    while (true) {
        size_t pos_join = upper.find(" JOIN ", scan_pos);
        if (pos_join == std::string::npos) break;
        size_t pos_on = upper.find(" ON ", pos_join);
        if (pos_on == std::string::npos) {
            throw std::runtime_error("JOIN sin ON");
        }
        size_t next_join = upper.find(" JOIN ", pos_on + 4);
        size_t clause_end = std::min(next_join == std::string::npos ? upper.size() : next_join,
                                     pos_where == std::string::npos ? upper.size() : pos_where);

        std::string join_table_block = trim(sql.substr(pos_join + 6, pos_on - (pos_join + 6)));
        auto [join_table, join_alias] = parse_table_alias(join_table_block);
        if (join_table.empty()) throw std::runtime_error("JOIN sin tabla");
        q.tables.push_back(join_table);
        register_alias(join_table, join_alias);

        std::string on_clause = trim(sql.substr(pos_on + 4, clause_end - (pos_on + 4)));
        size_t eqpos = on_clause.find('=');
        if (eqpos == std::string::npos) {
            throw std::runtime_error("ON sin igualdad (solo se soporta =)");
        }
        auto [left_tbl_token, left_col] = parse_qualified(on_clause.substr(0, eqpos));
        auto [right_tbl_token, right_col] = parse_qualified(on_clause.substr(eqpos + 1));
        if (left_tbl_token.empty() || right_tbl_token.empty()) {
            throw std::runtime_error("ON requiere tabla.col en ambos lados");
        }

        JoinCondition jc;
        jc.leftTable = resolve_table_name(aliasMap, left_tbl_token);
        jc.leftKey = left_col;
        jc.rightTable = resolve_table_name(aliasMap, right_tbl_token);
        jc.rightKey = right_col;
        q.joins.push_back(jc);

        scan_pos = pos_on + 4;
    }

    if (pos_where != std::string::npos) {
        size_t where_start = pos_where + 7;
        std::string where_clause = trim(sql.substr(where_start));
        for (const auto &cond : split_conjuncts(where_clause)) {
            static const std::vector<std::string> ops = {"<=", ">=", "!=", "<>", "=", "<", ">"};
            size_t op_pos = std::string::npos;
            std::string op_used;
            for (const auto &op : ops) {
                op_pos = cond.find(op);
                if (op_pos != std::string::npos) {
                    op_used = op;
                    break;
                }
            }
            if (op_pos == std::string::npos) {
                throw std::runtime_error("Predicado no soportado: " + cond);
            }
            auto [tbl_token, col] = parse_qualified(cond.substr(0, op_pos));
            if (tbl_token.empty()) {
                throw std::runtime_error("WHERE requiere tabla.col");
            }
            std::string val = cond.substr(op_pos + op_used.size());

            Predicate p;
            p.table = resolve_table_name(aliasMap, tbl_token);
            p.column = col;
            p.value = strip_quotes(val);
            p.op = to_upper_copy(op_used);
            q.predicates.push_back(p);
        }
    }

    if (q.tables.size() < 1 || q.joins.empty()) {
        throw std::runtime_error("El SQL debe contener al menos un JOIN");
    }

    return q;
}
