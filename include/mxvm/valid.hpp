#ifndef __VALID_H__
#define __VALID_H__

#include"scanner/scanner.hpp"

namespace mxvm {
    class Validator {
    public:
        Validator(const std::string &text) : scanner(text), index(0) {}
        bool validate();
        bool match(const std::string &m);
        void require(const std::string &r);
        bool match(const types::TokenType &t);
        void require(const types::TokenType &t);
        bool next();
    protected:
        scan::Scanner scanner;
        uint64_t index;
        scan::TToken *token;
    };
}

#endif