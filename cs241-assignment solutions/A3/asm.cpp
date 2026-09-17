#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <map>
#include <cstdint>
#include <vector>
#include <stdexcept>


// my dictionaries
const std::map<std::string, int> para_count = {
    {"br", 1},
    {"blr", 1},
    {"add", 5},
    {"sub", 5},
    {"mul", 5},
    {"sdiv", 5},
    {"udiv", 5},
    {"smulh", 5},
    {"umulh", 5},
    {"cmp", 3},
    {"ldr", 3},
    {"ldur", 7},
    {"stur", 7},
    {"b", 1}
};

const std::map<std::string, std::pair<uint32_t, uint32_t>> hash = {
    {"add",   {0b10001011001, 0b011000}},
    {"sub",   {0b11001011001, 0b011000}},
    {"mul",   {0b10011011000, 0b011111}},
    {"smulh", {0b10011011010, 0b011111}},
    {"umulh", {0b10011011110, 0b011111}},
    {"sdiv",  {0b10011010110, 0b000011}},
    {"udiv",  {0b10011010110, 0b000010}},
    {"cmp",   {0b11101011001, 0b011000}},
    {"br",    {0b11010110000, 0b000000}},
    {"blr",   {0b11010110001, 0b000000}},
    {"ldur",  {0b11111000010, 0b00}},
    {"stur",  {0b11111000000, 0b00}},
    {"ldr",   {0b01011000,    0b00}},
    {"b",     {0b000101,      0b00}}
};

const std::map<std::string, uint32_t> cond_map = {
    {".eq", 0b00000},
    {".ne", 0b00001},

    {".hs", 0b00010},
    {".lo", 0b00011},

    {".hi", 0b01000},
    {".ls", 0b01001},

    {".ge", 0b01010},
    {".lt", 0b01011},

    {".gt", 0b01100},
    {".le", 0b01101}
};


// quick helpers
bool checkReg(int r) {
    return r >= 0 && r <= 30;
}

bool checkRegOrXzr(int r) {
    return r >= 0 && r <= 31;
}

bool inRange(int value, int min, int max) {
    return value >= min && value <= max;
}

void binaryOutput(std::ostream& out, uint32_t binary)
{
    out << (char)((binary >> 0) & 0xFF)
        << (char)((binary >> 8) & 0xFF)
        << (char)((binary >> 16) & 0xFF)
        << (char)((binary >> 24) & 0xFF);
}

void emit8Byte(std::ostream& out, int64_t value)
{
    uint64_t uvalue = static_cast<uint64_t>(value);

    out << (char)((uvalue >> 0) & 0xFF)
        << (char)((uvalue >> 8) & 0xFF)
        << (char)((uvalue >> 16) & 0xFF)
        << (char)((uvalue >> 24) & 0xFF)
        << (char)((uvalue >> 32) & 0xFF)
        << (char)((uvalue >> 40) & 0xFF)
        << (char)((uvalue >> 48) & 0xFF)
        << (char)((uvalue >> 56) & 0xFF);
}




/** Prints an error to stderr with an "ERROR: " prefix, and newline suffix. Terminates the program with an error.
 *
 * @param message The error to print
 */
void formatError(const std::string & message)
{
    throw std::runtime_error(message);
}

enum TokenType {
    // Not a real token type we output: a unique value for initializing a TokenType when the
    // actual value is unknown
    NONE,

    DOTID,
    LABEL,
    ID,
    HEXINT,
    REG,
    ZREG,
    INT,
    COMMA,
    LBRACK,
    RBRACK,
    NEWLINE
};

struct Token
{
    TokenType type;
    std::string lexeme;
};

