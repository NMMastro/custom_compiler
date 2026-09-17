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

map<string, vector<string>> procedureTable;




struct Node {
    string value;
    vector<Node*> children;
    string type;
};

bool process_arglist(Node* node, map<string, string>& localTable, vector<string>& argumentTypes);
bool process_statements(Node* node, map<string, string>& localTable);
bool process_statement(Node* node, map<string, string>& localTable);
bool process_test(Node* node, map<string, string>& localTable);


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

string type_expression(Node* node, map<string, string>& localTable) {
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

        string childType = type_expression(node->children[0], localTable);
        node->type = childType;
        return childType;
    }

    if (node->value == "factor LPAREN expr RPAREN") {
        string childType = type_expression(node->children[1], localTable);
        node->type = childType;
        return childType;
    }

    if (node->value == "factor ID" || node->value == "lvalue ID") {
        Node* idNode = node->children[0];
        string name = get_id_name(idNode);

        if (localTable.count(name) == 0) {
            return "";
        }

        string idType = localTable[name];
        idNode->type = idType;
        node->type = idType;

        return idType;
    }

    if (node->value == "factor STAR factor" || node->value == "lvalue STAR factor") {
        string factorType = type_expression(node->children[1], localTable);

        if (factorType != "long*") {
            return "";
        }

        node->type = "long";
        return "long";
    }

    if (node->value == "factor AMP lvalue") {
        string lvalueType = type_expression(node->children[1], localTable);

        if (lvalueType != "long") {
            return "";
        }

        node->type = "long*";
        return "long*";
    }

    if (node->value == "lvalue LPAREN lvalue RPAREN") {
        string lvalueType = type_expression(node->children[1], localTable);

        node->type = lvalueType;
        return lvalueType;
    }

    if (node->value == "factor NEW LONG LBRACK expr RBRACK") {
        string exprType = type_expression(node->children[3], localTable);

        if (exprType != "long") {
            return "";
        }

        node->type = "long*";
        return "long*";
    }

    if (node->value == "term term STAR factor" ||
        node->value == "term term SLASH factor" ||
        node->value == "term term PCT factor") {

        string leftType = type_expression(node->children[0], localTable);
        string rightType = type_expression(node->children[2], localTable);

        if (leftType != "long" || rightType != "long") {
            return "";
        }

        node->type = "long";
        return "long";
    }

    if (node->value == "expr expr PLUS term") {
        string leftType = type_expression(node->children[0], localTable);
        string rightType = type_expression(node->children[2], localTable);

        if (leftType == "long" && rightType == "long") {
            node->type = "long";
            return "long";
        }

        if ((leftType == "long*" && rightType == "long") ||
            (leftType == "long" && rightType == "long*")) {

            node->type = "long*";
            return "long*";
        }

        return "";
    }

    if (node->value == "expr expr MINUS term") {
        string leftType = type_expression(node->children[0], localTable);
        string rightType = type_expression(node->children[2], localTable);

        if (leftType == "long" && rightType == "long") {
            node->type = "long";
            return "long";
        }

        if (leftType == "long*" && rightType == "long") {
            node->type = "long*";
            return "long*";
        }

        if (leftType == "long*" && rightType == "long*") {
            node->type = "long";
            return "long";
        }

        return "";
    }

    if (node->value == "factor ID LPAREN RPAREN") {
        Node* idNode = node->children[0];
        string name = get_id_name(idNode);

        if (localTable.count(name) > 0) {
            return "";
        }

        if (procedureTable.count(name) == 0) {
            return "";
        }

        if (!procedureTable[name].empty()) {
            return "";
        }

        node->type = "long";
        return "long";
    }

    if (node->value == "factor ID LPAREN arglist RPAREN") {
        Node* idNode = node->children[0];
        string name = get_id_name(idNode);

        if (localTable.count(name) > 0) {
            return "";
        }

        if (procedureTable.count(name) == 0) {
            return "";
        }

        vector<string> argumentTypes;

        if (!process_arglist(
                node->children[2],
                localTable,
                argumentTypes)) {
            return "";
        }

        if (argumentTypes != procedureTable[name]) {
            return "";
        }

        node->type = "long";
        return "long";
    }

    if (node->value == "factor GETCHAR LPAREN RPAREN") {
        node->type = "long";
        return "long";
    }

    return "";
}



bool process_statement(Node* node, map<string, string>& localTable) {
    if (node->value == "statement lvalue BECOMES expr SEMI") {
        string leftType = type_expression(node->children[0], localTable);
        string rightType = type_expression(node->children[2], localTable);
        return leftType != "" && leftType == rightType;
    }

    if (node->value == "statement PRINTLN LPAREN expr RPAREN SEMI") {
        return type_expression(node->children[2], localTable) == "long";
    }

    if (node->value == "statement PUTCHAR LPAREN expr RPAREN SEMI") {
        return type_expression(node->children[2], localTable) == "long";
    }

    if (node->value == "statement DELETE LBRACK RBRACK expr SEMI") {
        return type_expression(node->children[3], localTable) == "long*";
    }

    if (node->value == "statement WHILE LPAREN test RPAREN LBRACE statements RBRACE") {
        if (!process_test(node->children[2], localTable)) {
            return false;
        }
        return process_statements(node->children[5],localTable);
    }

    if (node->value == "statement IF LPAREN test RPAREN LBRACE statements RBRACE ELSE LBRACE statements RBRACE") {

        if (!process_test(node->children[2], localTable)) {
            return false;
        }

        if (!process_statements(node->children[5], localTable)) {
            return false;
        }

        return process_statements(node->children[9], localTable);
    }

    return false;
}

