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
#include<algorithm>

namespace mxvm {
    class ProgramNode;
    class SectionNode;
    class InstructionNode;
    class VariableNode;
    class CommentNode;
    class LabelNode;
    class Program;
    class ModuleNode;
    class ObjectNode;
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

        
        std::unique_ptr<ProgramNode> parseProgramOrObject(uint64_t& index);
        std::unique_ptr<ProgramNode> parseAST();
        bool generateProgramCode(const Mode &m, std::unique_ptr<Program> &program);
        bool generateDebugHTML(std::ostream &out, std::unique_ptr<Program> &program);
        void generateObjectAssemblyFile(std::unique_ptr<Program>& objProgram);
        void registerObjectExterns(std::unique_ptr<Program>& mainProgram, const std::unique_ptr<Program>& objProgram);
        std::string module_path = ".";
        std::string object_path = ".";
        std::string include_path = "/usr/local/include/mxvm/modules";
        bool object_mode = false;
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
        std::unique_ptr<ObjectNode> parseObject(uint64_t &index);
    
        void processDataSection(SectionNode* sectionNode, std::unique_ptr<Program>& program);
        void processCodeSection(SectionNode* sectionNode, std::unique_ptr<Program>& program);
        void processModuleSection(SectionNode* sectionNode, std::unique_ptr<Program>& program);
        void processObjectSection(SectionNode *sectionNode, std::unique_ptr<Program>& program);
        void processModuleFile(const std::string &src, std::unique_ptr<Program> &program);
        void processObjectFile(const std::string &src, std::unique_ptr<Program> &program);
        void setVariableValue(Variable& var, VarType type, const std::string& value, size_t buf_size = 0);
        void setDefaultVariableValue(Variable& var, VarType type);
        void resolveLabelReference(Operand& operand, const std::unordered_map<std::string, size_t>& labelMap);
        void collectObjectNames(std::vector<std::pair<std::string, std::string>> &names, const std::unique_ptr<Program> &program);
        void printObjectHTML(std::ostream &out, const std::unique_ptr<Program> &objPtr);
    };

    struct ExternalFunction {
        std::string name;
        std::string mod;
        bool module;
        bool operator==(const ExternalFunction &f) {
            return (name == f.name && mod == f.mod);
        }
    };

    class ModuleParser {
    public:
        explicit ModuleParser(const std::string &mod_name, const std::string &source);
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
        std::string mod_name;
        scan::Scanner scanner;
        uint64_t index = 0;
        scan::TToken *token = nullptr;
    };
}

#endif