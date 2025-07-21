#ifndef _PARSER_H_X
#define _PARSER_H_X

#include<iostream>
#include<string>
#include<cstdint>
#include<memory>
#include<unordered_map>
#include"scanner/scanner.hpp"
#include"scanner/exception.hpp"
#include"mxvm/instruct.hpp"
#include"mxvm/valid.hpp"


namespace mxvm {
    class ProgramNode;
    class SectionNode;
    class InstructionNode;
    class VariableNode;
    class CommentNode;
    class LabelNode;
    class Program;
    struct Variable;
    struct Operand;

    extern bool debug_mode;
    extern bool instruct_mode;
    extern bool html_mode;

    class Parser {
    public:
        explicit Parser(const std::string &source);
        uint64_t scan();
        void parse();
        auto operator[](size_t pos);
        
        std::unique_ptr<ProgramNode> parseAST();
        bool generateProgramCode(std::unique_ptr<Program> &program);
        bool generateDebugHTML(std::ostream &out, std::unique_ptr<Program> &program);        
    private:
        std::string source_file;
        scan::Scanner scanner;
        Validator validator;        
        std::unique_ptr<SectionNode> parseSection(uint64_t& index);
        std::unique_ptr<VariableNode> parseDataVariable(uint64_t& index);
        std::unique_ptr<InstructionNode> parseCodeInstruction(uint64_t& index);
        std::unique_ptr<CommentNode> parseComment(uint64_t& index);
        std::unique_ptr<LabelNode> parseLabel(uint64_t& index);
        
        void processDataSection(SectionNode* sectionNode, std::unique_ptr<Program>& program);
        void processCodeSection(SectionNode* sectionNode, std::unique_ptr<Program>& program);
        void setVariableValue(Variable& var, VarType type, const std::string& value);
        void setDefaultVariableValue(Variable& var, VarType type);
        void resolveLabelReference(Operand& operand, const std::unordered_map<std::string, size_t>& labelMap);
    };
}

#endif