#pragma once
#include <string>
#include <vector>

struct Predicate {
    std::string table;   // nombre tabla
    std::string column;  // columna
    std::string value;   // valor string (ya sin comillas)
};

struct JoinQuery {
    std::string leftTable;
    std::string rightTable;
    std::string leftKey;   // columna join en tabla izquierda
    std::string rightKey;  // columna join en tabla derecha
    std::vector<Predicate> predicates;
};

// Parsea un archivo .sql con una query del tipo:
// SELECT * FROM A JOIN B ON A.col = B.col [WHERE ...];
JoinQuery parse_sql_file(const std::string &path);
