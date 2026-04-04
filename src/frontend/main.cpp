/**
 * @file main.cpp
 * @brief Pascal-to-MXVM compiler command-line entry point
 * @author Jared Bruni
 */
#include "icode.hpp"
#include "parser.hpp"
#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <unordered_set>

std::string removeComments(const std::string &text) {
    std::ostringstream stream;
    enum State {
        CODE,
        BRACE_COMMENT,
        PAREN_COMMENT,
        STRING
    };
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
        if (argc != 3) {
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

        if (buffer.str().empty()) {
            std::cerr << "mxx: file is empty\n";
            return EXIT_FAILURE;
        }

        source = buffer.str();
        pascal::PascalParser parser(removeComments(source));
        if (!parser.validator.validate(argv[1])) {
            std::cerr << "mxx: validation failed.\n";
            return EXIT_FAILURE;
        }

        pascal::CodeGenVisitor emiter;

        if (parser.isUnitSource()) {
            auto ast = parser.parseUnit();
            if (ast) {
                emiter.generate(ast.get());
            }
        } else {
            auto ast = parser.parseProgram();
            if (ast) {
                // For programs with unit imports, parse each unit's interface
                // to register function/procedure signatures
                std::string inputPath = argv[1];
                std::string inputDir = inputPath.substr(0, inputPath.find_last_of("/\\") + 1);
                if (inputDir.empty()) inputDir = "./";

                for (const auto &dep : ast->uses) {
                    // Skip native modules
                    static const std::unordered_set<std::string> nativeModules = {"io", "std", "string", "sdl"};
                    if (nativeModules.count(dep)) continue;

                    // Try to find and parse the unit's .pas file for interface declarations
                    std::string unitFile = inputDir + dep + ".pas";
                    std::ifstream uf(unitFile);
                    if (!uf.is_open()) {
                        // Try lowercase
                        std::string lowerDep = dep;
                        std::transform(lowerDep.begin(), lowerDep.end(), lowerDep.begin(), ::tolower);
                        unitFile = inputDir + lowerDep + ".pas";
                        uf.open(unitFile);
                    }
                    if (uf.is_open()) {
                        std::ostringstream ubuf;
                        ubuf << uf.rdbuf();
                        uf.close();
                        try {
                            pascal::PascalParser unitParser(removeComments(ubuf.str()));
                            auto unitAst = unitParser.parseUnit();
                            if (unitAst) {
                                // Register interface declarations as forward declarations in the main AST
                                // and track which functions/procedures are external
                                for (auto &decl : unitAst->interfaceDecls) {
                                    if (auto *fd = dynamic_cast<pascal::FuncDeclNode *>(decl.get()))
                                        emiter.registerExternalFunc(fd->name, dep);
                                    else if (auto *pd = dynamic_cast<pascal::ProcDeclNode *>(decl.get()))
                                        emiter.registerExternalFunc(pd->name, dep);
                                    ast->block->declarations.push_back(std::move(decl));
                                }
                            }
                        } catch (...) {
                            // Unit source not available; user must ensure correct call signatures
                        }
                    }
                }

                emiter.generate(ast.get());
            }
        }

        {
            std::fstream outFile;
            outFile.open(argv[2], std::ios::out);
            if (!outFile.is_open()) {
                std::cerr << "Error could not open file: " << argv[2] << "\n";
                return EXIT_FAILURE;
            }
            std::ostringstream output;
            emiter.writeTo(output);
            std::string opt = pascal::mxvmOpt(output.str());
            outFile << opt << "\n";
            outFile.close();
        }
    } catch (const pascal::ParseException &e) {
        std::cerr << "Parse Error: " << e.what() << std::endl;
        return EXIT_FAILURE;
    } catch (const std::exception &e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return EXIT_FAILURE;
    } catch (const scan::ScanExcept &e) {
        std::cerr << "Syntax Error: " << e.why() << std::endl;
        return EXIT_FAILURE;
    }
    return 0;
}