#include "parser.hpp"
#include "icode.hpp"
#include <iostream>
#include <fstream>
#include <sstream>

int main(int argc, char **argv) {
    try {
        std::string source;
        if(argc != 3) {
            std::cerr << "Error: requires two arguments input/output.\n";
            return EXIT_FAILURE;
        }
         std::ifstream file(argv[1]);
        if (!file.is_open()) {
            std::cerr << "Error: Cannot open file " << argv[1] << std::endl;
            return 1;
        }
        std::ostringstream buffer;
        buffer << file.rdbuf();
        source = buffer.str();       
        pascal::PascalParser parser(source);
        auto ast = parser.parseProgram();
        if (ast) {      
            pascal::CodeGenVisitor emiter;
            emiter.generate(ast.get());
            std::fstream file;
            file.open(argv[2], std::ios::out);
            if(!file.is_open()) {
                std::cerr << "Error could not open file: " << argv[2] << "\n";
            }
            std::ostringstream output;
            emiter.writeTo(output);
            std::string opt = pascal::mxvmOpt(output.str());
            file << opt << "\n";
            file.close();
        }
    } catch (const pascal::ParseException& e) {
        std::cerr << "Parse Error: " << e.what() << std::endl;
        return 1;
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}