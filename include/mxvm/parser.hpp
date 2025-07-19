#ifndef _PARSER_H_X
#define _PARSER_H_X

#include<iostream>
#include<string>
#include<cstdint>
#include"scanner/scanner.hpp"

namespace mxvm {

    class Parser {
    public:
        explicit Parser(const std::string &source);
        uint64_t scan();
        void parse();
        auto operator[](size_t pos);
    protected:
        std::string source_file;
        scan::Scanner scanner;
    };
}

#endif