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
    std::string leftTable;
    std::vector<std::string> tables;
    std::vector<JoinCondition> joins;
    std::vector<Predicate> predicates;
};

JoinQuery parse_sql_file(const std::string &path);
