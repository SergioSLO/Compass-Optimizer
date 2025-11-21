// Optimizer.h
#pragma once

#include <unordered_map>
#include <string>

#include "BDReader.h"
#include "SQLParser.h"

// Opción 2 (mejora incremental de tu código actual,
// explora órdenes left-deep usando tus CMSketch)
void run_query_plan(const std::unordered_map<std::string, Table> &tables,
                    const JoinQuery &q);

// Opción 1 (planner tipo COMPASS del paper:
// espacio de planes bushy/left/right + costo = suma de tamaños intermedios)
void run_query_plan_compass(const std::unordered_map<std::string, Table> &tables,
                            const JoinQuery &q);
