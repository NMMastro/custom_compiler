#include <iostream>
#include <vector>
#include <string>
#include <sstream>
#include <stack>
#include <map>
#include <deque>

using namespace std;

// g++ -g -O0 -fPIC -std=gnu++20 -Wall -Wextra -pedantic-errors -Wno-unused-parameter -o bup bup.cc

// rules
struct Rule {
    string lhs;
    vector<string> rhs;
};
vector<Rule> rules;

map<pair<int, string>, int> transitions;
map<pair<int, string>, int> reductions;
map<int, int> acceptReductions;

stack<int> states;
vector<string> symbolStack;
deque<string> input;


void perform_shift() {
    string token = input.front();
    input.pop_front();
    symbolStack.push_back(token);
}

void perform_reduction(int n) {
    for (size_t i = 0; i < rules[n].rhs.size(); i++) {
        symbolStack.pop_back();
        states.pop();
    }
    symbolStack.push_back(rules[n].lhs);
    return;
}

void perform_print() {
    for (const auto& token: symbolStack) {
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

        if (line == ".TRANSITIONS") {
            break;
        }

        stringstream ss(line);
        while (ss >> s) {
            input.push_back(s);
        }
    }

    // Transitions
    while (getline(cin, line)) {

        if (line == ".REDUCTIONS") {
            break;
        }

        stringstream ss(line);

        int from_state;
        int to_state;

        ss >> from_state >> s >> to_state;

        transitions[make_pair(from_state, s)] = to_state;
    }

    // Reductions
    while (getline(cin, line)) {

        if (line == ".END") {
            break;
        }

        stringstream ss(line);

        int from_state;
        int rule_number;

        ss >> from_state >> rule_number >> s;

        if (s == ".ACCEPT") {
            acceptReductions[from_state] = rule_number;
        }
        else {
            reductions[make_pair(from_state, s)] = rule_number;
        }
    }

    int shifted = 0;

    states.push(0);
    perform_print();
    while (true) {
        int current_state = states.top();

        if (acceptReductions.count(current_state) > 0) {
            int rule_number = acceptReductions[current_state];
            perform_reduction(rule_number);

            perform_print();
            break;
        }

        if (input.empty()) {
            cerr << "ERROR at " << shifted + 1 << endl;
            break;
        }

        string next = input.front();

        if (reductions.count({current_state, next}) > 0) {
            int rule_number = reductions[{current_state, next}];
            perform_reduction(rule_number);

            int next_state = transitions[{states.top(), rules[rule_number].lhs}];
            states.push(next_state);

            perform_print();
        }
        else if (transitions.count({current_state, next}) > 0) {
            int to_state = transitions[{current_state, next}];
            states.push(to_state);

            perform_shift();
            shifted++;

            perform_print();
        }
        else {
            cerr << "ERROR at " << shifted + 1 << endl;
            break;
        }
    }

}