#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "BDReader.h"
#include "SQLParser.h"

struct JoinTreeNode {
    std::vector<std::string> tables;
    double est_card = 0.0;
    double cost = 0.0;
    bool is_leaf = false;
    std::string leaf_table;
    std::unique_ptr<JoinTreeNode> left;
    std::unique_ptr<JoinTreeNode> right;
};

void run_query_plan(const std::unordered_map<std::string, Table> &tables,
                    const JoinQuery &q);

std::unique_ptr<JoinTreeNode> run_query_plan_compass(
    const std::unordered_map<std::string, Table> &tables,
    const JoinQuery &q);
