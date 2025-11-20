#include "SQLParser.h"

#include <fstream>
#include <sstream>
#include <stdexcept>
#include <algorithm>
#include <cctype>
#include <iostream>

static std::string trim(const std::string &s) {
    size_t start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    size_t end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

static std::string to_upper_copy(std::string s) {
    for (char &c : s) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    return s;
}

static std::vector<std::string> split(const std::string &s, char delim) {
    std::vector<std::string> res;
    std::string cur;
    std::stringstream ss(s);
    while (std::getline(ss, cur, delim)) {
        res.push_back(cur);
    }
    return res;
}

static std::string strip_quotes(const std::string &s) {
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

// Parte "A.col" -> ("A","col")
static std::pair<std::string,std::string> parse_qualified(const std::string &s) {
    std::string tmp = trim(s);
    size_t dot = tmp.find('.');
    if (dot == std::string::npos) {
        return {"", tmp};
    }
    std::string t = trim(tmp.substr(0, dot));
    std::string c = trim(tmp.substr(dot + 1));
    return {t, c};
}

JoinQuery parse_sql_file(const std::string &path) {
    std::ifstream in(path);
    if (!in) {
        throw std::runtime_error("No se pudo abrir SQL: " + path);
    }

    std::string sql, line;
    while (std::getline(in, line)) {
        sql += line + " ";
    }
    sql = trim(sql);

    if (!sql.empty() && sql.back() == ';') {
        sql.pop_back();
    }

    std::string upper = to_upper_copy(sql);

    size_t pos_from  = upper.find(" FROM ");
    size_t pos_join  = upper.find(" JOIN ",  pos_from == std::string::npos ? 0 : pos_from);
    size_t pos_on    = upper.find(" ON ",    pos_join == std::string::npos ? 0 : pos_join);
    size_t pos_where = upper.find(" WHERE ", pos_on   == std::string::npos ? 0 : pos_on);

    if (pos_from == std::string::npos || pos_join == std::string::npos || pos_on == std::string::npos) {
        throw std::runtime_error("SQL no soportado: se requiere SELECT * FROM A JOIN B ON ...");
    }

    JoinQuery q;

    // Tabla izquierda
    {
        size_t start = pos_from + 6; // len(" FROM ")
        size_t end   = pos_join;
        std::string t = trim(sql.substr(start, end - start));
        q.leftTable = trim(split(t, ' ')[0]);
    }

    // Tabla derecha
    {
        size_t start = pos_join + 6; // len(" JOIN ")
        size_t end   = pos_on;
        std::string t = trim(sql.substr(start, end - start));
        q.rightTable = trim(split(t, ' ')[0]);
    }

    // ON
    {
        size_t start = pos_on + 4; // len(" ON ")
        size_t end   = (pos_where == std::string::npos) ? sql.size() : pos_where;
        std::string on_clause = trim(sql.substr(start, end - start));

        size_t eqpos = on_clause.find('=');
        if (eqpos == std::string::npos) {
            throw std::runtime_error("ON clause sin '='");
        }
        std::string left_expr  = trim(on_clause.substr(0, eqpos));
        std::string right_expr = trim(on_clause.substr(eqpos + 1));

        auto [lt, lk] = parse_qualified(left_expr);
        auto [rt, rk] = parse_qualified(right_expr);

        if (lt.empty()) lt = q.leftTable;
        if (rt.empty()) rt = q.rightTable;

        q.leftKey  = lk;
        q.rightKey = rk;
    }

    // WHERE (opcional)
    if (pos_where != std::string::npos) {
        size_t start = pos_where + 7; // len(" WHERE ")
        size_t end   = sql.size();
        std::string where_clause = trim(sql.substr(start, end - start));

        std::vector<std::string> parts;
        std::string tmp = where_clause;
        size_t pos = 0;
        while (true) {
            size_t p = to_upper_copy(tmp).find(" AND ", pos);
            if (p == std::string::npos) {
                parts.push_back(trim(tmp.substr(pos)));
                break;
            } else {
                parts.push_back(trim(tmp.substr(pos, p - pos)));
                pos = p + 5;
            }
        }

        for (auto &cond : parts) {
            if (cond.empty()) continue;
            size_t eq = cond.find('=');
            if (eq == std::string::npos) {
                std::cerr << "Advertencia: se ignora la condición (no es '='): " << cond << "\n";
                continue;
            }
            std::string left_expr  = trim(cond.substr(0, eq));
            std::string right_expr = trim(cond.substr(eq + 1));

            auto [tt, cc] = parse_qualified(left_expr);
            if (tt.empty()) {
                std::cerr << "Advertencia: condición sin tabla explícita, se ignora: " << cond << "\n";
                continue;
            }

            Predicate p;
            p.table  = tt;
            p.column = cc;
            p.value  = strip_quotes(right_expr);
            q.predicates.push_back(p);
        }
    }

    return q;
}
