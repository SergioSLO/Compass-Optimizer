#include "PlanVisualizer.h"

#include <cstdlib>
#include <fstream>
#include <iostream>

namespace {

void write_node(std::ofstream &out, const JoinTreeNode *node, int &id) {
    if (!node) return;
    int my_id = id++;
    if (node->is_leaf) {
        out << "  node" << my_id << " [label=\"Scan "
            << node->leaf_table << "\\n est=" << node->est_card
            << "\", shape=box, style=filled, fillcolor=lightblue];\n";
        return;
    }

    out << "  node" << my_id << " [label=\"Join(";
    for (size_t i = 0; i < node->tables.size(); ++i) {
        if (i) out << ",";
        out << node->tables[i];
    }
    out << ")\\n est=" << node->est_card
        << "\\n cost=" << node->cost
        << "\", shape=oval, style=filled, fillcolor=lightyellow];\n";

    if (node->left) {
        int left_id = id;
        write_node(out, node->left.get(), id);
        out << "  node" << my_id << " -> node" << left_id << ";\n";
    }
    if (node->right) {
        int right_id = id;
        write_node(out, node->right.get(), id);
        out << "  node" << my_id << " -> node" << right_id << ";\n";
    }
}

}  // namespace

void export_plan_to_dot(const JoinTreeNode *root, const std::string &path) {
    if (!root) {
        std::cerr << "No hay plan para exportar.\n";
        return;
    }
    std::ofstream out(path);
    if (!out) {
        std::cerr << "No se pudo crear archivo DOT en " << path << "\n";
        return;
    }

    out << "digraph plan {\n";
    out << "  rankdir=TB;\n";
    out << "  node [fontname=\"Arial\"];\n";
    int id = 0;
    write_node(out, root, id);
    out << "}\n";
}

bool dot_to_png(const std::string &dot_path, const std::string &png_path) {
    std::string cmd = "dot -Tpng \"" + dot_path + "\" -o \"" + png_path + "\"";
    int r = std::system(cmd.c_str());
    return (r == 0);
}
