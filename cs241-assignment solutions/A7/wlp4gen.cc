#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <unordered_set>
#include <sstream>

using namespace std;

// g++ -g -O0 -fPIC -std=gnu++20 -Wall -Wextra -pedantic-errors -Wno-unused-parameter -o wlp4gen wlp4gen.cc

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


struct Node {
    string value;
    vector<Node*> children;
    string type;
};

struct SymbolInfo {
    string type;
    int offset;
};

void generate(Node* node);
void generate_main(Node* node);
void generate_expr(Node* node, const map<string, SymbolInfo>& symbolTable);
void generate_term(Node* node, const map<string, SymbolInfo>& symbolTable);
void generate_factor(Node* node, const map<string, SymbolInfo>& symbolTable);
void generate_statement(Node* node, const map<string, SymbolInfo>& symbolTable);
void generate_statements(Node* node, const map<string, SymbolInfo>& symbolTable);

int labelCounter = 0;

bool is_nonterminal(const string& symbol) {
    return nonterminals.count(symbol) > 0;
}

string make_label(const string& name_prefix) {
    string ret = name_prefix + to_string(labelCounter);
    ++labelCounter;

    return ret;
}

Node* build_tree() {
    // Get input line for this node.
    string line;
    getline(cin, line);

    // Init the node
    Node* n = new Node;

    // remove " : long*" or " : long" ending
    size_t len = line.size();
    if (len >= 8 && line.substr(len - 8, 8) == " : long*") {
        line = line.substr(0, len-8);
        n->type = "long*";
    }
    else if (len >= 7 && line.substr(len - 7, 7) == " : long") {
        line = line.substr(0, len-7);
        n->type = "long";
    }
    else {
        n->type = "";
    }
    n->value = line;

    // Get symbol
    stringstream ss(line);

    string symbol;
    ss >> symbol;

    // If terminal return n
    if (!is_nonterminal(symbol)) {
        return n;
    }

    // Make children tree
    while (ss >> symbol) {
        if (symbol != ".EMPTY") {
            n->children.push_back(build_tree());
        }
    }
    
    return n;
}

string get_id_name(Node* idNode) {
    string kind;
    string name;

    stringstream ss(idNode->value);
    ss >> kind >> name;

    return name;
}

string get_num_value(Node*numNode) {
    string kind;
    string num;

    stringstream ss(numNode->value);
    ss >> kind >> num;

    return num;
}

void load_constant(const string& reg, const string& value) {
    cout << "ldr " << reg << ", 8" << endl;
    cout << "b 12" << endl;
    cout << ".8byte " << value << endl;
}

void push (const string& reg) {
    cout << "stur " << reg << ", [sp, -8]" << endl;
    cout << "sub sp, sp, x8" << endl;
}

void pop (const string& reg) {
    cout << "add sp, sp, x8" << endl;
    cout << "ldur " << reg << ", [sp, -8]" << endl;
}

void generate_factor(Node* node, const map<string, SymbolInfo>& symbolTable) {
    if (node->value == "factor NUM") {
        load_constant("x0", get_num_value(node->children[0]));
    }
    else if (node->value == "factor ID") {
        string name = get_id_name(node->children[0]);
        int offset = symbolTable.at(name).offset;

        cout << "ldur x0, [x29, " << offset << "]" << endl;
    }
    else if (node->value == "factor LPAREN expr RPAREN") {
        generate_expr(node->children[1], symbolTable);
    }
    else if (node->value == "factor GETCHAR LPAREN RPAREN") {
        cout << "ldur x0, [x11, 0]" << endl;
    }
}

void generate_expr(Node* node, const map<string, SymbolInfo>& symbolTable) {
    if (node->value == "expr term") {
        generate_term(node->children[0], symbolTable);
    }
    else if (node->value == "expr expr PLUS term") {
        generate_expr(node->children[0], symbolTable);
        push("x0");

        generate_term(node->children[2], symbolTable);

        pop("x1");

        cout << "add x0, x1, x0" << endl;
    }
    else if (node->value == "expr expr MINUS term") {
        generate_expr(node->children[0], symbolTable);
        push("x0");

        generate_term(node->children[2], symbolTable);

        pop("x1");

        cout << "sub x0, x1, x0" << endl;
    }

}

void generate_term(Node* node, const map<string, SymbolInfo>& symbolTable) {
    if (node->value == "term factor") {
        generate_factor(node->children[0], symbolTable);
    }
    else if (node->value == "term term STAR factor") {
        generate_term(node->children[0], symbolTable);
        push("x0");

        generate_factor(node->children[2], symbolTable);

        pop("x1");

        cout << "mul x0, x1, x0" << endl;
    }
    else if (node->value == "term term SLASH factor") {
        generate_term(node->children[0], symbolTable);
        push("x0");

        generate_factor(node->children[2], symbolTable);

        pop("x1");

        cout << "sdiv x0, x1, x0" << endl;
    }
    else if (node->value == "term term PCT factor") {
        generate_term(node->children[0], symbolTable);
        push("x0");

        generate_factor(node->children[2], symbolTable);

        pop("x1");

        cout << "sdiv x2, x1, x0" << endl;
        cout << "mul x2, x2, x0" << endl;
        cout << "sub x0, x1, x2" << endl;
    }

}

void generate_dcls(Node* node, map<string, SymbolInfo>& symbolTable, int& frameSlots) {
    if (node->value == "dcls .EMPTY") {
        return;
    }

    if (node->value == "dcls dcls dcl BECOMES NUM SEMI") {
        generate_dcls(node->children[0], symbolTable, frameSlots);

        Node* dclNode = node->children[1];
        Node* numNode = node->children[3];

        string dclName = get_id_name(dclNode->children[1]);
        string numValue = get_num_value(numNode);

        int offset = (frameSlots - 2) * (-8);
        symbolTable[dclName] = {"long", offset};

        load_constant("x0", numValue);
        push("x0");
        ++frameSlots;
    }
}

