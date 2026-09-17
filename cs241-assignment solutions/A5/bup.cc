#include <iostream>
#include <vector>
#include <string>
#include <sstream>
#include <deque>

using namespace std;

// g++ -g -O0 -fPIC -std=gnu++20 -Wall -Wextra -pedantic-errors -Wno-unused-parameter -o slr slr.cc

struct Rule {
    string lhs;
    vector<string> rhs;
};

vector<Rule> rules;
vector<string> reduction;
deque<string> input;

void perform_shift() {
    string token = input.front();
    input.pop_front();
    reduction.push_back(token);
}

void perform_reduction(int n) {
    for (size_t i = 0; i < rules[n].rhs.size(); i++) {
        reduction.pop_back();
    }
    reduction.push_back(rules[n].lhs);
    return;
}

void perform_print() {
    for (const auto& token: reduction) {
        cout << token << " ";
    }
    cout << ".";
    for (const auto& token: input) {
        cout << " " << token;
    }
    cout << endl;
}


int main() {

    string line;
    string s;

    // CFG input section
    getline(cin, line);

    while (getline(cin, line)) {

        if (line == ".INPUT") {
            break;
        }

        stringstream ss(line);

        ss >> s;
        Rule r {s, vector<string>()};

        while (ss >> s) {
            if (s != ".EMPTY") {
                r.rhs.push_back(s);
            }
        }
        rules.push_back(r);
    }

    // Input
    while (getline(cin, line)) {

        if (line == ".ACTIONS") {
            break;
        }

        stringstream ss(line);
        while (ss >> s) {
            input.push_back(s);
        }
    }

    // Actions
    while (getline(cin, line)) {

        if (line == ".END") {
            break;
        }

        stringstream ss(line);
        ss >> s;

        if (s == "shift") {
            perform_shift();
        }
        else if (s == "reduce") {
            int n;
            ss >> n;
            perform_reduction(n);
        }
        else if (s == "print") {
            perform_print();
        }

    }


}