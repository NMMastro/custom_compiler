#include <iostream>
#include <vector>
#include <string>
#include <sstream>
#include <stack>
#include <unordered_set>
#include <map>

using namespace std;

// g++ -g -O0 -fPIC -std=gnu++20 -Wall -Wextra -pedantic-errors -Wno-unused-parameter -o wlp4type wlp4type.cc

const unordered_set<string> nonterminals = {
    "start",
    "procedures",
    "procedure",
    "main",
    "params",
    "paramlist",
    "type",
    "dcls",
    "dcl",
    "statements",
    "statement",
    "test",
    "expr",
    "term",
    "factor",
    "arglist",
    "lvalue"
};

map<string, string> symbolTable;

struct Node {
    string value;
    vector<Node*> children;
    string type;
};

// ---------------- HELPER FUNCTIONS ----------------

bool is_nonterminal(const string& symbol) {
    return nonterminals.count(symbol) > 0;
}

string get_id_name(Node* idNode) {
    string kind;
    string name;

    stringstream ss(idNode->value);
    ss >> kind >> name;

    return name;
}

string get_dcl_type(Node* dcl) {
    Node* typeNode = dcl->children[0];

    if (typeNode->value == "type LONG") {
        return "long";
    }

    if (typeNode->value == "type LONG STAR") {
        return "long*";
    }

    return "";
}

string type_expression(Node* node) {
    string symbol;
    stringstream ss(node->value);
    ss >> symbol;

    if (symbol == "NUM") {
        node->type = "long";
        return "long";
    }

    if (symbol == "NULL") {
        node->type = "long*";
        return "long*";
    }

    if (node->value == "factor NUM" ||
        node->value == "factor NULL" ||
        node->value == "term factor" ||
        node->value == "expr term") {

        string childType = type_expression(node->children[0]);
        node->type = childType;
        return childType;
    }

    if (node->value == "factor LPAREN expr RPAREN") {
        string childType = type_expression(node->children[1]);
        node->type = childType;
        return childType;
    }

    if (node->value == "factor ID") {
        Node* idNode = node->children[0];
        string name = get_id_name(idNode);

        if (symbolTable.count(name) == 0) {
            return "";
        }

        string idType = symbolTable[name];
        idNode->type = idType;
        node->type = idType;

        return idType;
    }

    return "";
}

bool add_declaration(Node* dcl) {
    string name = get_id_name(dcl->children[1]);
    string declaredType = get_dcl_type(dcl);

    if (symbolTable.count(name) > 0) {
        return false;
    }

    symbolTable[name] = declaredType;

    dcl->children[1]->type = declaredType;

    return true;
}

bool process_dcls(Node* node) {
    if (node->value == "dcls .EMPTY") {
        return true;
    }
    
    if (!process_dcls(node->children[0])) {
        return false;
    }

    string dclType = get_dcl_type(node->children[1]);
    string initType = type_expression(node->children[3]);

    if (dclType != initType) {
        return false;
    }

    if (!add_declaration(node->children[1])) {
        return false;
    }

    return true;

}


Node* build_tree() {
    // Get input line for this node.
    string line;
    getline(cin, line);

    // Init the node with input line
    Node* n = new Node;
    n->value = line;
    n->type = "";

    stringstream ss(line);

    // Get symbol
    string symbol;
    ss >> symbol;

    // If terminal return n
    if (!is_nonterminal(symbol)) {
        return n;
    }

    while (ss >> symbol) {
        if (symbol != ".EMPTY") {
            n->children.push_back(build_tree());
        }
    }
    
    return n;
}

void print_tree(Node* node) {
    cout << node->value;

    if (node->type != "") {
        cout << " : " << node->type;
    }
    cout << endl;

    for (Node* child : node->children) {
        print_tree(child);
    }
}

void delete_tree(Node* node) {
    if (node == nullptr) return;

    for (Node* child : node->children) {
        delete_tree(child);
    }

    delete node;
}

void process_error(Node* root) {
    cerr << "ERROR" << endl;
    delete_tree(root);
}

int main() {

    Node* root = build_tree();
    Node* mainNode = root->children[0];

    Node* firstDcl = mainNode->children[3];
    Node* secondDcl = mainNode->children[5];
    Node* dclsNode = mainNode->children[8];
    Node* returnExpr = mainNode->children[11];

    // Check if parameters of wain do not have the same name
    if (!add_declaration(firstDcl) || !add_declaration(secondDcl)) {
        process_error(root);
        return 1;
    }

    // Check if type of second parameter of wain is not long
    if (get_dcl_type(secondDcl) != "long") {
        process_error(root);
        return 1;
    }

    if(!process_dcls(dclsNode)) {
        process_error(root);
        return 1;
    }

    // Check if return type is not long
    if (type_expression(returnExpr) != "long") {
        process_error(root);
        return 1;
    }

    print_tree(root);
    delete_tree(root);
    return 0;
}