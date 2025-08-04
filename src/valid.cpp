#include "mxvm/valid.hpp"
#include "scanner/exception.hpp"
#include "mxvm/instruct.hpp"
#include<algorithm>

namespace mxvm {

    void Validator::collect_labels(std::unordered_map<std::string, std::string> &labels) {
        for (size_t i = 0; i < scanner.size(); ++i) {
            const auto &tok = scanner[i];
            if (tok.getTokenType() == types::TokenType::TT_ID) {
                if (i + 1 < scanner.size() && scanner[i + 1].getTokenValue() == ":") {
                    labels[tok.getTokenValue()] = tok.getTokenValue(); 
                }
            }
        }
    }

    bool Validator::validate(const std::string &name) {
        filename = name;
        scanner.scan();
        std::unordered_map<std::string, std::string> labels;
        collect_labels(labels);

        auto skipSeparators = [&]() {
           /* while (true) {
                if (!token) break;
                if ((match("\n")  && match(types::TokenType::TT_SYM))) {
                    if (!next()) break;
                    continue;
                }
                break; 
            }*/
        };

        next();
        skipSeparators();
        if (match("program")) {
            next();
        } else {
            require("object");
            next();
        }

        skipSeparators();
        require(types::TokenType::TT_ID);
        next();
        skipSeparators();
        require("{");
        next();

        std::unordered_map<std::string, Variable> vars;
        for (auto &name : { "stdout", "stdin", "stderr" }) {
            vars[name] = Variable();
            vars[name].var_name = name;
        }

        skipSeparators();
        while (token && !match("}")) {
            skipSeparators();
            require("section");
            next();
            skipSeparators();
            require(types::TokenType::TT_ID);
            std::string sectionName = token->getTokenValue();
            next();
            skipSeparators();
            require("{");
            next();

            if (sectionName == "module" || sectionName == "object") {
                skipSeparators();
                while (token && !match("}")) {
                    skipSeparators();
                    if (match(types::TokenType::TT_ID)) {
                        next();
                        skipSeparators();
                        if (match(",")) {
                            next();
                            continue;
                        }
                        continue;
                    }
                    break;
                }
                skipSeparators();
                require("}");
                next();
            }
            else if (sectionName == "data") {
                skipSeparators();
                while (token && !match("}")) {
                    skipSeparators();
                    if (match(types::TokenType::TT_ID) &&
                        (token->getTokenValue() == "int" ||
                        token->getTokenValue() == "string" ||
                        token->getTokenValue() == "float" ||
                        token->getTokenValue() == "ptr" ||
                        token->getTokenValue() == "byte" ||
                        token->getTokenValue() == "export"))
                    {
                        if(token->getTokenValue() == "export")
                            next();

                        std::string vtype = token->getTokenValue();
                        next();
                        skipSeparators();

                        require(types::TokenType::TT_ID);
                        std::string vname = token->getTokenValue();
                        vars[vname].var_name = vname;
                        next();
                        skipSeparators();

                        if (match(",") && vtype == "string") {
                            next();
                            skipSeparators();
                            if (match(types::TokenType::TT_NUM) || match(types::TokenType::TT_HEX)) {
                                next();
                                skipSeparators();
                                continue;
                            } else {
                                throw mx::Exception("Syntax Error in file '" + filename + "': string buffer requires number on line " + std::to_string(token->getLine()));
                            }
                        }

                        require("=");
                        next();
                        skipSeparators();
                        if (match("-")) {
                            next();
                            skipSeparators();
                        }

                        if (vtype == "byte") {
                            if (!(match(types::TokenType::TT_NUM) || match(types::TokenType::TT_HEX))) {
                                throw mx::Exception("Syntax Error in file '" + filename + "': byte must be a valid byte value integer 0-255 on line " + std::to_string(token->getLine()));
                            }
                            int64_t value = std::stoll(token->getTokenValue(), nullptr, 0);
                            if (value < 0 || value > 0xFF) {
                                throw mx::Exception("Syntax Error in file '" + filename + "': byte out of range 0-255 on line: " + std::to_string(token->getLine()));
                            }
                            next();
                            skipSeparators();
                        } else if (vtype == "string") {
                            require(types::TokenType::TT_STR);
                            next();
                            skipSeparators();
                        } else if (token->getTokenValue() == "null" ||
                                match(types::TokenType::TT_NUM) ||
                                match(types::TokenType::TT_HEX) ||
                                match(types::TokenType::TT_STR)) {
                            next();
                            skipSeparators();
                        } else {
                            throw mx::Exception("Syntax Error in file '" + filename + "': Expected value for variable, found: " + token->getTokenValue() + " at line " + std::to_string(token->getLine()));
                        }
                    }
                    else {
                        throw mx::Exception("Syntax Error in file '" + filename + "': Expected variable declaration, found: " + token->getTokenValue() + " at line " + std::to_string(token->getLine()));
                    }
                }
                skipSeparators();
                require("}");
                next();
            }
            else if (sectionName == "code") {
                skipSeparators();
                const std::vector<std::string> branchOps = {
                    "call","jmp","je","jne","jg","jl","jge","jle","jz","jnz","ja","jb"
                };
                while (token && !match("}")) {
                    size_t old_index = index;

                    skipSeparators();
                    if (match(types::TokenType::TT_ID) && token->getTokenValue() == "function") {
                        next();
                        skipSeparators();
                        require(types::TokenType::TT_ID);
                        next();
                        skipSeparators();
                        require(":");
                        next();
                        continue;
                    }
                    if (match(types::TokenType::TT_ID) && peekIs(":")) {
                        next();
                        next();
                        continue;
                    }

                    if (match(types::TokenType::TT_ID)) {
                        std::string op = token->getTokenValue();
                        if (std::find(IncType.begin(), IncType.end(), op) == IncType.end()) {
                            throw mx::Exception("Syntax Error in file '" + filename + "': Unknown instruction '" + op + "' at line " + std::to_string(token->getLine()));
                        }
                        next();
                        skipSeparators();

                        if (op == "ret" || op == "done") {
                            continue;
                        }

                        if (std::find(branchOps.begin(), branchOps.end(), op) != branchOps.end()) {
                            require(types::TokenType::TT_ID);
                            next();
                            skipSeparators();
                            if (match(".")) {
                                next();
                                skipSeparators();
                                require(types::TokenType::TT_ID);
                                next();
                                skipSeparators();
                            }
                            continue;
                        }

                        while (true) {
                            skipSeparators();
                            if (match(types::TokenType::TT_ID)) {
                                std::string name = token->getTokenValue();
                                next();
                                skipSeparators();
                                if (match(".")) {
                                    next();
                                    skipSeparators();
                                    require(types::TokenType::TT_ID);
                                    name += "." + token->getTokenValue();
                                    next();
                                    skipSeparators();
                                }
                                var_names.emplace_back(name, *token);
                            } else if (match("-") && peekIs(types::TokenType::TT_NUM)) {
                                next();
                                next();
                            } else if (match(types::TokenType::TT_NUM) || match(types::TokenType::TT_HEX) || match(types::TokenType::TT_STR)) {
                                next();
                            } else {
                                break;
                            }

                            skipSeparators();
                            if (match(",")) {
                                next();
                                continue;
                            }
                            break;
                        }
                        continue;
                    }

                    // If we reach here, the token is not a recognized instruction, label, or function. It's a syntax error.
                    else {
                         throw mx::Exception("Syntax Error in file '" + filename + "': Unexpected token '" + token->getTokenValue() + "' in code section at line " + std::to_string(token->getLine()));
                    }

                    if (old_index == index) {
                       if (!next()) break;
                    }
                }
                skipSeparators();
                require("}");
                next();
            }
            else {
                throw mx::Exception("Syntax Error in file '" + filename + "': Unknown section: " + sectionName + " at line " + std::to_string(token->getLine()));
            }
            skipSeparators();
        }

        skipSeparators();
        require("}");
        return true;
    }


