#include <fstream>
#include <iostream>
#include <regex>
#include <sstream>
#include <map>
#include <set>
#include <cstdint>
#include <stdexcept>

/** Maps the instruction name to the parameter type.  The value must be a 3 character string, 'r'
 *  represents a register, 'i' represents an immediate, 'z' represents a register where 0 is allowed, and ' ' represents no value */
const std::map<std::string, std::string> INSTRUCTION_PARAMETER_PATTERN = {
    {"add",   "rrz"},
    {"sub",   "rrz"},
    {"mul",   "rrz"},
    {"smulh", "rrz"},
    {"umulh", "rrz"},
    {"sdiv",  "rrz"},
    {"udiv",  "rrz"},
    {"cmp",   "rz "},
    {"br",    "r  "},
    {"blr",   "r  "},
    {"ldur",  "rri"},
    {"stur",  "rri"},
    {"ldr",   "ri "},
    {"b",     "i  "}
};

const std::map<std::string, uint32_t> INSTRUCTION_WORDTEMPLATE_PATTERN = {
    {"add",   0b10001011001000000110000000000000},
    {"sub",   0b11001011001000000110000000000000},
    {"mul",   0b10011011000000000111110000000000},
    {"smulh", 0b10011011010000000111110000000000},
    {"umulh", 0b10011011110000000111110000000000},
    {"sdiv",  0b10011010110000000000110000000000},
    {"udiv",  0b10011010110000000000100000000000},
    {"cmp",   0b11101011001000000110000000011111},
    {"br",    0b11010110000111110000000000000000},
    {"blr",   0b11010110001111110000000000000000},
    {"ldur",  0b11111000010000000000000000000000},
    {"stur",  0b11111000000000000000000000000000},
    {"ldr",   0b01011000000000000000000000000000},
    {"b",     0b00010100000000000000000000000000}
};

bool fitsSignedBits(int value, int bits)
{
    int min = -(1 << (bits - 1));
    int max =  (1 << (bits - 1)) - 1;

    return value >= min && value <= max;
}


void formatError(const std::string & message);

/** For a given instruction, returns the machine code for that instruction.
 *
 * @param[out] word The machine code for the instruction
 * @param instruction The name of the instruction
 * @param one The value of the first parameter
 * @param two The value of the second parameter
 * @param three The value of the third parameter
 */
bool compileLine(uint32_t &          word,
                 const std::string & instruction,
                 int            one,
                 int            two,
                 int            three)
{

    try {

        uint32_t word_template = INSTRUCTION_WORDTEMPLATE_PATTERN.at(instruction);
        std::string pattern = INSTRUCTION_PARAMETER_PATTERN.at(instruction);

        if (pattern == "rrz") {
            word = word_template | (three << 16) | (two << 5) | one;
        }

        else if (pattern == "rz ") {
            word = word_template | (two << 16) | (one << 5);
        }

        else if (pattern == "r  ") {
            word = word_template | (one << 5);
        }

        else if (pattern == "rri") {
            if (!fitsSignedBits(three, 9)) {
                formatError("Immediate value out of range for " + instruction);
                return false;
            }

            word = word_template | ((static_cast<uint32_t>(three) & 0x1FF) << 12) | (two << 5) | one;
        }

        else if (pattern == "ri ") {
            if (two % 4 != 0) {
                formatError("Immediate value for ldr must be divisible by 4");
                return false;
            }

            int encodedImmediate = two / 4;

            if (!fitsSignedBits(encodedImmediate, 19)) {
                formatError("Immediate value out of range for " + instruction);
                return false;
            }

            word = word_template
                | ((static_cast<uint32_t>(encodedImmediate) & 0x7FFFF) << 5)
                | one;
        }
        
        else if (pattern == "i  ") {
            if (one % 4 != 0) {
                formatError("Immediate value for b must be divisible by 4");
                return false;
            }

            int encodedImmediate = one / 4;

            if (!fitsSignedBits(encodedImmediate, 26)) {
                formatError("Immediate value out of range for " + instruction);
                return false;
            }

            word = word_template
                | (static_cast<uint32_t>(encodedImmediate) & 0x3FFFFFF);
        }

        else {
            formatError("Unsupported instruction pattern");
            return false;
        }

    } catch (const std::exception& e) {

        formatError(e.what());
        return false;
    }

    return true;
}

/** Prints an error to stderr with an "ERROR: " prefix, and newline suffix.
 *
 * @param message The error to print
 */
void formatError(const std::string & message)
{
    std::cerr << "ERROR: " << message << std::endl;
}

/** Matches a line of ARM assembly, potentially with comments */
std::regex ARM_LINE_PATTERN(
    "^\\s*([a-z]+)\\s+(x\\d+|0x[0-9a-fA-F]*|-?\\d+|xzr)\\s*(?:(?:(?:,\\s*(x\\d+|0x[0-9a-fA-F]*|-?\\d+|xzr))?(?:,\\s*(x\\d+|0x[0-9a-fA-F]*|-?\\d+|xzr))?)|(?:,\\s*\\[\\s*(x\\d+|xzr)\\s*,\\s*(0x[0-9a-fA-F]*|-?\\d+)\\s*\\]))\\s*(?:\\/\\/.*)?$"
);

/** Recognizes an empty line (or an empty line with a comment) */
std::regex EMPTY_LINE("^\\s*(//.*)?$");


