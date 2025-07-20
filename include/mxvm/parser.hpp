#ifndef _PARSER_H_X
#define _PARSER_H_X

#include<iostream>
#include<string>
#include<cstdint>
#include<memory>
#include"scanner/scanner.hpp"
#include"scanner/exception.hpp"
#include"mxvm/instruct.hpp"


namespace mxvm {
    class ProgramNode;
    class SectionNode;
    class InstructionNode;
    class VariableNode;
    class CommentNode;

    class Parser {
    public:
        explicit Parser(const std::string &source);
        uint64_t scan();
        void parse();
        auto operator[](size_t pos);
        std::unique_ptr<ProgramNode> parseAST();        
    private:
        std::string source_file;
        scan::Scanner scanner;
        std::unique_ptr<SectionNode> parseSection(uint64_t& index);
        std::unique_ptr<VariableNode> parseDataVariable(uint64_t& index);
        std::unique_ptr<InstructionNode> parseCodeInstruction(uint64_t& index);
        std::unique_ptr<CommentNode> parseComment(uint64_t& index);
    };
}

#endif