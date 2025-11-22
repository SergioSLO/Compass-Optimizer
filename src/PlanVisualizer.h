#pragma once

#include <string>

#include "Optimizer.h"

void export_plan_to_dot(const JoinTreeNode *root, const std::string &path);
bool dot_to_png(const std::string &dot_path, const std::string &png_path);
