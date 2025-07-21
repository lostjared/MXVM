#ifndef __VALID_H__
#define __VALID_H__

#include"scanner/scanner.hpp"

namespace mxvm {
    class Validator {
    public:
        Validator(const std::string &text) : scanner(text), index(0) {}
        bool validate();
        void collect_labels(std::unordered_map<std::string, std::string> &labels);
        bool match(const std::string &m);
        void require(const std::string &r);
        bool match(const types::TokenType &t);
        void require(const types::TokenType &t);
        bool next();
        bool peekIs(const std::string &s);
        bool peekIs(const types::TokenType &t);
    protected:
        scan::Scanner scanner;
        uint64_t index;
        scan::TToken *token;
    };
}

#endif