#define TOKEN_TYPE_READER(t) if(s == #t) return t
TokenType stringToTokenType(const std::string & s)
{
    TOKEN_TYPE_READER(DOTID);
    TOKEN_TYPE_READER(LABEL);
    TOKEN_TYPE_READER(ID);
    TOKEN_TYPE_READER(HEXINT);
    TOKEN_TYPE_READER(REG);
    TOKEN_TYPE_READER(ZREG);
    TOKEN_TYPE_READER(INT);
    TOKEN_TYPE_READER(COMMA);
    TOKEN_TYPE_READER(LBRACK);
    TOKEN_TYPE_READER(RBRACK);
    TOKEN_TYPE_READER(NEWLINE);
    return NONE;
}
#undef TOKEN_TYPE_READER

#define TOKEN_TYPE_PRINTER(t) case t: return #t
std::string tokenTypeToString(const TokenType & t)
{
    switch (t) {
        TOKEN_TYPE_PRINTER(DOTID);
        TOKEN_TYPE_PRINTER(LABEL);
        TOKEN_TYPE_PRINTER(ID);
        TOKEN_TYPE_PRINTER(HEXINT);
        TOKEN_TYPE_PRINTER(REG);
        TOKEN_TYPE_PRINTER(ZREG);
        TOKEN_TYPE_PRINTER(INT);
        TOKEN_TYPE_PRINTER(COMMA);
        TOKEN_TYPE_PRINTER(LBRACK);
        TOKEN_TYPE_PRINTER(RBRACK);
        TOKEN_TYPE_PRINTER(NEWLINE);
        default:
            formatError("Unrecognized token type");
            return "";
    }
    return "NONE";
}
#undef TOKEN_TYPE_PRINTER

std::ostream & operator<<(std::ostream & out, const Token token)
{
    out << tokenTypeToString(token.type) << " " << token.lexeme;
    return out;
}

std::istream & operator>>(std::istream & in, Token& token)
{
    std::string tokenType;
    in >> tokenType;
    token.type = stringToTokenType(tokenType);
    if (token.type != NEWLINE) {
        in >> token.lexeme;
    } else {
        token.lexeme = "";
    }
    return in;
}

// My custom functions lets go
std::vector<std::vector<Token>> tokensToLines(std::vector<Token> tokens) {
    std::vector<std::vector<Token>> lines_vector;
    std::vector<Token> cur_line; 

    for (const Token &token : tokens) {
        if (token.type == NEWLINE) {
            lines_vector.push_back(cur_line);
            cur_line.clear();
        } else { 
            cur_line.push_back(token);
        }
    }

    if (!cur_line.empty()) {
        lines_vector.push_back(cur_line); 
    }

    return lines_vector; 
}


bool compileLine(uint32_t &word,
                 const std::string &instruction,
                 int one,
                 int two,
                 int three)
{
    if (hash.count(instruction) == 0) {
        formatError("unknown instruction: " + instruction);
    }

    // add x0, x1, x2
    // sub x0, x1, x2
    // mul x0, x1, x2
    // sdiv x0, x1, x2
    // etc.
    if (instruction == "add" || instruction == "sub" ||
        instruction == "mul" || instruction == "sdiv" ||
        instruction == "udiv" || instruction == "smulh" ||
        instruction == "umulh") {

        if (!checkRegOrXzr(one) || !checkRegOrXzr(two) || !checkRegOrXzr(three)) {
            formatError("register out of range in " + instruction);
        }

        word = (hash.at(instruction).first << 21)
             | (three << 16)
             | (hash.at(instruction).second << 10)
             | (two << 5)
             | one;

        return true;
    }

    // cmp x0, x1
    if (instruction == "cmp") {
        if (!checkRegOrXzr(one) || !checkRegOrXzr(two)) {
            formatError("register out of range in cmp");
        }

        word = (hash.at(instruction).first << 21)
             | (two << 16)
             | (hash.at(instruction).second << 10)
             | (one << 5)
             | 0b11111;

        return true;
    }

    // br x30
    // blr x5
    if (instruction == "br" || instruction == "blr") {
        if (!checkRegOrXzr(one)) {
            formatError("register out of range in " + instruction);
        }

        word = (hash.at(instruction).first << 21)
             | (0b11111 << 16)
             | (hash.at(instruction).second << 10)
             | (one << 5)
             | 0b00000;

        return true;
    }

    // ldur x0, [x1, 8]
    // stur x0, [x1, 8]
    if (instruction == "ldur" || instruction == "stur") {
        if (!checkRegOrXzr(one) || !checkRegOrXzr(two)) {
            formatError("register out of range in " + instruction);
        }

        if (!inRange(three, -256, 255)) {
            formatError("immediate out of range in " + instruction);
        }

        word = (hash.at(instruction).first << 21)
             | ((three & 0x1FF) << 12)
             | (hash.at(instruction).second << 10)
             | (two << 5)
             | one;

        return true;
    }

    // ldr x0, 8
    if (instruction == "ldr") {
        if (!checkReg(one)) {
            formatError("register out of range in ldr");
        }

        if (two % 4 != 0) {
            formatError("ldr immediate must be divisible by 4");
        }

        if (!inRange(two, -1048576, 1048572)) {
            formatError("immediate out of range in ldr");
        }

        word = (hash.at(instruction).first << 24)
             | (((two >> 2) & 0x7FFFF) << 5)
             | (one & 0x1F);

        return true;
    }

    // b 12
    if (instruction == "b") {
        if (one % 4 != 0) {
            formatError("branch immediate must be divisible by 4");
        }

        if (!inRange(one, -134217728, 134217724)) {
            formatError("branch immediate out of range");
        }

        word = (hash.at(instruction).first << 26)
             | ((one >> 2) & 0x03FFFFFF);

        return true;
    }

    formatError("could not compile instruction: " + instruction);
    return false;
}

