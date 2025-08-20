#include "parser.hpp"
#include "icode.hpp"
#include <iostream>
#include <fstream>
#include <sstream>


std::string removeComments(const std::string &text) {
    std::ostringstream stream;
    enum State { CODE, BRACE_COMMENT, PAREN_COMMENT, STRING };
    State state = CODE;
    
    for (size_t i = 0; i < text.length(); ++i) {
        char c = text[i];
        char next = (i + 1 < text.length()) ? text[i + 1] : '\0';
        
        switch (state) {
            case CODE:
                if (c == '{') {
                    state = BRACE_COMMENT;
                } else if (c == '(' && next == '*') {
                    state = PAREN_COMMENT;
                    i++; 
                } else if (c == '\'') {
                    stream << c;
                    state = STRING;
                } else if (c == '/' && next == '/') {
                    
                    while (i < text.length() && text[i] != '\n') {
                        i++;
                    }
                    if (i < text.length()) {
                        stream << '\n';
                    }
                } else {
                    stream << c;
                }
                break;
                
            case BRACE_COMMENT:
                if (c == '}') {
                    state = CODE;
                }
                break;
                
            case PAREN_COMMENT:
                if (c == '*' && next == ')') {
                    state = CODE;
                    i++; 
                }
                break;
                
            case STRING:
                stream << c;
                if (c == '\'') {
                    if (next == '\'') {
                        stream << next;
                        i++; 
                    } else {
                        state = CODE;
                    }
                }
                break;
        }
    }

    return stream.str();
}

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
        pascal::PascalParser parser(removeComments(source));
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
    } catch(const scan::ScanExcept &e) {
        std::cerr << "Syntax Error: " << e.why() << std::endl;
    }
    return 0;
}