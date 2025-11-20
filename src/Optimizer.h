#pragma once
#include <unordered_map>
#include <string>

#include "BDReader.h"
#include "SQLParser.h"

// Ejecuta el "plan" para una JoinQuery sobre las tablas cargadas,
// construye sketches, estima join y muestra el query plan.
void run_query_plan(const std::unordered_map<std::string, Table> &tables,
                    const JoinQuery &q);
