#include "PlanVisualizer.h"
#include <fstream>
#include <cstdlib>
#include <iostream>

static void write_node(std::ofstream& out, const JoinTreeNode* node, int& id) {
    if (!node) return;

    int my_id = id++;
    if (node->is_leaf) {
        out << "  node" << my_id << " [label=\"Scan " 
            << node->leaf_table << "\\n est=" << node->est_card 
            << "\", shape=box, style=filled, fillcolor=lightblue];\n";
    } else {
        out << "  node" << my_id << " [label=\"Join(";
        for (size_t i = 0; i < node->tables.size(); i++) {
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
}

void export_plan_to_dot(const JoinTreeNode* root, const std::string& path) {
    std::ofstream out(path);
    if (!out) {
        std::cerr << "Error: no se pudo crear " << path << "\n";
        return;
    }

    out << "digraph plan {\n";
    out << "  rankdir=TB;\n";
    out << "  node [fontname=\"Arial\"];\n";

    int id = 0;
    write_node(out, root, id);

    out << "}\n";
}

bool dot_to_png(const std::string& dot_path, const std::string& png_path) {
    std::string cmd = "dot -Tpng " + dot_path + " -o " + png_path;
    int r = system(cmd.c_str());
    return (r == 0);
}
