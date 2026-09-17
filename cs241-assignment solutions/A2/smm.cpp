#include <iostream>
#include <sstream>
#include <string>
#include <vector>
#include <set>
#include <map>
const std::string ALPHABET    = ".ALPHABET";
const std::string STATES      = ".STATES";
const std::string TRANSITIONS = ".TRANSITIONS";
const std::string INPUT       = ".INPUT";
const std::string EMPTY       = ".EMPTY";

bool isChar(std::string s) {
  return s.length() == 1;
}
bool isRange(std::string s) {
  return s.length() == 3 && s[1] == '-';
}

// Locations in the program that you should modify to store the
// DFA information have been marked with four-slash comments:
//// (Four-slash comment)
int main() {
  std::istream& in = std::cin;
  std::string s;

  std::getline(in, s); // Alphabet section (skip header)
  // Read characters or ranges separated by whitespace
  while(in >> s) {
    if (s == STATES) { 
      break; 
    } else {
      if (isChar(s)) {
        //// Variable 's[0]' is an alphabet symbol
      } else if (isRange(s)) {
        for(char c = s[0]; c <= s[2]; ++c) {
          //// Variable 'c' is an alphabet symbol
        }
      } 
    }
  }

  std::string startingState = "";
  std::set<std::string> accepting_states;

  std::getline(in, s); // States section (skip header)
  // Read states separated by whitespace
  while(in >> s) {
    if (s == TRANSITIONS) { 
      break; 
    } else {
      static bool initial = true;
      bool accepting = false;
      if (s.back() == '!' && !isChar(s)) {
        accepting = true;
        s.pop_back();
      }
      //// Variable 's' contains the name of a state
      if (initial) {
        startingState = s;
        //// The state is initial
        initial = false;
      }

      if(accepting) {
        accepting_states.insert(s);
      }
    }
  }


  std::map<std::pair<std::string, char>, std::string> transitions; // format: map< pair<State1, transitionChar>, State2>

  std::getline(in, s); // Transitions section (skip header)
  // Read transitions line-by-line
  while(std::getline(in, s)) {
    if (s == INPUT) { 
      // Note: Since we're reading line by line, once we encounter the
      // input header, we will already be on the line after the header
      break; 
    } else {
      std::string fromState, symbols, toState;
      std::istringstream line(s);
      std::vector<std::string> lineVec;
      while(line >> s) {
        lineVec.push_back(s);
      }
      fromState = lineVec.front();
      toState = lineVec.back();
      for(int i = 1; i < lineVec.size()-1; ++i) {
        std::string s = lineVec[i];
        if (isChar(s)) {
          symbols += s;
        } else if (isRange(s)) {
          for(char c = s[0]; c <= s[2]; ++c) {
            symbols += c;
          }
        }
      }
      for ( char c : symbols ) {
          //// There is a transition from 'fromState' to 'toState' on 'c'
          transitions[std::make_pair(fromState, c)] = toState;
      }
    }
  }

  // Input section (already skipped header)

  s = "";
  std::getline(in, s);

  for (int i = 0; i < s.size();) {
      std::string currentToken = "";
      std::string currentState = startingState;

      while (i < s.size()) {
        std::pair key = std::make_pair(currentState, s[i]);

        if (transitions.find(key) == transitions.end()) {
            break;
        }

        currentState = transitions[key];
        currentToken += s[i];
        ++i;
      }


      if (accepting_states.count(currentState) > 0) {
        std::cout << currentToken << std::endl;
      }
      else {
        std::cerr << "ERROR" << std::endl;
        return 1;
      }
  }
  
}