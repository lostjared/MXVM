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
    class ModuleNode;
    struct Variable;
    struct Operand;

    extern bool debug_mode;
    extern bool instruct_mode;
    extern bool html_mode;

    enum class Mode { MODE_INTERPRET, MODE_COMPILE };

    class Parser {
    public:
        explicit Parser(const std::string &source);
        uint64_t scan();
        void parse();
        auto operator[](size_t pos);
        
        std::unique_ptr<ProgramNode> parseAST();
        bool generateProgramCode(const Mode &m, std::unique_ptr<Program> &program);
        bool generateDebugHTML(std::ostream &out, std::unique_ptr<Program> &program);        
        std::string module_path = ".";
    private:
        std::string source_file;
        scan::Scanner scanner;
        Validator validator;
        Mode parser_mode = Mode::MODE_INTERPRET;        
        std::unique_ptr<SectionNode> parseSection(uint64_t& index);
        std::unique_ptr<VariableNode> parseDataVariable(uint64_t& index);
        std::unique_ptr<InstructionNode> parseCodeInstruction(uint64_t& index);
        std::unique_ptr<CommentNode> parseComment(uint64_t& index);
        std::unique_ptr<LabelNode> parseLabel(uint64_t& index);
        std::unique_ptr<ModuleNode> parseModule(uint64_t& index);
        
        void processDataSection(SectionNode* sectionNode, std::unique_ptr<Program>& program);
        void processCodeSection(SectionNode* sectionNode, std::unique_ptr<Program>& program);
        void processModuleSection(SectionNode* sectionNode, std::unique_ptr<Program>& program);
        void processModuleFile(const std::string &src, std::unique_ptr<Program> &program);
        void setVariableValue(Variable& var, VarType type, const std::string& value, size_t buf_size = 0);
        void setDefaultVariableValue(Variable& var, VarType type);
        void resolveLabelReference(Operand& operand, const std::unordered_map<std::string, size_t>& labelMap);
    };

    struct ExternalFunction {
        std::string name;
    };

    class ModuleParser {
    public:
        explicit ModuleParser(const std::string &source);
        uint64_t scan();
        bool parse();
        scan::TToken operator[](size_t pos);
        scan::TToken at(size_t pos);
        bool generateProgramCode(const Mode &m, const std::string &mod_id, const std::string &mod_name, std::unique_ptr<Program> &program);
        bool next();
        bool match(const std::string &s);
        bool match(const types::TokenType &t);
        void require(const std::string &s);
        void require(const types::TokenType &t);
    protected:
        std::vector<ExternalFunction> functions;
        scan::Scanner scanner;
        uint64_t index = 0;
        scan::TToken *token = nullptr;
    };
}

#endif