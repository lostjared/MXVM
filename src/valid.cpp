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

    bool Validator::validate() {
        scanner.scan();
        std::unordered_map<std::string, std::string> labels;
        collect_labels(labels);
        next(); require("program"); next();
        require(types::TokenType::TT_ID); next();
        require("{"); next();

        std::unordered_map<std::string, Variable> vars;

        while (token->getTokenValue() != "}") {
            require("section"); next();
            require(types::TokenType::TT_ID);
            std::string sectionName = token->getTokenValue(); next();
            require("{"); next();

            if (sectionName == "data") {
                while (token->getTokenValue() != "}") {
                    if (token->getTokenType() == types::TokenType::TT_ID &&
                        (token->getTokenValue() == "int" || token->getTokenValue() == "string" || token->getTokenValue() == "float" || token->getTokenValue() == "ptr"))
                    {
                        Variable value;

                        next(); 
                        require(types::TokenType::TT_ID); 
                        value.var_name = token->getTokenValue();
                        vars[value.var_name] = value;
                        next();
                        require("="); next();
                        if (token->getTokenValue() == "null"  || token->getTokenType() == types::TokenType::TT_NUM ||
                            token->getTokenType() == types::TokenType::TT_HEX ||
                            token->getTokenType() == types::TokenType::TT_STR)
                        {
                            next();
                        } else {
                            throw mx::Exception(
                                "Syntax Error: Expected value for variable, found: " + token->getTokenValue() +
                                " at line " + std::to_string(token->getLine())
                            );
                        }
                    }
                    else if (token->getTokenValue() == "\n" || token->getTokenValue() == ";") {
                        next();
                    }
                    else {
                        throw mx::Exception(
                            "Syntax Error: Expected variable declaration, found: " + token->getTokenValue() +
                            " at line " + std::to_string(token->getLine()));
                    }
                }
                require("}"); next();
            }
            else if (sectionName == "code") {
                while (token->getTokenValue() != "}") {
                    if (token->getTokenValue() == "\n" || token->getTokenValue() == ";" || token->getTokenValue().rfind("//", 0) == 0) {
                        next();
                        continue;
                    }

                    if(match(types::TokenType::TT_ID) && token->getTokenValue() == "function") {
                        next();
                        if (match(types::TokenType::TT_ID) && peekIs(":")) {
                            next();
                            next();
                            continue;
                        } else {
                            throw mx::Exception("Syntax Error: function must be followed by a label at l ine " + std::to_string(token->getLine()));
                        }
                    }  else {
                        if (match(types::TokenType::TT_ID) && peekIs(":")) {
                            next();
                            next();
                            continue;
                        }
                    }
                                     
                    if (match(types::TokenType::TT_ID)) {
                        std::string op = token->getTokenValue();
                        if (std::find(IncType.begin(), IncType.end(), op) == IncType.end()) {
                            throw mx::Exception(
                                "Syntax Error: Unknown instruction '" + op + "' at line " + std::to_string(token->getLine())
                            );
                        }
                        if(op == "ret" || op == "done") {
                            next();
                            continue;
                        } else if(op == "call" || op == "jmp" || op == "je" || op == "jne" || op == "jg" || op == "jl" || op == "jge" || op == "jle" || op == "jz" || op == "jnz" || op == "ja" || op == "jb") {
                            if(next()) {
                                if(match(types::TokenType::TT_ID)) {
                                    std::string label = token->getTokenValue();
                                    if(labels.find(label) == labels.end()) {
                                        throw mx::Exception(
                                            "Label not found: '" + label +
                                            "' at line " + std::to_string(token->getLine()) 
                                        );
                                    }
                                    next();
                                } else {
                                    throw mx::Exception(
                                        "Instruction requires label at line " + std::to_string(token->getLine())
                                    );
                                }
                            } else {
                                throw mx::Exception("Instruction requires label");                
                            }
                            continue;
                        }
                        next();
                    }
                    else {
                        throw mx::Exception(
                            "Syntax Error: Expected instruction or label, found: " + token->getTokenValue() +
                            " at line " + std::to_string(token->getLine())
                        );
                    }
                    
                    bool firstOperand = true;
                    while (true) {
                        if (firstOperand) {

                            if (match(types::TokenType::TT_ID)) {
                                std::string argName = token->getTokenValue();
                                if (vars.find(argName) == vars.end()) {
                                    throw mx::Exception(
                                        "Syntax Error: Argument variable not defined: " + argName +
                                        " at line " + std::to_string(token->getLine())
                                    );
                                }
                            }

                            if (!(match(types::TokenType::TT_ID) || match(types::TokenType::TT_NUM) || match(types::TokenType::TT_HEX) || match(types::TokenType::TT_STR))) {
                                break; 
                            }

                        } else {
                            if (token->getTokenValue() != ",") {
                                break; 
                            }
                            next(); 
                            
                            while (token->getTokenValue() == "\n" || token->getTokenValue() == ";" || token->getTokenValue().rfind("//", 0) == 0) {
                                next();
                            }
                            

                            if (!(match(types::TokenType::TT_ID) || match(types::TokenType::TT_NUM) || match(types::TokenType::TT_HEX) || match(types::TokenType::TT_STR))) {
                                throw mx::Exception(
                                    "Syntax Error: Expected operand after comma at line " + std::to_string(token->getLine())
                                );
                            }

                            if (match(types::TokenType::TT_ID)) {
                                std::string argName = token->getTokenValue();
                                if (vars.find(argName) == vars.end()) {
                                    throw mx::Exception(
                                        "Syntax Error: Argument variable not defined: " + argName +
                                        " at line " + std::to_string(token->getLine())
                                    );
                                }
                            }
                        }
                        
                        if (match(types::TokenType::TT_NUM)) {
                            auto v = token->getTokenValue();
                            if (v.find('e') != std::string::npos || v.find('.') != std::string::npos) {
                                throw mx::Exception(
                                    "Syntax Error: Invalid integer constant: " + v +
                                    " at line " + std::to_string(token->getLine())
                                );                            }
                        }
                        if (match(types::TokenType::TT_HEX)) {
                            auto v = token->getTokenValue();
                            if (!v.starts_with("0x") && !v.starts_with("0X")) {
                                throw mx::Exception(
                                    "Syntax Error: Invalid hex constant: " + v +
                                    " at line " + std::to_string(token->getLine())
                                );
                            }
                        }
                        next(); 
                        firstOperand = false;
                    }
                }
                require("}"); next();
            }
            else {
                throw mx::Exception("Syntax Error: Unknown section: " + sectionName +
                    " at line " + std::to_string(token->getLine())
                );
            }
        }
        require("}");
        return true;
    }

    bool Validator::match(const std::string &m) {
        if(token->getTokenValue() != m)
            return false;
        return true;
    }

    void Validator::require(const std::string &r) {
        if(r != token->getTokenValue()) 
            throw mx::Exception(
                "Syntax Error: Required: " + r +
                " Found: " + token->getTokenValue() +
                " at line " + std::to_string(token->getLine())
            );
    }

    bool Validator::match(const types::TokenType &t) {
        if(t != token->getTokenType())
            return false;
        return true;
    }

    void Validator::require(const types::TokenType &t) {
         if(t != token->getTokenType()) 
            throw mx::Exception(
                "Syntax Error: Required: " + std::to_string(static_cast<int>(t)) +
                " instead found: " + token->getTokenValue() +
                ":" + std::to_string(static_cast<int>(token->getTokenType())) +
                " at line " + std::to_string(token->getLine())
            );
    }
    
    bool Validator::next() {
        while (index < scanner.size() && scanner[index].getTokenType() != types::TokenType::TT_STR && scanner[index].getTokenValue() == "\n") {
            index++;
        }
        if (index < scanner.size()) {
            token = &scanner[index++];  
            return true;
        }
        return false;
    }

    bool Validator::peekIs(const std::string &s) {
        return index < scanner.size() && scanner[index].getTokenValue() == s;
    }

    bool Validator::peekIs(const types::TokenType &t) {
        return index < scanner.size() && scanner[index].getTokenType() == t;
    }
}