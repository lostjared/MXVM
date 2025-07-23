#include"mxvm/parser.hpp"
#include"scanner/exception.hpp"
#include"scanner/scanner.hpp"
#include"mxvm/ast.hpp"
#include"mxvm/icode.hpp"
#include<unordered_map>
#include<fstream>

namespace mxvm {

    bool debug_mode = false;
    bool instruct_mode = false;
    bool html_mode = false;

    ModuleParser::ModuleParser(const std::string &source) : scanner(source) {}
    
    uint64_t ModuleParser::scan() {
        scanner.scan();
        return scanner.size();
    }
    
    bool ModuleParser::parse() {
        next();
        require("module");
        next();
        require(types::TokenType::TT_ID);
        std::string mod_name = token->getTokenValue();
        next();
        require("{");    
        while(!match("}")) {
            if(match(types::TokenType::TT_ID) && token->getTokenValue() == "extern") {
                next();
                require(types::TokenType::TT_ID);
                std::string func_name = token->getTokenValue();
                functions.push_back({func_name});
                next();
                continue;
            }
            next();
        }
        return true;
    }
    
    scan::TToken ModuleParser::operator[](size_t pos) {
        return at(pos);
    }
        
    scan::TToken ModuleParser::at(size_t pos) {
        if(pos < scanner.size())
            return scanner[pos];

        throw scan::ScanExcept("Out of bounds");
        return scanner[0];
    }

    bool ModuleParser::generateProgramCode(const std::string &mod_id, const std::string &mod_name, std::unique_ptr<Program> &program) {
        for(auto &f : functions) {
            program->add_runtime_extern(mod_name, "mxvm_" + mod_id + "_" + f.name, f.name);
        }
        return true;
    }

    bool ModuleParser::next() {
        while (index < scanner.size() && scanner[index].getTokenType() != types::TokenType::TT_STR && scanner[index].getTokenValue() == "\n") {
            index++;
        }
        if (index < scanner.size()) {
            token = &scanner[index++];  
            return true;
        }
        return false;
    }

    bool ModuleParser::match(const std::string &s) {
        return (token->getTokenValue() == s);
    }

    bool ModuleParser::match(const types::TokenType &t) {
        return (token->getTokenType() == t);
    }

    void ModuleParser::require(const std::string &s) {
        if(!match(s)) {
            throw mx::Exception("Syntax Error: Looking for: " + s + " on line " + std::to_string(token->getLine()));
        }
    }
    void ModuleParser::require(const types::TokenType &t) {
        if(!match(t)) {
            throw mx::Exception("Syntax Error: Looking for: " + std::to_string(static_cast<unsigned int>(t)) + " on line " + std::to_string(token->getLine()));
        }
    }