void generate_lvalue_address(Node* node, const map<string, SymbolInfo>& symbolTable) {
    if (node->value == "lvalue ID") {
        string name = get_id_name(node->children[0]);
        int offset = symbolTable.at(name).offset;

        if (offset >= 0) {
            load_constant("x1", to_string(offset));
            cout << "add x0, x29, x1" << endl;
        }
        else {
            load_constant("x1", to_string(-offset));
            cout << "sub x0, x29, x1" << endl;
        }
    }
    else if (node->value == "lvalue LPAREN lvalue RPAREN") {
        generate_lvalue_address(node->children[1], symbolTable);
    }
}

void generate_test(Node* node, const map<string, SymbolInfo>& symbolTable, const string& falseLabel) {
    // Note: False Label is the label to branch to when condition is false

    generate_expr(node->children[0], symbolTable);
    push("x0");

    generate_expr(node->children[2], symbolTable);

    pop("x1");

    cout << "cmp x1, x0" << endl;
    
    if (node->value == "test expr EQ expr") {
        cout << "b.ne " << falseLabel << endl;
    }
    else if (node->value == "test expr NE expr") {
        cout << "b.eq " << falseLabel << endl;
    }
    else if (node->value == "test expr LT expr") {
        cout << "b.ge " << falseLabel << endl;
    }
    else if (node->value == "test expr LE expr") {
        cout << "b.gt " << falseLabel << endl;
    }
    else if (node->value == "test expr GE expr") {
        cout << "b.lt " << falseLabel << endl;
    }
    else if (node->value == "test expr GT expr") {
        cout << "b.le " << falseLabel << endl;
    }
}

void generate_statement(Node* node, const map<string, SymbolInfo>& symbolTable) {
    if (node->value == "statement lvalue BECOMES expr SEMI") {
        generate_lvalue_address(node->children[0], symbolTable);

        push("x0");

        generate_expr(node->children[2], symbolTable);

        pop("x1");

        cout << "stur x0, [x1, 0]" << endl;
    }
    else if(node->value == "statement IF LPAREN test RPAREN LBRACE statements RBRACE ELSE LBRACE statements RBRACE") {
        string else_start = make_label("elseStart");
        string else_end = make_label("elseEnd");

        generate_test(node->children[2], symbolTable, else_start);
        generate_statements(node->children[5], symbolTable);
        cout << "b " << else_end << endl;

        cout << else_start << ":" << endl;
        generate_statements(node->children[9], symbolTable);
        cout << else_end << ":" << endl;
    }
    else if (node->value == "statement WHILE LPAREN test RPAREN LBRACE statements RBRACE") {
        string while_start = make_label("whileStart");
        string while_end = make_label("whileEnd");

        cout << while_start << ":" << endl;
        generate_test(node->children[2], symbolTable, while_end);
        generate_statements(node->children[5], symbolTable);
        cout << "b " << while_start << endl;

        cout << while_end << ":" << endl;
    }
    else if (node->value == "statement PUTCHAR LPAREN expr RPAREN SEMI") {
        generate_expr(node->children[2], symbolTable);
        cout << "stur x0, [x12, 0]" << endl;
    }
    else if (node->value == "statement PRINTLN LPAREN expr RPAREN SEMI") {
        generate_expr(node->children[2], symbolTable);
        cout << "blr x10" << endl;
    }
}

void generate_statements(Node* node, const map<string, SymbolInfo>& symbolTable) {
    if (node->value == "statements .EMPTY") {
        return;
    }
    else if (node->value == "statements statements statement") {
        generate_statements(node->children[0], symbolTable);
        generate_statement(node->children[1], symbolTable);
    }
}

void generate_main(Node* node) {

    map<string, SymbolInfo> symbolTable;
    int frameSlots = 0;

    // load constants
    load_constant("x8", "8");
    cout << "sub x20, x20, x20" << endl;
    load_constant("x10", "print");
    load_constant("x11", "0xc000000000010000");
    load_constant("x12", "0xc000000000010008");

    // Get arguments and their names
    Node* firstDcl = node->children[3];
    Node* secondDcl = node->children[5];

    string firstDclName = get_id_name(firstDcl->children[1]);
    string secondDclName = get_id_name(secondDcl->children[1]);

    // Store x30
    push("x30");
    ++frameSlots;

    // Push Arguments to stack
    symbolTable[firstDclName] = {"long", 8};
    push("x0");
    ++frameSlots;

    symbolTable[secondDclName] = {"long", 0};
    push("x1");
    ++frameSlots;

    // Initialize Frame Pointer 
    cout << "add x29, sp, xzr" << endl;

    generate_dcls(node->children[8], symbolTable, frameSlots);

    generate_statements(node->children[9], symbolTable);

    generate_expr(node->children[11], symbolTable);

    cout << "ldur x30, [x29, 16]" << endl;

    // Move sp back to start
    for (int i = 0; i < frameSlots; ++i) {
        cout << "add sp, sp, x8" << endl;
    }

    cout << "br x30" << endl;
}

void generate(Node* node) {
    if (node->value == "start BOF procedures EOF") {
        generate(node->children[1]);
    }
    else if (node->value == "procedures main") {
        generate_main(node->children[0]);
    }
}

void delete_tree(Node* node) {
    if (node == nullptr) return;

    for (Node* child : node->children) {
        delete_tree(child);
    }

    delete node;
}

int main() {

    Node* root = build_tree();

    cout << ".import print" << endl; 
    generate(root);

    delete_tree(root);
}