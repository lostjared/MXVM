#include "mxvm/valid.hpp"
#include "scanner/exception.hpp"

namespace mxvm {

    bool Validator::validate() {
        scanner.scan();
        next();
        require("program");
        next();
        require(types::TokenType::TT_ID);
        next();
        require("{");
        next();

        while (token->getTokenValue() != "}") {
            require("section");
            next();
            require(types::TokenType::TT_ID); 
            std::string sectionName = token->getTokenValue();
            next();
            require("{");
            next();

            if (sectionName == "data") {
                while (token->getTokenValue() != "}") {
                    if (token->getTokenType() == types::TokenType::TT_ID &&
                        (token->getTokenValue() == "int" || token->getTokenValue() == "string" || token->getTokenValue() == "float")) {
                        next();
                        require(types::TokenType::TT_ID); 
                        next();
                        require("="); 
                        next();
                        if (token->getTokenType() == types::TokenType::TT_NUM ||
                            token->getTokenType() == types::TokenType::TT_HEX ||
                            token->getTokenType() == types::TokenType::TT_STR) {
                            next();
                        } else {
                            throw mx::Exception("Syntax Error: Expected value for variable, found: " + token->getTokenValue());
                        }
                    } else if (token->getTokenValue() == "\n" || token->getTokenValue() == ";") {
                        next(); 
                    } else {
                        throw mx::Exception("Syntax Error: Expected variable declaration, found: " + token->getTokenValue());
                    }
                }
                require("}");
                next();
            } else if (sectionName == "code") {
                while (token->getTokenValue() != "}") {
                    if (token->getTokenType() == types::TokenType::TT_ID && next(), token->getTokenValue() == ":") {
                        next();
                        continue;
                    }
                    
                    if (token->getTokenType() == types::TokenType::TT_ID || token->getTokenType() == types::TokenType::TT_NUM) {
                        next();
                    } else {
                        throw mx::Exception("Syntax Error: Expected instruction or label, found: " + token->getTokenValue());
                    }

                    int operandCount = 0;
                    while (token->getTokenValue() == ",") next();

                    while (token->getTokenType() == types::TokenType::TT_ID ||
                           token->getTokenType() == types::TokenType::TT_NUM ||
                           token->getTokenType() == types::TokenType::TT_HEX ||
                           token->getTokenType() == types::TokenType::TT_STR) {
                        operandCount++;
                        next();
                        while (token->getTokenValue() == ",") next();
                    }
                    while (token->getTokenValue() == "\n" || token->getTokenValue() == ";" || token->getTokenValue().starts_with("//")) {
                        next();
                    }
                }
                require("}");
                next();
            } else {
                throw mx::Exception("Syntax Error: Unknown section: " + sectionName);
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
            throw mx::Exception("Syntax Error: Required: " + r +  " Found: " + token->getTokenValue());
            
    }

    bool Validator::match(const types::TokenType &t) {
        if(t != token->getTokenType())
            return false;
        return true;
    }

    void Validator::require(const types::TokenType &t) {
         if(t != token->getTokenType()) 
            throw mx::Exception("Syntax Error: Required: " + std::to_string(static_cast<int>(t)) + " instead found: " + token->getTokenValue() + ":" + std::to_string(static_cast<int>(token->getTokenType())));
    }
    
    bool Validator::next() {
        while(index < scanner.size()) {
            token = &scanner[index];
            if(token->getTokenValue() == "\n") {
                index++;   
                continue;
            }
            else 
                break;
        }
        if(index < scanner.size()) {
            token = &scanner[index++];
            return true;
        }
        return false; 
    }
}