bool process_test(Node* node, map<string, string>& localTable) {
    string leftType = type_expression(node->children[0], localTable);
    string rightType = type_expression(node->children[2], localTable);

    if (leftType == "" || rightType == "") {
        return false;
    }

    return leftType == rightType;
}

bool process_statements(Node* node, map<string, string>& localTable) {
    if (node->value == "statements .EMPTY") {
        return true;
    }

    if (node->value == "statements statements statement") {
        if (!process_statements(node->children[0],localTable)) {
            return false;
        }

        return process_statement(node->children[1],localTable);
    }

    return false;
}


bool add_declaration(Node* dcl, map<string, string>& localTable) {
    string name = get_id_name(dcl->children[1]);
    string declaredType = get_dcl_type(dcl);

    if (localTable.count(name) > 0) {
        return false;
    }

    localTable[name] = declaredType;

    dcl->children[1]->type = declaredType;

    return true;
}

bool process_dcls(Node* node, map<string, string>& localTable) {
    if (node->value == "dcls .EMPTY") {
        return true;
    }
    
    if (!process_dcls(node->children[0], localTable)) {
        return false;
    }

    string dclType = get_dcl_type(node->children[1]);
    string initType = type_expression(node->children[3], localTable);

    if (dclType != initType) {
        return false;
    }

    if (!add_declaration(node->children[1], localTable)) {
        return false;
    }

    return true;

}

bool process_paramlist(Node* node, map<string, string>&localTable, vector<string>& signature) {
    Node* dcl = node->children[0];
    string parameterType = get_dcl_type(dcl);

    if (!add_declaration(dcl, localTable)) {
        return false;
    }
    signature.push_back(parameterType);

    if (node->value == "paramlist dcl") {
        return true;
    }

    if (node->value == "paramlist dcl COMMA paramlist") {
        return process_paramlist(node->children[2], localTable, signature);
    }
    return false;
}

bool process_params(Node* node, map<string, string>& localTable, vector<string>& signature) {
    if (node->value == "params .EMPTY") {
        return true;
    }

    if (node->value == "params paramlist") {
        return process_paramlist(
            node->children[0],
            localTable,
            signature
        );
    }

    return false;
}

bool process_arglist(Node* node, map<string, string>& localTable, vector<string>& argumentTypes) {
    string firstType = type_expression(node->children[0], localTable);

    if (firstType == "") {
        return false;
    }

    argumentTypes.push_back(firstType);

    if (node->value == "arglist expr") {
        return true;
    }

    if (node->value == "arglist expr COMMA arglist") {
        return process_arglist(
            node->children[2],
            localTable,
            argumentTypes
        );
    }

    return false;
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

bool process_main(Node* mainNode) {
    map<string, string> localTable;

    Node* firstDcl = mainNode->children[3];
    Node* secondDcl = mainNode->children[5];
    Node* dclsNode = mainNode->children[8];
    Node* statementsNode = mainNode->children[9];
    Node* returnExpr = mainNode->children[11];

    if (!add_declaration(firstDcl, localTable) || !add_declaration(secondDcl, localTable)) {
        return false;
    }

    if (get_dcl_type(secondDcl) != "long") {
        return false;
    }

    if (!process_dcls(dclsNode, localTable)) {
        return false;
    }

    if (!process_statements(statementsNode, localTable)) {
        return false;
    }

    if (type_expression(returnExpr, localTable) != "long") {
        return false;
    }

    return true;
}

bool process_procedure(Node* node) {
    string name = get_id_name(node->children[1]);

    if (procedureTable.count(name) > 0) {
        return false;
    }

    map<string, string> localTable;
    vector<string> signature;

    Node* paramsNode = node->children[3];
    Node* dclsNode = node->children[6];
    Node* statementsNode = node->children[7];
    Node* returnExpr = node->children[9];

    if (!process_params(paramsNode, localTable, signature)) {
        return false;
    }

    procedureTable[name] = signature;


    if (!process_dcls(dclsNode, localTable)) {
        return false;
    }

    if (!process_statements(statementsNode, localTable)) {
        return false;
    }

    if (type_expression(returnExpr, localTable) != "long") {
        return false;
    }

    return true;
}

bool process_procedures(Node* node) {
    if (node->value == "procedures main") {
        return process_main(node->children[0]);
    }

    if (node->value == "procedures procedure procedures") {
        if (!process_procedure(node->children[0])) {
            return false;
        }

        return process_procedures(node->children[1]);
    }

    return false;
}


int main() {
    Node* root = build_tree();

    if (!process_procedures(root)) {
        process_error(root);
        return 1;
    }

    print_tree(root);
    delete_tree(root);

    return 0;
}