bool compileCondBranch(uint32_t &word, int offset, uint32_t condCode)
{
    if (offset % 4 != 0) {
        formatError("conditional branch immediate must be divisible by 4");
    }

    if (!inRange(offset, -1048576, 1048572)) {
        formatError("conditional branch immediate out of range");
    }

    int encodedImmediate = offset >> 2;

    word = (0b01010100 << 24)
         | ((encodedImmediate & 0x7FFFF) << 5)
         | (condCode & 0xF);

    return true;
}

int readRegisterToken(const Token& token, bool allowXzr, bool allowSp)
{
    if (token.type == ZREG) {
        if (!allowXzr) {
            formatError("xzr not allowed here");
        }
        return 31;
    }

    if (token.type == ID && token.lexeme == "sp") {
        if (!allowSp) {
            formatError("sp not allowed here");
        }
        return 31;
    }

    if (token.type != REG) {
        formatError("expected register");
    }

    return std::stoi(token.lexeme.substr(1));
}

int64_t readImmediateToken(const Token& token,
                           const std::map<std::string, uint32_t>& labels)
{
    if (token.type == INT) {
        return std::stoll(token.lexeme);
    }

    if (token.type == HEXINT) {
        return static_cast<int64_t>(
            std::stoull(token.lexeme.substr(2), nullptr, 16)
        );
    }

    if (token.type == ID) {
        if (labels.count(token.lexeme) == 0) {
            formatError("undefined label: " + token.lexeme);
        }

        return static_cast<int64_t>(labels.at(token.lexeme));
    }

    formatError("expected immediate or label");
    return 0;
}

void firstPass(const std::vector<std::vector<Token>>& lines,
               std::map<std::string, uint32_t>& labels,
               std::vector<std::string>& labelOrder)
{
    uint32_t address = 0;

    for (const std::vector<Token>& line : lines) {
        if (line.empty()) {
            continue;
        }

        size_t i = 0;

        // Labels can only appear at the beginning of a line.
        while (i < line.size() && line[i].type == LABEL) {
            std::string labelName = line[i].lexeme.substr(0, line[i].lexeme.size() - 1);

            if (labels.count(labelName) > 0) {
                formatError("duplicate label: " + labelName);
            }

            labels[labelName] = address;
            labelOrder.push_back(labelName);

            i++;
        }

        // Line was only labels.
        if (i == line.size()) {
            continue;
        }

        // No labels allowed after instruction/directive begins.
        for (size_t j = i; j < line.size(); j++) {
            if (line[j].type == LABEL) {
                formatError("label appears after instruction/directive");
            }
        }

        if (line[i].type == DOTID) {
            if (line[i].lexeme != ".8byte") {
                formatError("unknown directive: " + line[i].lexeme);
            }

            address += 8;
        }
        else if (line[i].type == ID) {
            if (para_count.count(line[i].lexeme) == 0) {
                formatError("unknown instruction: " + line[i].lexeme);
            }

            address += 4;
        }
        else {
            formatError("line must start with label, instruction, or .8byte");
        }
    }
}