    bool Validator::match(const std::string &m) {
        if (!token) return false;
        if(token->getTokenValue() != m)
            return false;
        return true;
    }

    void Validator::require(const std::string &r) {
        if (!token) {
            throw mx::Exception("Syntax Error in file '" + filename + "': Required: " + r + " but reached end of file");
        }
        if(r != token->getTokenValue()) 
            throw mx::Exception(
                "Syntax Error in file '" + filename + "': Required: " + r +
                " Found: " + token->getTokenValue() +
                " at line " + std::to_string(token->getLine())
            );
    }

    bool Validator::match(const types::TokenType &t) {

        if(!token || index >= scanner.size())
            return false;

        if(t != token->getTokenType())
            return false;
        return true;
    }

    void Validator::require(const types::TokenType &t) {
        if(!token || index >= scanner.size()) {
            throw mx::Exception("unexpected EOF");
        }

         if(t != token->getTokenType()) 
            throw mx::Exception(
                "Syntax Error in file '" + filename + "': Required: " + tokenTypeToString(t) +
                " instead found: " + token->getTokenValue() +
                ":" + tokenTypeToString(token ->getTokenType()) +
                " at line " + std::to_string(token->getLine())
            );
    }
    
    bool Validator::next() {
        // Skip all newlines
        while (index < scanner.size() && 
               scanner[index].getTokenValue() == "\n" && 
               scanner[index].getTokenType() == types::TokenType::TT_SYM) {
            index++;
        }
        
        if (index < scanner.size()) {
            token = &scanner[index++];
            return true;
        }
        
        // We've reached the end
        token = nullptr;
        return false;
    }

    bool Validator::peekIs(const std::string &s) {
        return index < scanner.size() && scanner[index].getTokenValue() == s;
    }

    bool Validator::peekIs(const types::TokenType &t) {
        return index < scanner.size() && scanner[index].getTokenType() == t;
    }

    std::string Validator::tokenTypeToString(types::TokenType t) {
        switch (t) {
            case types::TokenType::TT_ID: return "Identifier";
            case types::TokenType::TT_NUM: return "Number";
            case types::TokenType::TT_HEX: return "Hex";
            case types::TokenType::TT_STR: return "String";
            case types::TokenType::TT_SYM: return "Symbol";
            default: return "Unknown";
        }
        return "";
    }
}