    Parser::Parser(const std::string &source) : source_file(source), scanner(source), validator(source)  {

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

            if (tokenValue == "program") {
                i++;
                if (i < scanner.size()) {
                    std::string program_name;
                    auto nameToken = this->operator[](i);
                    program_name = nameToken.getTokenValue();
                    program->name = program_name; 
                }
                i++;
                if (i < scanner.size() && this->operator[](i).getTokenValue() == "{") {
                    i++;                
                    while (i < scanner.size()) {
                        auto innerToken = this->operator[](i);
                        std::string innerValue = innerToken.getTokenValue();
                        if (innerValue == "}") {
                            i++; 
                            break;
                        }
                        if (innerValue == "section") {
                            auto section = parseSection(i);
                            if (section) {
                                program->addSection(std::move(section));
                            }
                            continue;
                        }
                        i++;
                    }
                }
                break; 
            }
            else if (tokenValue == "section") {
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
        } else if(sectionName == "module") {
            sectionType = SectionNode::MODULE;
        } 
        else {
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
                
                if (value == "\n" || value == " " || value == "\t") {
                    index++;
                    continue;
                }
            
                if (value == "//" || value.starts_with("//") || value == "#" || value.starts_with("#")) {
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
                    } else {
                        index++;
                    }
                } else if(sectionType == SectionNode::MODULE) {
                    auto module = parseModule(index);
                    if(module) {
                        section->addStatement(std::move(module));
                    } else {
                        index++;
                    }
                }
                else if (sectionType == SectionNode::CODE) {
                    if(token.getTokenType() == types::TokenType::TT_ID && token.getTokenValue() == "function"  && index + 2 < scanner.size() && this->operator[](index + 1).getTokenType() == types::TokenType::TT_ID && this->operator[](index + 2).getTokenValue() == ":") {
                        index++;
                        auto label = parseLabel(index);
                        if(label) {
                            label->function = true;
                            section->addStatement(std::move(label));
                        }
                    } else if (token.getTokenType() == types::TokenType::TT_ID && 
                        index + 1 < scanner.size() && 
                        this->operator[](index + 1).getTokenValue() == ":") {
                        auto label = parseLabel(index);
                        if (label) {
                            section->addStatement(std::move(label));
                        }
                    } else {
                        auto instruction = parseCodeInstruction(index);
                        if (instruction) {
                            section->addStatement(std::move(instruction));
                        } else {
                            index++;
                        }
                    }
                }
            }
        }
        
        return section;
    }

    std::unique_ptr<ModuleNode> Parser::parseModule(uint64_t& index) {
        if(index >= scanner.size()) return nullptr;
        std::string name = this->operator[](index).getTokenValue();
        index++;
        if(index < scanner.size()) {
            if(this->operator[](index).getTokenValue() == ",")
                index++;
        }
        return std::make_unique<ModuleNode>(name);
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
                types::TokenType tokenType = valueToken.getTokenType();
                switch (tokenType) {
                    case types::TokenType::TT_STR:
                        if (value.starts_with("\"") && value.ends_with("\"")) {
                            value = value.substr(1, value.length() - 2);
                        }
                        index++;
                        return std::make_unique<VariableNode>(varType, varName, value);
                        
                    case types::TokenType::TT_NUM:
                        index++;
                        return std::make_unique<VariableNode>(varType, varName, value);
                        
                    case types::TokenType::TT_HEX:
                        index++;
                        return std::make_unique<VariableNode>(varType, varName, value);
                        
                    case types::TokenType::TT_ID:
                        index++;
                        return std::make_unique<VariableNode>(varType, varName, value);   
                    default:
                        index++;
                        return std::make_unique<VariableNode>(varType, varName, value);
                }
            }
        } else if(index < scanner.size()  && this->operator[](index).getTokenValue() == "," && varType == VarType::VAR_STRING) {
            index++;
            size_t buf_size = 0;
            if(index < scanner.size()) {
                buf_size = std::stoll(this->operator[](index).getTokenValue(), nullptr, 0);
            }
            if(buf_size == 0) {
                throw mx::Exception("string buffer: " + varName + " requires valid size");
            }
            return std::make_unique<VariableNode>(varType, varName, buf_size);
        }
        return std::make_unique<VariableNode>(varType, varName);
    }
    
    std::unique_ptr<InstructionNode> Parser::parseCodeInstruction(uint64_t& index) {
        static std::unordered_map<std::string, Inc> instructionMap = {
            {"mov", MOV}, {"load", LOAD}, {"store", STORE},
            {"add", ADD}, {"sub", SUB}, {"mul", MUL}, {"div", DIV},
            {"or", OR}, {"and", AND}, {"xor", XOR}, {"not", NOT}, {"mod", MOD},
            {"cmp", CMP}, {"jmp", JMP}, {"je", JE}, {"jne", JNE},
            {"jl", JL}, {"jle", JLE}, {"jg", JG}, {"jge", JGE},
            {"jz", JZ}, {"jnz", JNZ}, {"ja", JA}, {"jb", JB},
            {"print", PRINT}, {"exit", EXIT}, {"alloc", ALLOC}, {"free", FREE},
            {"getline", GETLINE}, {"push", PUSH}, {"pop", POP}, {"stack_load", STACK_LOAD},
            {"stack_store", STACK_STORE}, {"stack_sub", STACK_SUB}, {"call", CALL}, {"ret", RET},
            {"string_print", STRING_PRINT}, {"done", DONE}, {"to_int", TO_INT}, {"to_float", TO_FLOAT},
            {"invoke", INVOKE}, {"return", RETURN}
        };
        
        if (index >= scanner.size()) return nullptr;

        auto token = this->operator[](index);
        std::string tokenValue = token.getTokenValue();
        
        if (token.getTokenType() == types::TokenType::TT_ID) {
            if (index + 1 < scanner.size() && this->operator[](index + 1).getTokenValue() == ":") {
                return nullptr;
            }
        }
        
        auto instIt = instructionMap.find(tokenValue);
        if (instIt == instructionMap.end()) {
            throw mx::Exception("Error invalid instruction: " + tokenValue);
            return nullptr;
        }
        
        Inc instruction = instIt->second;
        index++; 
        
        std::vector<Operand> operands;
        
        while (index < scanner.size()) {
            auto operandToken = this->operator[](index);
            std::string value = operandToken.getTokenValue();
            types::TokenType tokenType = operandToken.getTokenType();
            
            if (value == "\n" || value == "//" || value.starts_with("//") || 
                value == "#" || value.starts_with("#") || value == "}") {
                break;
            }
            
            if (value == "," || value == " " || value == "\t") {
                index++;
                continue;
            }
            
            Operand operand;
            
            switch (tokenType) {
                case types::TokenType::TT_NUM:
                    operand.op = value;
                    operand.op_value = std::stoll(value);
                    operand.type = OperandType::OP_CONSTANT;
                    break;
                    
                case types::TokenType::TT_HEX:
                    operand.op = value;
                    if (value.starts_with("0x") || value.starts_with("0X")) {
                        operand.op_value = std::stoll(value, nullptr, 16);
                    } else {
                        operand.op_value = std::stoll(value, nullptr, 16);
                    }
                    operand.type = OperandType::OP_CONSTANT;
                    break;
                    
                case types::TokenType::TT_ID:
                    operand.op = value;
                    operand.label = value;
                    operand.type = OperandType::OP_VARIABLE;
                    break;
                    
                case types::TokenType::TT_STR:
                    operand.op = value;
                    operand.type = OperandType::OP_CONSTANT;
                    break;
                    
                case types::TokenType::TT_SYM:
                    if (value != ",") {
                        operand.op = value;
                    }
                    break;
                default:
                    operand.op = value;
                    break;
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
    
        auto token = this->operator[](index);
        if (token.getTokenValue() == "//" || token.getTokenValue() == "#" || 
            token.getTokenValue().starts_with("//") || token.getTokenValue().starts_with("#")) {
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

    std::unique_ptr<LabelNode> Parser::parseLabel(uint64_t& index) {
        if (index >= scanner.size()) return nullptr;
    
        auto token = this->operator[](index);
    
        
        if (token.getTokenType() != types::TokenType::TT_ID) {
            return nullptr;
        }
    
        std::string labelName = token.getTokenValue();
        index++; 
    
        
        if (index < scanner.size() && this->operator[](index).getTokenValue() == ":") {
            index++;
            return std::make_unique<LabelNode>(labelName);
        }
    
        return nullptr;
    }

    void Parser::parse() {
        auto ast = parseAST();
        if (ast) {
            std::cout << ast->toString() << std::endl;
        }
    }

    bool Parser::generateProgramCode(std::unique_ptr<Program> &program) {
        try {
            if(!validator.validate()) {
                return false;
            }
        } catch (mx::Exception &e) {
            std::cerr << e.what() << "\n";
            return false;
        }
        
        auto ast = parseAST();
        if(ast) {
            if(debug_mode) {
                std::cout << "Program name: " << ast->name << "\n";
                std::cout << ast->toString() << "\n";
            }
            program->name = ast->name;
            for (const auto& section : ast->sections) {
                auto sectionNode = dynamic_cast<SectionNode*>(section.get());
                if (!sectionNode) continue;
                
                if (sectionNode->type == SectionNode::DATA) {
                    processDataSection(sectionNode, program);
                } else if (sectionNode->type == SectionNode::CODE) {
                    processCodeSection(sectionNode, program);
                } else if(sectionNode->type == SectionNode::MODULE) {
                    processModuleSection(sectionNode, program);
                }
            }
            if(mxvm::html_mode) {
                std::fstream ofile;
                ofile.open(ast->name + ".html", std::ios::out);
                if(ofile.is_open()) {
                    if(generateDebugHTML(ofile, program)) {
                        std::cout << "generated " << ast->name + ".html\n";
                    }
                    ofile.close();
                }
            }
            return true;
        }
        return false;
    }

    bool Parser::generateDebugHTML(std::ostream &out, std::unique_ptr<Program> &program) {
        out << "<!DOCTYPE html>\n<html lang=\"en\">\n<head>\n";
        out << "<meta charset=\"UTF-8\">\n<title>" << program->name << " MXVM Debug Info</title>\n";
        out << "<style>\n"
            "body { font-family: Arial, sans-serif; background: #f8f8f8; margin: 0; padding: 0; }\n"
            "h1 { background: #333; color: #fff; padding: 20px; margin: 0; }\n"
            "section { margin: 30px; }\n"
            "table { border-collapse: collapse; width: 100%; background: #fff; }\n"
            "th, td { border: 1px solid #ccc; padding: 8px 12px; text-align: left; }\n"
            "th { background: #eee; }\n"
            "tr:nth-child(even) { background: #f2f2f2; }\n"
            ".addr { font-family: monospace; color: #555; }\n"
            ".type { font-weight: bold; color: #0074d9; }\n"
            ".inst { font-family: monospace; }\n"
            "</style>\n</head>\n<body>\n";
        out << "<h1>" << program->name << " MXVM Debug Information</h1>\n";

        // Variables Table
        out << "<section>\n<h2>Memory</h2>\n<table>\n";
        out << "<tr><th>Name</th><th>Type</th><th>Initial Value</th></tr>\n";
        for (const auto& [name, var] : program->vars) {
            out << "<tr>";
            out << "<td>" << name << "</td>";
            out << "<td class=\"type\">";
            switch (var.type) {
                case VarType::VAR_INTEGER: out << "int"; break;
                case VarType::VAR_FLOAT: out << "float"; break;
                case VarType::VAR_STRING: out << "string"; break;
                case VarType::VAR_POINTER: out << "ptr"; break;
                case VarType::VAR_LABEL: out << "label"; break;
                case VarType::VAR_ARRAY: out << "array"; break;
                default: out << "unknown"; break;
            }
            out << "</td><td>";
            switch (var.type) {
                case VarType::VAR_INTEGER: out << var.var_value.int_value; break;
                case VarType::VAR_FLOAT: out << var.var_value.float_value; break;
                case VarType::VAR_STRING: out << Program::escapeNewLines(var.var_value.str_value); break;
                case VarType::VAR_POINTER: out << (var.var_value.ptr_value ? "0x" + std::to_string(reinterpret_cast<uintptr_t>(var.var_value.ptr_value)) : "null"); break;
                case VarType::VAR_LABEL: out << var.var_value.label_value; break;
                default: out << ""; break;
            }
            out << "</td></tr>\n";
        }
        out << "</table>\n</section>\n";
        out << "<section>\n<h2>Instructions</h2>\n<table>\n";
        out << "<tr><th>Address</th><th>Instruction</th><th>Operands</th></tr>\n";
        for (size_t addr = 0; addr < program->inc.size(); ++addr) {
            const auto& instr = program->inc[addr];
            out << "<tr>";
            out << "<td class=\"addr\">0x" << std::uppercase << std::hex << addr << std::dec << "</td>";
            out << "<td class=\"inst\">" << instr.instruction << "</td>";
            out << "<td>";
            std::vector<std::string> ops;
            if (!instr.op1.op.empty()) ops.push_back(instr.op1.op);
            if (!instr.op2.op.empty()) ops.push_back(instr.op2.op);
            if (!instr.op3.op.empty()) ops.push_back(instr.op3.op);
            for (const auto& vop : instr.vop) {
                if (!vop.op.empty()) ops.push_back(vop.op);
            }
            for (size_t i = 0; i < ops.size(); ++i) {
                out << ops[i];
                if (i + 1 < ops.size()) out << ", ";
            }
            out << "</td></tr>\n";
        }
        out << "</table>\n</section>\n";
        out << "<section>\n<h2>Labels</h2>\n<table>\n";
        out << "<tr><th>Name</th><th>Address</th></tr>\n";
        for (const auto& [label, addr] : program->labels) {
            std::string ltype = "";
            if(addr.second)
                ltype = "<b>function</b> ";

            out << "<tr><td>" << ltype << label << "</td><td class=\"addr\">0x" << std::uppercase << std::hex << addr.first << std::dec << "</td></tr>\n";
        }
        out << "</table>\n</section>\n";
        out << "<footer style=\"text-align:center;padding:20px;color:#888;\">Generated by <b>MXVM</b></footer>\n";
        out << "</body>\n</html>\n";
        return true;
    }

    void Parser::processDataSection(SectionNode* sectionNode, std::unique_ptr<Program>& program) {
        for (const auto& statement : sectionNode->statements) {
            auto variableNode = dynamic_cast<VariableNode*>(statement.get());
            if (variableNode) {
                Variable var;
                var.type = variableNode->type;
                var.var_name = variableNode->name;
                
                if (variableNode->hasInitializer) {
                    setVariableValue(var, variableNode->type, variableNode->initialValue);
                } else {
                    setDefaultVariableValue(var, variableNode->type);
                }
                var.var_value.buffer_size = variableNode->buffer_size;
                program->add_variable(var.var_name, var);
            }
        }
    }

    void Parser::processModuleSection(SectionNode* sectionNode, std::unique_ptr<Program>& program) {
        for(const auto &statement : sectionNode->statements) {
            auto moduleNode = dynamic_cast<ModuleNode *>(statement.get());
            if(moduleNode) {
                std::string &name = moduleNode->name;
                processModuleFile(name, program);
            }
        }
    }

    void Parser::processModuleFile(const std::string &src, std::unique_ptr<Program> &program) {
        std::string module_name = "libmxvm_" + src;
        std::string module_path_so = "modules/" + src + "/" + module_name + ".so";
        if(!module_path.ends_with("/"))
            module_path += "/";
        std::string module_src = module_path + "modules/" + src + "/" + src + ".mxvm";
        std::fstream file;
        file.open(module_src, std::ios::in);
        if(!file.is_open()) {
            throw mx::Exception("Error could not find module source file: " + module_src);
        }
        std::ostringstream data;
        data << file.rdbuf();
        ModuleParser mod_parser(data.str());
        if(mod_parser.scan() > 0) {
            if(mod_parser.parse() && mod_parser.generateProgramCode(src, module_path_so, program)) {
                return;
            } else {
                throw mx::Exception("Error parsing module file.\n");
            }
        }
    }

    void Parser::processCodeSection(SectionNode* sectionNode, std::unique_ptr<Program>& program) {
        std::unordered_map<std::string, size_t> labelMap; 
    
        size_t instructionIndex = 0;
        for (const auto& statement : sectionNode->statements) {
            if (auto labelNode = dynamic_cast<LabelNode*>(statement.get())) {
                labelMap[labelNode->name] =  instructionIndex;
                program->add_label(labelNode->name, instructionIndex, labelNode->function);
            } else if (dynamic_cast<InstructionNode*>(statement.get())) {
                instructionIndex++;
            }
        }
    for (const auto& statement : sectionNode->statements) {
            auto instructionNode = dynamic_cast<InstructionNode*>(statement.get());
            if (instructionNode) {
                Instruction instr;
                instr.instruction = instructionNode->instruction;
                if (instructionNode->operands.size() > 0) {
                    instr.op1 = instructionNode->operands[0];
                    resolveLabelReference(instr.op1, labelMap);
                }
                if (instructionNode->operands.size() > 1) {
                    instr.op2 = instructionNode->operands[1];
                    resolveLabelReference(instr.op2, labelMap);
                }
                if (instructionNode->operands.size() > 2) {
                    instr.op3 = instructionNode->operands[2];
                    resolveLabelReference(instr.op3, labelMap);
                }
                if (instructionNode->operands.size() > 3) {
                    for (size_t i = 3; i < instructionNode->operands.size(); ++i) {
                        Operand extraOp = instructionNode->operands[i];
                        resolveLabelReference(extraOp, labelMap);
                        instr.vop.push_back(extraOp);
                    }
                }
                program->add_instruction(instr);
            }
        }
    }

    void Parser::setVariableValue(Variable& var, VarType type, const std::string& value, size_t buf_size) {
        switch (type) {
            case VarType::VAR_INTEGER:
                if (value.starts_with("0x") || value.starts_with("0X")) {
                    var.var_value.int_value = std::stoull(value, nullptr, 16);
                } else {
                    var.var_value.int_value = std::stoull(value);
                }
                var.var_value.type = VarType::VAR_INTEGER;
                break;
                
            case VarType::VAR_FLOAT:
                var.var_value.float_value = std::stod(value);
                var.var_value.type = VarType::VAR_FLOAT;
                break;
                
            case VarType::VAR_STRING:
                var.var_value.str_value = value;
                var.var_value.type = VarType::VAR_STRING;
                var.var_value.buffer_size = buf_size;
                break;
                
            case VarType::VAR_POINTER:
                var.var_value.ptr_value = nullptr;
                if (value == "null" || value == "0") {
                    var.var_value.ptr_value = nullptr;
                    var.var_value.str_value = "null";
                } 
                var.var_value.type = VarType::VAR_POINTER;
                break;
                
            case VarType::VAR_LABEL:
                var.var_value.label_value = value;
                var.var_value.type = VarType::VAR_LABEL;
                break;
                
            default:
                setDefaultVariableValue(var, type);
                break;
        }
    }

    void Parser::setDefaultVariableValue(Variable& var, VarType type) {
        switch (type) {
            case VarType::VAR_INTEGER:
                var.var_value.int_value = 0;
                var.var_value.type = VarType::VAR_INTEGER;
                break;
                
            case VarType::VAR_FLOAT:
                var.var_value.float_value = 0.0;
                var.var_value.type = VarType::VAR_FLOAT;
                break;
                
            case VarType::VAR_STRING:
                var.var_value.str_value = "";
                var.var_value.type = VarType::VAR_STRING;
                break;
                
            case VarType::VAR_POINTER:
                var.var_value.ptr_value = nullptr;
                var.var_value.type = VarType::VAR_POINTER;
                break;
                
            case VarType::VAR_LABEL:
                var.var_value.label_value = "";
                var.var_value.type = VarType::VAR_LABEL;
                break;
                
            default:
                var.var_value.type = VarType::VAR_NULL;
                break;
        }
    }

    void Parser::resolveLabelReference(Operand& operand, const std::unordered_map<std::string, size_t>& labelMap) {
        if (!operand.label.empty()) {
            auto it = labelMap.find(operand.label);
            if (it != labelMap.end()) {
                operand.op_value = static_cast<int>(it->second);
            }
        }
    }
}