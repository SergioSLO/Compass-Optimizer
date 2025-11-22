// SQLParser.h
#pragma once

#include <string>
#include <vector>

struct Predicate {
    std::string table;
    std::string column;
    std::string value;
    std::string op;
};

struct JoinCondition {
    std::string leftTable;
    std::string leftKey;
    std::string rightTable;
    std::string rightKey;
};

struct JoinQuery {
    // Main FROM table (left-most)
    std::string leftTable;

    // All tables in the order: leftTable, right1, right2, ...
    std::vector<std::string> tables;

    // Sequence of join conditions, in the order encountered
    std::vector<JoinCondition> joins;

    // WHERE predicates (only = and combined with AND supported)
    std::vector<Predicate> predicates;
};

JoinQuery parse_sql_file(const std::string &path);
