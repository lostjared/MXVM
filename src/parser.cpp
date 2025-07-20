#include"mxvm/parser.hpp"
#include"scanner/exception.hpp"
#include"scanner/scanner.hpp"
#include"mxvm/ast.hpp"
#include<unordered_map>

namespace mxvm {
    Parser::Parser(const std::string &source) : source_file(source), scanner(source)  {

    }
    
    uint64_t Parser::scan() {
        try {
            scanner.scan();
        } catch(scan::ScanExcept &e){
            std::cerr << "Scanner error: " << e.why() << "\n";
        }
        return scanner.size();
    }

    auto Parser::operator[](size_t pos) {
        if(pos < scanner.size()) {
            return scanner[pos];
        }
        throw mx::Exception("Error: scanner index out of bounds.\n");
        return scanner[0];
    }

    std::unique_ptr<ProgramNode> Parser::parseAST() {
        auto program = std::make_unique<ProgramNode>();
        
        for (uint64_t i = 0; i < scanner.size(); ++i) {
            auto token = this->operator[](i);
            
            if (token.getTokenValue() == "\n" || token.getTokenValue() == " ") {
                continue;
            }
            
            std::string tokenValue = token.getTokenValue();
            
            if (tokenValue == "section") {
                auto section = parseSection(i);
                if (section) {
                    program->addSection(std::move(section));
                }
            }
        }
        
        return program;
    }
    
    std::unique_ptr<SectionNode> Parser::parseSection(uint64_t& index) {
        index++; 
        
        if (index >= scanner.size()) return nullptr;
        
        auto sectionNameToken = this->operator[](index);
        std::string sectionName = sectionNameToken.getTokenValue();
        
        SectionNode::SectionType sectionType;
        if (sectionName == "data") {
            sectionType = SectionNode::DATA;
        } else if (sectionName == "code") {
            sectionType = SectionNode::CODE;
        } else {
            return nullptr; 
        }
        
        auto section = std::make_unique<SectionNode>(sectionType);
        index++;
        
        
        if (index < scanner.size() && this->operator[](index).getTokenValue() == "{") {
            index++; 
            
            
            while (index < scanner.size()) {
                auto token = this->operator[](index);
                std::string value = token.getTokenValue();
                
            
                if (value == "}") {
                    index++; 
                    break;
                }
                
                
                if (value == "\n") {
                    index++;
                    continue;
                }
                
                
                if (value == "//" || value.starts_with("//")) {
                    auto comment = parseComment(index);
                    if (comment) {
                        section->addStatement(std::move(comment));
                    }
                    continue;
                }
                
                if (sectionType == SectionNode::DATA) {
                    auto variable = parseDataVariable(index);
                    if (variable) {
                        section->addStatement(std::move(variable));
                    }
                } else if (sectionType == SectionNode::CODE) {
                    auto instruction = parseCodeInstruction(index);
                    if (instruction) {
                        section->addStatement(std::move(instruction));
                    }
                }
            }
        }
        
        return section;
    }
    
    std::unique_ptr<VariableNode> Parser::parseDataVariable(uint64_t& index) {
        if (index >= scanner.size()) return nullptr;
        
        auto typeToken = this->operator[](index);
        std::string typeStr = typeToken.getTokenValue();
        
        VarType varType;
        if (typeStr == "int") varType = VarType::VAR_INTEGER;
        else if (typeStr == "float") varType = VarType::VAR_FLOAT;
        else if (typeStr == "string") varType = VarType::VAR_STRING;
        else if (typeStr == "ptr") varType = VarType::VAR_POINTER;
        else if (typeStr == "array") varType = VarType::VAR_ARRAY;
        else return nullptr;
        
        index++; 
        
        if (index >= scanner.size()) return nullptr;
        
        auto nameToken = this->operator[](index);
        if (nameToken.getTokenType() != types::TokenType::TT_ID) {
            return nullptr;
        }
        
        std::string varName = nameToken.getTokenValue();
        index++; 
        
        
        if (index < scanner.size() && this->operator[](index).getTokenValue() == "=") {
            index++; 
            
            if (index < scanner.size()) {
                auto valueToken = this->operator[](index);
                std::string value = valueToken.getTokenValue();
                
                
                if (valueToken.getTokenType() == types::TokenType::TT_STR || 
                    (value.starts_with("\"") && value.ends_with("\""))) {
                    index++;
                    return std::make_unique<VariableNode>(varType, varName, value);
                } else {
                    
                    index++;
                    return std::make_unique<VariableNode>(varType, varName, value);
                }
            }
        }
        
        return std::make_unique<VariableNode>(varType, varName);
    }
    
    std::unique_ptr<InstructionNode> Parser::parseCodeInstruction(uint64_t& index) {
        static std::unordered_map<std::string, Inc> instructionMap = {
            {"mov", MOV}, {"load", LOAD}, {"store", STORE},
            {"add", ADD}, {"sub", SUB}, {"mul", MUL}, {"div", DIV},
            {"or", OR}, {"and", AND}, {"xor", XOR}, {"not", NOT},
            {"cmp", CMP}, {"jmp", JMP}, {"je", JE}, {"jne", JNE},
            {"jl", JL}, {"jle", JLE}, {"jg", JG}, {"jge", JGE},
            {"jz", JZ}, {"jnz", JNZ}, {"ja", JA}, {"jb", JB},
            {"print", PRINT}, {"exit", EXIT}
        };
        
        if (index >= scanner.size()) return nullptr;
        
        auto token = this->operator[](index);
        std::string instructionName = token.getTokenValue();
        
        auto instIt = instructionMap.find(instructionName);
        if (instIt == instructionMap.end()) {
            index++;
            return nullptr;
        }
        
        Inc instruction = instIt->second;
        index++; // Move past instruction name
        
        std::vector<Operand> operands;
        
        // Parse operands until newline or comment
        while (index < scanner.size()) {
            auto operandToken = this->operator[](index);
            std::string value = operandToken.getTokenValue();
            
            if (value == "\n" || value == "//" || value.starts_with("//")) {
                break;
            }
            
            if (value == ",") {
                index++;
                continue;
            }
            
            Operand operand;
            if (operandToken.getTokenType() == types::TokenType::TT_NUM) {
                operand.op = value;
                operand.op_value = std::stoi(value);
            } else if (operandToken.getTokenType() == types::TokenType::TT_ID) {
                operand.op = value;
                operand.label = value;
            } else if (operandToken.getTokenType() == types::TokenType::TT_SYM && value != ",") {
                operand.op = value;
            }
            
            if (!operand.op.empty()) {
                operands.push_back(operand);
            }
            
            index++;
        }
        
        return std::make_unique<InstructionNode>(instruction, operands);
    }
    
    std::unique_ptr<CommentNode> Parser::parseComment(uint64_t& index) {
        std::string commentText;
        if (this->operator[](index).getTokenValue() == "//") {
            index++;
        }       
        while (index < scanner.size()) {
            auto token = this->operator[](index);
            if (token.getTokenValue() == "\n") {
                break;
            }
            commentText += token.getTokenValue() + " ";
            index++;
        }
        return std::make_unique<CommentNode>(commentText);
    }

    void Parser::parse() {
        auto ast = parseAST();
        if (ast) {
            std::cout << ast->toString() << std::endl;
        }
    }
}