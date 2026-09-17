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
void generate_lvalue_address(Node* node, const map<string, SymbolInfo>& symbolTable);

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

string get_num_value(Node* numNode) {
    string kind;
    string num;

    stringstream ss(numNode->value);
    ss >> kind >> num;

    return num;
}

string get_dcl_type(Node* dclNode) {
    Node* typeNode = dclNode->children[0];

    if (typeNode->value == "type LONG") {
        return "long";
    }
    else if (typeNode->value == "type LONG STAR") {
        return "long*";
    }

    return "";
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

int generate_arglist(Node* node, const map<string, SymbolInfo>& symbolTable) {
    generate_expr(node->children[0], symbolTable);
    push("x0");

    if (node->value == "arglist expr") {
        return 1;
    }
    if (node->value == "arglist expr COMMA arglist") {
        return 1 + generate_arglist(node->children[2], symbolTable);
    }
    return 0;
}

void generate_call(const string& procedureName, Node* arglist, const map<string, SymbolInfo>& symbolTable) {
    push("x29");
    push("x30");

    int argCount = 0;
    if (arglist != nullptr) {
        argCount = generate_arglist(arglist, symbolTable);
    }

    load_constant("x1", "F" + procedureName);
    cout << "blr x1" << endl;

    for (int i = 0; i < argCount; ++i) {
        cout << "add sp, sp, x8" << endl;
    }

    pop("x30");
    pop("x29");
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
    else if (node->value == "factor ID LPAREN RPAREN") {
        string procedureName = get_id_name(node->children[0]);
        generate_call(procedureName, nullptr, symbolTable);
    }
    else if (node->value == "factor ID LPAREN arglist RPAREN") {
        string procedureName = get_id_name(node->children[0]);
        generate_call(procedureName, node->children[2], symbolTable);
    }
    else if (node->value == "factor NULL") {
        cout << "add x0, x6, xzr" << endl;
    }
    else if (node->value == "factor STAR factor") {
        generate_factor(node->children[1], symbolTable);
        cout << "ldur x0, [x0, 0]" << endl;
    }
    else if (node->value == "factor AMP lvalue") {
        generate_lvalue_address(node->children[1], symbolTable);
    }
    else if (node->value == "factor NEW LONG LBRACK expr RBRACK") {
        generate_expr(node->children[3], symbolTable);

        push("x30");
        load_constant("x1", "new");
        cout << "blr x1" << endl;
        pop("x30");

        string successLabel = make_label("newSuccess");

        cout << "cmp x0, xzr" << endl;
        cout << "b.ne " << successLabel << endl;
        cout << "add x0, x6, xzr" << endl;
        cout << successLabel << ":" << endl;
    }
}

void generate_expr(Node* node, const map<string, SymbolInfo>& symbolTable) {
    if (node->value == "expr term") {
        generate_term(node->children[0], symbolTable);
    }
    else if (node->value == "expr expr PLUS term") {

        string leftType = node->children[0]->type;
        string rightType = node->children[2]->type;

        generate_expr(node->children[0], symbolTable);
        push("x0");

        generate_term(node->children[2], symbolTable);

        pop("x1");

        if (leftType == "long*" && rightType == "long") {
            cout << "mul x0, x8, x0" << endl;
        }
        else if (leftType == "long" && rightType == "long*") {
            cout << "mul x1, x8, x1" << endl;
        }

        cout << "add x0, x1, x0" << endl;
    }
    else if (node->value == "expr expr MINUS term") {

        string leftType = node->children[0]->type;
        string rightType = node->children[2]->type;

        generate_expr(node->children[0], symbolTable);
        push("x0");

        generate_term(node->children[2], symbolTable);

        pop("x1");

        if (leftType == "long*" && rightType == "long") {
            cout << "mul x0, x8, x0" << endl;
        }

        cout << "sub x0, x1, x0" << endl;

        if (leftType == "long*" && rightType == "long*") {
            cout << "sdiv x0, x0, x8" << endl;
        }
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

void generate_dcls(Node* node, map<string, SymbolInfo>& symbolTable, int& localSlots) {
    if (node->value == "dcls .EMPTY") {
        return;
    }

    if (node->value == "dcls dcls dcl BECOMES NUM SEMI") {
        generate_dcls(node->children[0], symbolTable, localSlots);

        Node* dclNode = node->children[1];
        Node* numNode = node->children[3];

        string dclName = get_id_name(dclNode->children[1]);
        string dclType = get_dcl_type(dclNode);
        string numValue = get_num_value(numNode);

        int offset = (localSlots + 1) * (-8);
        symbolTable[dclName] = {dclType, offset};

        load_constant("x0", numValue);
        push("x0");
        ++localSlots;
    }
    else if (node->value == "dcls dcls dcl BECOMES NULL SEMI") {
        generate_dcls(node->children[0], symbolTable, localSlots);

        Node* dclNode = node->children[1];

        string dclName = get_id_name(dclNode->children[1]);
        string dclType = get_dcl_type(dclNode);

        int offset = (localSlots + 1) * (-8);
        symbolTable[dclName] = {dclType, offset};

        cout << "add x0, x6, xzr" << endl;
        push("x0");

        ++localSlots;
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
    else if (node->value == "lvalue STAR factor") {
            generate_factor(node->children[1], symbolTable);
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
        if (node->children[0]->type == "long*") {
            cout << "b.hs " << falseLabel << endl;
        }
        else {
            cout << "b.ge " << falseLabel << endl;
        }
    }
    else if (node->value == "test expr LE expr") {
        if (node->children[0]->type == "long*") {
            cout << "b.hi " << falseLabel << endl;
        }
        else {
            cout << "b.gt " << falseLabel << endl;
        }
    }
    else if (node->value == "test expr GE expr") {
        if (node->children[0]->type == "long*") {
            cout << "b.lo " << falseLabel << endl;
        }
        else {
            cout << "b.lt " << falseLabel << endl;
        }

    }
    else if (node->value == "test expr GT expr") {
        if (node->children[0]->type == "long*") {
            cout << "b.ls " << falseLabel << endl;
        }
        else {
            cout << "b.le " << falseLabel << endl;
        }
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
        push("x30");
        cout << "blr x10" << endl;
        pop("x30");
    }
    else if (node->value == "statement DELETE LBRACK RBRACK expr SEMI") {
        generate_expr(node->children[3], symbolTable);

        string skipDeleteLabel = make_label("skipDeleteLabel");

        cout << "cmp x0, x6" << endl;
        cout << "b.eq " << skipDeleteLabel << endl;

        push("x30");
        load_constant("x1", "delete");
        cout << "blr x1" << endl;
        pop("x30");

        cout << skipDeleteLabel << ":" << endl;

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
    int localSlots = 0;

    // load constants
    load_constant("x8", "8");
    cout << "sub x20, x20, x20" << endl;
    load_constant("x6", "-65536");
    load_constant("x10", "print");
    load_constant("x11", "0xc000000000010000");
    load_constant("x12", "0xc000000000010008");

    // Get arguments and their names
    Node* firstDcl = node->children[3];
    Node* secondDcl = node->children[5];

    string firstDclName = get_id_name(firstDcl->children[1]);
    string secondDclName = get_id_name(secondDcl->children[1]);

    string firstDclType = get_dcl_type(firstDcl);
    string secondDclType = get_dcl_type(secondDcl);

    // Store x30
    push("x30");

    // Push Arguments to stack
    symbolTable[firstDclName] = {firstDclType, 8};
    push("x0");

    symbolTable[secondDclName] = {secondDclType, 0};
    push("x1");

    // Initialize Frame Pointer 
    cout << "add x29, sp, xzr" << endl;


    if (firstDclType == "long") {
        cout << "sub x1, x1, x1" << endl;
    }

    push("x30");
    load_constant("x5", "init");
    cout << "blr x5" << endl;
    pop("x30");

    generate_dcls(node->children[8], symbolTable, localSlots);

    generate_statements(node->children[9], symbolTable);

    generate_expr(node->children[11], symbolTable);

    cout << "ldur x30, [x29, 16]" << endl;

    // Move sp back to start
    for (int i = 0; i < localSlots + 3; ++i) {
        cout << "add sp, sp, x8" << endl;
    }

    cout << "br x30" << endl;
}

void collect_paramlist(Node* node, vector<Node*>& parameters) {
    parameters.push_back(node->children[0]);

    if (node->value == "paramlist dcl COMMA paramlist") {
        collect_paramlist(node->children[2], parameters);
    }
}

void collect_params(Node* node, vector<Node*>& parameters) {
    if (node->value == "params .EMPTY") {
        return;
    }
    else if (node->value == "params paramlist") {
        collect_paramlist(node->children[0], parameters);
    }
}

void generate_procedure(Node* node) {
    map<string, SymbolInfo> symbolTable;
    vector<Node*> parameters;

    string procedureName = get_id_name(node->children[1]);
    collect_params(node->children[3], parameters);

    cout << "F" << procedureName << ":" << endl;
    cout << "add x29, sp, xzr" << endl;

    int parameterCount = parameters.size();

    for (int i = 0; i < parameterCount; ++i) {

        Node* dclNode = parameters[i];

        string parameterName = get_id_name(dclNode->children[1]);
        string parameterType = get_dcl_type(dclNode);

        int offset = (parameterCount-1-i)*8;

        symbolTable[parameterName] = {parameterType, offset};
    }

    int localSlots = 0;

    generate_dcls(node->children[6], symbolTable, localSlots);

    generate_statements(node->children[7], symbolTable);

    generate_expr(node->children[9], symbolTable);

    for (int i = 0; i < localSlots; ++i) {
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
    else if (node->value == "procedures procedure procedures") {
        generate(node->children[1]);
        generate_procedure(node->children[0]);
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
    cout << ".import init" << endl; 
    cout << ".import new" << endl; 
    cout << ".import delete" << endl; 

    
    generate(root);

    delete_tree(root);
}