void binaryPass(const std::vector<std::vector<Token>>& lines,
                const std::map<std::string, uint32_t>& labels,
                std::ostream& out)
{
    uint32_t address = 0;

    for (const std::vector<Token>& line : lines) {
        if (line.empty()) {
            continue;
        }

        size_t i = 0;

        // Skip leading labels.
        while (i < line.size() && line[i].type == LABEL) {
            i++;
        }

        // Line was only labels.
        if (i == line.size()) {
            continue;
        }

        // .8byte case
        if (line[i].type == DOTID) {
            if (line[i].lexeme != ".8byte") {
                formatError("unknown directive: " + line[i].lexeme);
            }

            if (i + 2 != line.size()) {
                formatError(".8byte takes exactly one argument");
            }

            int64_t value = readImmediateToken(line[i + 1], labels);
            emit8Byte(out, value);

            address += 8;
            continue;
        }

        // Must be instruction from here.
        if (line[i].type != ID) {
            formatError("expected instruction or .8byte");
        }

        std::string instruction = line[i].lexeme;

        if (para_count.count(instruction) == 0) {
            formatError("unknown instruction: " + instruction);
        }

        uint32_t binary = 0;

        // add/sub/mul/etc.
        if (para_count.at(instruction) == 5) {
            if (i + 6 != line.size()) {
                formatError(instruction + " takes exactly three registers");
            }

            if (line[i + 2].type != COMMA || line[i + 4].type != COMMA) {
                formatError("expected commas in " + instruction);
            }

            int one = readRegisterToken(line[i + 1], false, true);
            int two = readRegisterToken(line[i + 3], false, true);
            int three = readRegisterToken(line[i + 5], true, false);
            compileLine(binary, instruction, one, two, three);
            binaryOutput(out, binary);

            address += 4;
            continue;
        }

        // br / blr / b / b.cond
        if (para_count.at(instruction) == 1) {
            // b.cond case: ID b, DOTID .eq, target
            if (instruction == "b" &&
                i + 3 == line.size() &&
                line[i + 1].type == DOTID) {

                std::string cond = line[i + 1].lexeme;

                if (cond_map.count(cond) == 0) {
                    formatError("invalid condition code: " + cond);
                }

                int target = static_cast<int>(readImmediateToken(line[i + 2], labels));

                // If target is a label, convert absolute address to relative offset.
                if (line[i + 2].type == ID) {
                    target = target - static_cast<int>(address);
                }

                compileCondBranch(binary, target, cond_map.at(cond));
                binaryOutput(out, binary);

                address += 4;
                continue;
            }

            // br x30 / blr x5
            if (instruction == "br" || instruction == "blr") {
                if (i + 2 != line.size()) {
                    formatError(instruction + " takes exactly one register");
                }

                int one = readRegisterToken(line[i + 1], false, false);

                compileLine(binary, instruction, one, 0, 0);
                binaryOutput(out, binary);

                address += 4;
                continue;
            }

            // plain b target
            if (instruction == "b") {
                if (i + 2 != line.size()) {
                    formatError("b takes exactly one argument");
                }

                int target = static_cast<int>(readImmediateToken(line[i + 1], labels));

                // If target is a label, convert absolute address to relative offset.
                if (line[i + 1].type == ID) {
                    target = target - static_cast<int>(address);
                }

                compileLine(binary, instruction, target, 0, 0);
                binaryOutput(out, binary);

                address += 4;
                continue;
            }
        }

        // cmp x0, x1
        // ldr x0, target
        if (para_count.at(instruction) == 3) {
            if (i + 4 != line.size()) {
                formatError(instruction + " has wrong number of tokens");
            }

            if (line[i + 2].type != COMMA) {
                formatError("expected comma in " + instruction);
            }

            if (instruction == "cmp") {
                int one = readRegisterToken(line[i + 1], false, true);
                int two = readRegisterToken(line[i + 3], true, false);
                compileLine(binary, instruction, one, two, 0);
                binaryOutput(out, binary);

                address += 4;
                continue;
            }

            if (instruction == "ldr") {
                int one = readRegisterToken(line[i + 1], false, false);
                int target = static_cast<int>(readImmediateToken(line[i + 3], labels));

                // For ldr labels, make it PC-relative.
                if (line[i + 3].type == ID) {
                    target = target - static_cast<int>(address);
                }

                compileLine(binary, instruction, one, target, 0);
                binaryOutput(out, binary);

                address += 4;
                continue;
            }
        }

        // ldur/stur x0, [x1, imm]
        if (para_count.at(instruction) == 7) {
            if (i + 8 != line.size()) {
                formatError(instruction + " has wrong number of tokens");
            }

            if (line[i + 2].type != COMMA ||
                line[i + 3].type != LBRACK ||
                line[i + 5].type != COMMA ||
                line[i + 7].type != RBRACK) {
                formatError("bad memory syntax in " + instruction);
            }

            int one = readRegisterToken(line[i + 1], false, true);
            int two = readRegisterToken(line[i + 4], false, true);
            int three = static_cast<int>(readImmediateToken(line[i + 6], labels));

            compileLine(binary, instruction, one, two, three);
            binaryOutput(out, binary);

            address += 4;
            continue;
        }

        formatError("could not parse instruction: " + instruction);
    }
}








