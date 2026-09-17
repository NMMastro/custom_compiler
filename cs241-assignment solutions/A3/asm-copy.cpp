#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <map>
#include <cstdint>
#include <vector>

/** Prints an error to stderr with an "ERROR: " prefix, and newline suffix. Terminates the program with an error.
 *
 * @param message The error to print
 */
void formatError(const std::string & message)
{
    throw std::runtime_error(message);
}

std::map<std::string, int> idParamCountBuild()
{
    std::map<std::string, int> params;

    // br x30
    // blr x5
    // tokens after ID: REG
    params["br"] = 1;
    params["blr"] = 1;

    // add x0, x1, x2
    // tokens after ID: REG COMMA REG COMMA REG
    params["add"] = 5;
    params["sub"] = 5;
    params["mul"] = 5;
    params["sdiv"] = 5;
    params["udiv"] = 5;
    params["smulh"] = 5;
    params["umulh"] = 5;

    // cmp x0, x1
    // tokens after ID: REG COMMA REG
    params["cmp"] = 3;

    // ldr x0, 8
    // tokens after ID: REG COMMA INT/HEXINT/ID
    params["ldr"] = 3;

    // ldur x0, [x1, 8]
    // tokens after ID: REG COMMA LBRACK REG COMMA INT RBRACK
    params["ldur"] = 7;
    params["stur"] = 7;

    // b 12
    // tokens after ID: INT/HEXINT/ID
    params["b"] = 1;

    // b.eq 12
    // tokenized as: ID b DOTID .eq INT 12
    // tokens after ID: DOTID INT/HEXINT/ID
    // This is a special b case, so do not put it separately as "b.eq".
    // Still params["b"] = 1 for normal b.
    
    return params;
}

std::map<std::string, uint32_t> condHashBuild()
{
    std::map<std::string, uint32_t> cond;

    cond[".eq"] = 0b00000; // equals
    cond[".ne"] = 0b00001; // not equals

    cond[".hs"] = 0b00010; // unsigned >=
    cond[".lo"] = 0b00011; // unsigned <

    cond[".hi"] = 0b01000; // unsigned >
    cond[".ls"] = 0b01001; // unsigned <=

    cond[".ge"] = 0b01010; // signed >=
    cond[".lt"] = 0b01011; // signed <

    cond[".gt"] = 0b01100; // signed >
    cond[".le"] = 0b01101; // signed <=

    return cond;
}

std::map<std::string, std::pair<uint32_t, uint32_t> > hash_build() {
    std::map<std::string, std::pair<uint32_t, uint32_t> > dict;

    dict["add"] = std::pair<uint32_t, uint32_t>(0b10001011001, 0b011000);
    dict["sub"] = std::pair<uint32_t, uint32_t>(0b11001011001, 0b011000);
    dict["mul"] = std::pair<uint32_t, uint32_t>(0b10011011000, 0b011111);
    dict["smulh"] = std::pair<uint32_t, uint32_t>(0b10011011010, 0b011111);
    dict["umulh"] = std::pair<uint32_t, uint32_t>(0b10011011110, 0b011111);
    dict["sdiv"] = std::pair<uint32_t, uint32_t>(0b10011010110, 0b000011);
    dict["udiv"] = std::pair<uint32_t, uint32_t>(0b10011010110, 0b000010);

    dict["cmp"] = std::pair<uint32_t, uint32_t>(0b11101011001, 0b011000);

    dict["br"] = std::pair<uint32_t, uint32_t>(0b11010110000, 0b000000);
    dict["blr"] = std::pair<uint32_t, uint32_t>(0b11010110001, 0b000000);

    dict["ldur"] = std::pair<uint32_t, uint32_t>(0b11111000010, 0b00);
    dict["stur"] = std::pair<uint32_t, uint32_t>(0b11111000000, 0b00);
    dict["ldr"] = std::pair<uint32_t, uint32_t>(0b01011000, 0b00);
    dict["b"] = std::pair<uint32_t, uint32_t>(0b000101, 0b00);
    
    return dict;
}

bool range_checker(int value, int max, int min, std::string instruction) {
    if (value < min || value > max) {
        return false;
    }
    return true;
}

bool checkReg(int r) {
    return r >= 0 && r <= 30;
}

bool checkRegOrXzr(int r) {
    return r >= 0 && r <= 31;
}

