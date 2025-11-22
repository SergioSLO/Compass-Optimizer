#pragma once
#include <string>
#include "Optimizer.h"

// Genera archivo .dot con el plan
void export_plan_to_dot(const JoinTreeNode* root, const std::string& path);

// Convierte .dot -> .png usando Graphviz (si está instalado)
bool dot_to_png(const std::string& dot_path, const std::string& png_path);