/** Takes a tokenization of an ARM64 assembly file as input, then outputs a list of parameters for compileLine,
 * replacing label uses with their respective addresses. Prints label addresses into standard out.
 *
 * If the file is not found, print an error and returns a non-0 value.
 *
 * @return 0 on success, non-0 on error
 */
int _main(int argc, char * argv[])
{
    if(argc > 2)
    {
        std::cerr << "Usage:" << std::endl
                  << "\ttokenasm [FILE]" << std::endl
                  << std::endl
                  << "If FILE is unspecified or if FILE is `-`, read the assembly from standard "
                  << "in. Otherwise, read the assembly from FILE." << std::endl;
        return 1;
    }

    std::ifstream fp;
    std::istream &in =
        (argc > 1 && std::string(argv[1]) != "-")
      ? [&]() -> std::istream& {
            fp.open(argv[1]);
            return fp;
        }()
      : std::cin;

    if(!fp && argc > 1)
    {
        formatError((std::stringstream() << "File '" << argv[1] << "' not found!").str());
        return 1;
    }
    Token currToken;
    std::vector<Token> tokens;
    while (!in.eof()) {
        in >> currToken;
        if (!in.fail()) tokens.push_back(currToken);
        
    }
    
    // -- YOUR CODE HERE --
    // You've been given a vector of all the tokens, so you're now free to manipulate and scan all tokens as many times as necessary.
    // Go ham!

    std::vector<std::vector<Token>> lines = tokensToLines(tokens);

    std::map<std::string, uint32_t> labels;
    std::vector<std::string> labelOrder;

    firstPass(lines, labels, labelOrder);

    std::ostringstream binaryBuffer;
    binaryPass(lines, labels, binaryBuffer);

    // Print symbol table to stderr.
    for (const std::string& name : labelOrder) {
        std::cerr << name << " " << labels.at(name) << std::endl;
    }

    // Print binary to stdout.
    std::cout << binaryBuffer.str();

    

    return 0;
}

int main(int argc, char* argv[]) {
    try {
        _main(argc, argv);
        return 0;
    } catch (std::exception& e) {
        std::cerr << "ERROR: " << e.what() << "\n";
        return 1;
    }
}