/** For a given instruction, returns the machine code for that instruction.
 *
 * @param[out] word The machine code for the instruction
 * @param instruction The name of the instruction
 * @param one The value of the first parameter
 * @param two The value of the second parameter
 * @param three The value of the third parameter
 * Check for negative values 
 * check for i overflow
 */
bool compileLine(uint32_t &          word,
                 const std::string & instruction,
                 int            one,
                 int            two,
                 int            three)
{
    std::map<std::string, std::pair<uint32_t, uint32_t>> hash = hash_build();    
    
    
        
    if (instruction == "mul" || instruction == "smulh" || instruction == "umulh" || 
        instruction == "sdiv" || instruction == "udiv" || instruction == "add" || instruction == "sub") {

        if (instruction == "add" || instruction == "sub") {
            if (!checkRegOrXzr(one) || !checkRegOrXzr(two) || !checkRegOrXzr(three)) {
                formatError("out of range register " + instruction);
                return false;
            }

        } else {
            if (!checkRegOrXzr(one) || !checkRegOrXzr(two) || !checkRegOrXzr(three)) {
                formatError("Out-of-range register in instruction " + instruction);
                return false;
            }
        }   

            word = (hash[instruction].first << 21) | (three << 16) | (hash[instruction].second << 10) | (two << 5) | one;
            return true;

        }

    if (instruction == "cmp") {

        if (!checkRegOrXzr(one) || !checkRegOrXzr(two)) {
            formatError("Out-of-range register in instruction " + instruction);
            return false;
        }

        word = (hash[instruction].first << 21) | (two << 16) | (hash[instruction].second << 10) | (one << 5) | 0b11111; 
        return true;
    }

    if (instruction == "br" || instruction == "blr") {
        if (!checkRegOrXzr(one)) {
            formatError("Out-of-range register in instruction " + instruction);
            return false;
        }

        word = (hash[instruction].first << 21) | (0b11111 << 16) | (hash[instruction].second << 10) | (one << 5) | 0b00000; 
        return true;
    }

    if (instruction == "ldur" || instruction == "stur") {

        if (!range_checker(three, 255, -256, instruction)) {
            formatError("Out-of-range immediate value " + std::to_string(three)+ " in instruction " + instruction);
            return false;
        }

        if (!checkRegOrXzr(one) || !checkRegOrXzr(two)) {
            formatError("Out-of-range register in instruction " + instruction);
            return false;
        }

        word = (hash[instruction].first << 21) | ((three & 0x1FF) << 12) | (hash[instruction].second << 10) | (two << 5) | one; 
        return true;
    }

    if (instruction == "b") {
        uint32_t op_code = 0b01010100;

        // Add check to make sure two is divisable by 4
        if (one % 4 != 0) {
            formatError("Immediate must be divisible by 4: " + std::to_string(one) + " in instruction " + instruction);
            return false;
        }

        //Add check for range of i: one
        if (three == -121) {  
            if (!range_checker(one, 1048572, -1048576, instruction)) {
                formatError("Out-of-range immediate value " + std::to_string(one)+ " in instruction " + instruction);
                return false;
            }  
        } else if (!range_checker(one, 134217724, -134217728, instruction)) {
            formatError("Out-of-range immediate value " + std::to_string(one)+ " in instruction " + instruction);
            return false;
        }    

        if (three == -121) {
            int imm = one >> 2; // because encoding stores i where branch is PC + i*4
            word =  (op_code << 24) |
                    ((imm & 0x7FFFF) << 5) |
                    (two & 0x1F);

            return true;
        }

        word = ((hash[instruction].first & 0x3F) << 26) |
           ((one >> 2) & 0x03FFFFFF);
            return true;
    }

    if (instruction == "ldr") {


        if (two % 4 != 0) {
            formatError("Immediate must be divisible by 4: " + std::to_string(two) + " in instruction " + instruction);
            return false;
        }

        //Add check for i: two
        if (!range_checker(two, 1048572, -1048576, instruction)) {
            formatError("Out-of-range immediate value " + std::to_string(two)+ " in instruction " + instruction);
            return false;
        }

        if (!checkReg(one)) {
            formatError("Out-of-range register in instruction " + instruction);
            return false;
        }

        word = (hash[instruction].first << 24) |
               (((two >> 2) & 0x7FFFF) << 5) |
                (one & 0x1F);

        return true;
    }
    
    return false;
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



//Split up the vector of tokens into lines 
std::vector<std::vector<Token>> splitIntoLines(std::vector<Token> tokens) {

    std::vector<std::vector<Token>> lines;
    std::vector<Token> current_line; 

    for (const Token &token : tokens) {
        if (token.type == NEWLINE) {
            lines.push_back(current_line);
            current_line.clear();
        } else { 
            current_line.push_back(token);
        }
    }

    if (!current_line.empty()) {
        lines.push_back(current_line); 
    }

    return lines; 

}


void first_pass(const std::vector<std::vector<Token>>& lines,
               std::map<std::string, uint32_t>& labels,
               std::vector<std::string>& labelOrder) {

                uint32_t address = 0;

                for (const std::vector<Token> &line : lines) {

                    if (line.empty()) {
                        continue; 
                    }

                    size_t i = 0;

                    while (i < line.size() && line[i].type == LABEL) {
                        //trim off the colon at the end
                        std::string labelName = line[i].lexeme.substr(0, line[i].lexeme.size() - 1);

                        if (labels.count(labelName) > 0) {
                            formatError("duplicate label name:" + labelName);
                        }
                        
                        //store the labelName
                        labels[labelName] = address;
                        labelOrder.push_back(labelName); 

                        i++; 
                    }

                    //If the whole line was a label dont increase address
                    if (i == line.size()) {
                        continue;
                    }

                    //Make sure no label after whole loop terminates 
                    for (size_t j = i ; j < line.size() ; ++j){
                        if (line[j].type == LABEL) {
                            formatError("Label appears after instrution");
                        }

                    }

                    // .8byte takes 8byte 
                    if (line[i].type == DOTID) {
                        if (line[i].lexeme != ".8byte") {
                            formatError("unknown dirrective: " + line[i].lexeme);
                        }

                        address += 8;
                    } else if (line[i].type == ID) {
                        address += 4; 
                    } else {
                        formatError("line must start with label, instruction, or .8byte");
                    }
                }
}

void binary_output(std::ostream& out, uint32_t binary) {
    out << (char)((binary >> 0) & 0xFF)
        << (char)((binary >> 8) & 0xFF)
        << (char)((binary >> 16) & 0xFF)
        << (char)((binary >> 24) & 0xFF);
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

void binary_pass(std::vector<std::vector<Token>> lines, std::map<std::string, uint32_t> labels, std::ostream& out) {

    auto para_count = idParamCountBuild();

    uint32_t address = 0;
    for (std::vector<Token> line : lines) {
        size_t i = 0;
        if (line.empty()) { continue; }

        while (i < line.size() && line[i].type == LABEL) {
            // anything: must be in label map
            ++i; // Skip over the labels 
        }

        if (line.size() == i) {
            continue;
        }

        // Check to make sure the line has a valid type
        if (line[i].type == ID) { 
            // Can be br or blr
            auto instruction = line[i].lexeme;

            if (para_count.count(instruction) == 0) {
                formatError("unknown instruction: " + instruction);
            }

            if (para_count[line[i].lexeme] == 5) {
                if (i + 6 != line.size()) {
                    formatError(instruction + " takes exactly three registers");
                }

                if (line[i + 2].type != COMMA) {
                    formatError("expected comma after first register");
                }

                if (line[i + 4].type != COMMA) {
                    formatError("expected comma after second register");
                }  
                
                //bool isAddSub = instruction == "add" || instruction == "sub";

                int one = readRegisterToken(line[i + 1], false, true);
                int two = readRegisterToken(line[i + 3], false, true);
                int three = readRegisterToken(line[i + 5], true, false);

                uint32_t binary = 0;
                bool compiled = compileLine(binary, instruction, one, two, three);

                if (compiled) {
                    binary_output(out, binary); 
                    address += 4;
                } else if (!compiled) {
                    formatError("could not compile instruction: " + instruction);
                }
            }


            // case #2 1 param and b.cond 
            // case #2: 1-param instructions and b.cond
if (para_count[instruction] == 1) {
    uint32_t binary = 0;
    bool compiled = false;

    // b.cond case:
    // ID b
    // DOTID .eq/.ne/.lt/etc.
    // INT/HEXINT/ID target
    if (instruction == "b" &&
        i + 3 == line.size() &&
        line[i + 1].type == DOTID) {

        auto cond_map = condHashBuild();

        std::string cond = line[i + 1].lexeme;

        if (cond_map.count(cond) == 0) {
            formatError("invalid condition code: " + cond);
        }

        int condCode = cond_map[cond];

        int immediate = readImmediateToken(line[i + 2], labels);

        // If target is a label, convert absolute address to relative offset
        if (line[i + 2].type == ID) {
            immediate = immediate - static_cast<int>(address);
        }

        compiled = compileLine(binary, instruction, immediate, condCode, -121);
    }

    // br x30 / blr x5
    else if (instruction == "br" || instruction == "blr") {
        if (i + 2 != line.size()) {
            formatError(instruction + " takes exactly one register");
        }

        int one = readRegisterToken(line[i + 1], false, true);

        compiled = compileLine(binary, instruction, one, 0, 0);
    }

    // plain b:
    // ID b
    // INT/HEXINT/ID target
    else if (instruction == "b") {
        if (i + 2 != line.size()) {
            formatError("b takes exactly one immediate or label");
        }

        int immediate = readImmediateToken(line[i + 1], labels);

        // If target is a label, convert absolute address to relative offset
        if (line[i + 1].type == ID) {
            immediate = immediate - static_cast<int>(address);
        }

        // -1 means normal b, not b.cond
        compiled = compileLine(binary, instruction, immediate, -1, 0);
    }

    else {
        formatError("unknown one-parameter instruction: " + instruction);
    }

    if (!compiled) {
        formatError("could not compile instruction: " + instruction);
    }

    binary_output(out, binary);
    address += 4;
}

            //Case #3 param = 2 ldr, cmp 
            if (para_count[instruction] == 3) { 

                if (i + 4 != line.size()) {
                    formatError(instruction + " takes exactly three tokens after instruction");
                }

                if (line[i + 2].type != COMMA) {
                    formatError("expected comma");
                }

                uint32_t binary = 0;
                bool compiled = false;

                if (instruction == "cmp") {
                    int one = readRegisterToken(line[i + 1], false, true);
                    int two = readRegisterToken(line[i + 3], true, false);

                    compiled = compileLine(binary, instruction, one, two, 0);
                    address += 4;
                }

                else if (instruction == "ldr") {
                    int one = readRegisterToken(line[i + 1], false, false);
                    int two = readImmediateToken(line[i + 3], labels);

                    compiled = compileLine(binary, instruction, one, two, 0);
                    address += 4;
                }

                else {
                    formatError("unknown 3-token instruction: " + instruction);
                }

                if (compiled) {
                    binary_output(out, binary);
                } else {
                    formatError("could not compile instruction: " + instruction);
                }
            }

            // case #4: 7-token parameter instructions
            // ldur x0, [x1, 8]
            // stur x0, [x1, 8]
            if (para_count[instruction] == 7) {
                if (i + 8 != line.size()) {
                    formatError(instruction + " takes register, [register, immediate]");
                }

                if (line[i + 2].type != COMMA) {
                    formatError("expected comma after first register");
                }

                if (line[i + 3].type != LBRACK) {
                    formatError("expected [");
                }

                if (line[i + 5].type != COMMA) {
                    formatError("expected comma inside memory operand");
                }

                if (line[i + 7].type != RBRACK) {
                    formatError("expected ]");
                }

                int one = readRegisterToken(line[i + 1], false, true);      // destination/source reg
                int two = readRegisterToken(line[i + 4], false, true);      // base reg
                int three = readImmediateToken(line[i + 6], labels);  // offset

                uint32_t binary = 0;
                bool compiled = compileLine(binary, instruction, one, two, three);
                address += 4;

                if (compiled) {
                    binary_output(out, binary);
                } else {
                    formatError("could not compile instruction: " + instruction);
                }
            }
        }

        if (line[i].type == DOTID) {
            //.8byte when it is start of a line u cant start with .cond
            if (line[i].lexeme != ".8byte") {
                formatError("can't start a line with DOTID that is not .8byte");
            }
            // Pattern must be:
            // DOTID .8byte
            // INT/HEXINT/ID value
            if (i + 2 != line.size()) {
                formatError(".8byte takes exactly one argument");
            }

            int64_t value = readImmediateToken(line[i + 1], labels);

            // Output 8 bytes
            emit8Byte(out, value); 

            address += 8;
            continue;
        }

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
    std::vector<std::vector<Token>> lines = splitIntoLines(tokens); // Split up into lines

    std::map<std::string, uint32_t> labels;
    std::vector<std::string> order; 

    first_pass(lines, labels, order);

    std::ostringstream binaryBuffer;

    binary_pass(lines, labels, binaryBuffer);

    // Only print labels and binary if no error happened
    for (const std::string& name: order) {
        std::cerr << name << " " << labels[name] << std::endl;
    }

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