/** Convert a string representation of an immediate value to a signed 32-bit integer. Accounts for negatives.
 * If the string starts with "0x", it is interpreted as an unsigned hexadecimal value.
 *
 * The function name is read as "string to uint32".
 *
 * @param s The string to parse
 * @return The uint32_t representation of the string
 */
int readImm(const std::string & s)
{
    if(s.starts_with("0x"))
    {
        return std::stoi(s.substr(2), nullptr, 16);
    }
    return std::stoi(s);
}

/** Convert a string representation of a register name to the register number. If "xzr" (the zero register)
 * is read, it returns 31. Otherwise, it returns the register number directly. 
 *
 * The function name is read as "string to uint32".
 *
 * @param s The string to parse
 * @return The uint32_t representation of the string
 */
uint32_t readReg(bool zeroable, const std::string & s)
{
    if (s == "xzr") {
        if (zeroable) {
            return 31;
        } else {
            throw std::runtime_error("Register 'xzr' is not allowed in a non-'xm' position");
        }
    }

    if(!s.starts_with("x"))
    {
        throw std::runtime_error("Invalid register value '" + s + "'");
    }
    int ret = std::stoi(s.substr(1), nullptr, 10);
    if (ret > 30) {
        throw std::runtime_error("Register value '" + s + "' is too large");
    }
    if (ret < 0) {
        throw std::runtime_error("Register value '" + s + "' is negative");
    }
    return ret;
}


/** Compiles one line of assembly and send the binary to standard out.  If the assembly is invalid,
 *  print an error to stderr and return false.  Assumes that the assembly does not have a trailing
 *  comment.
 *
 * @param line The line to parse
 * @return True if the line is valid assembly and was output to stdout, false otherwise
 */
bool parseLine(const std::string & line)
{
    std::smatch matches;
    if(!std::regex_search(line, matches, ARM_LINE_PATTERN))
    {
        formatError((std::stringstream() << "Unable to parse line: \"" << line << "\"").str());
        return false;
    }

    std::string instruction = matches[1];

    uint32_t parameters[3] = {0, 0, 0};

    auto pattern = INSTRUCTION_PARAMETER_PATTERN.find(instruction);
    if (pattern == INSTRUCTION_PARAMETER_PATTERN.end()) {
        formatError((std::stringstream() << "'" << instruction << "' is not a known instruction").str());
        return false;
    }

    std::string argmatches[3] = {
        matches[2],
        matches[3].matched ? matches[3] : matches[5],
        matches[4].matched ? matches[4] : matches[6],
    };
    uint32_t index = 0;
    try {
        for (char c : pattern->second) {
            if (c == 'r') {
                if (argmatches[index] == "") {
                    throw std::runtime_error("Instruction '" + instruction + "' is missing a register value");
                }
                parameters[index] = readReg(false, argmatches[index]);
            } else if (c == 'z') {
                if (argmatches[index] == "") {
                   throw std::runtime_error("Instruction '" + instruction + "' is missing a zeroable register value");
                }
                parameters[index] = readReg(true, argmatches[index]);
            } else if (c == 'i') {
                if (argmatches[index] == "") {
                    throw std::runtime_error("Instruction '" + instruction + "' is missing an immediate value");
                }
                parameters[index] = readImm(argmatches[index]);
            } else { // Extra args
                if (argmatches[index] != "") {
                    throw std::runtime_error("Instruction '" + instruction + "' has extraneous arguments");
                }
            }
            index++;
        }
    } catch (std::runtime_error& s) {
        formatError(s.what());
        return false;
    } catch (std::invalid_argument& arg){
        formatError(arg.what());
        return false;
    }

    uint32_t binary = 0;
    bool compiled = compileLine(binary,
                                instruction,
                                parameters[0],
                                parameters[1],
                                parameters[2]);
    if(compiled)
    {
        // Output of the binary in little-endian order
        std::cout << (char)((binary >> 0) & 0xFF)
                  << (char)((binary >> 8) & 0xFF)
                  << (char)((binary >> 16) & 0xFF)
                  << (char)((binary >> 24) & 0xFF);
        return true;
    }
    else
    {
        // compileLine (should have) printed an error.  We don't have to print one here.
        return false;
    }
}

/** Entrypoint for the assembler.  The first parameter (optional) is a mips assembly file to
 *  read.  If no parameter is specified, read assembly from stdin.  Prints machine code to stdout.
 *  If invalid assembly is found, prints an error to stderr, stops reading assembly, and return a
 *  non-0 value.
 *
 * If the file is not found, print an error and returns a non-0 value.
 *
 * @return 0 on success, non-0 on error
 */
int main(int argc, char * argv[])
{
    if(argc > 2)
    {
        std::cerr << "Usage:" << std::endl
                  << "\tasm [$FILE]" << std::endl
                  << std::endl
                  << "If $FILE is unspecified or if $FILE is `-`, read the assembly from standard "
                  << "in. Otherwise, read the assembly from $FILE." << std::endl;
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
        formatError((std::stringstream() << "file '" << argv[1] << "' not found!").str());
        return 1;
    }

    while(!in.eof())
    {
        std::string line;
        std::getline( in, line );

        // Filter out any comments
        uint32_t startComment = line.find(";");
        if(startComment != std::string::npos)
        {
            line = line.substr(0, line.find(";"));
        }

        std::smatch matches;
        if(std::regex_search(line, matches, EMPTY_LINE))
        {
            continue;
        }

        if(!parseLine(line))
        {
            return 1;
        }
    }

    return 0;
}
