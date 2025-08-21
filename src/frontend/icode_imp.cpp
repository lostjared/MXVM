#include "icode.hpp"

namespace pascal {
        bool IOFunctionHandler::canHandle(const std::string& funcName) const {
            return funcName == "writeln" || funcName == "write" || funcName == "readln";
        }

        void IOFunctionHandler::generate(CodeGenVisitor& visitor, const std::string& funcName, const std::vector<std::unique_ptr<ASTNode>>& arguments) {
            if (funcName == "writeln" || funcName == "write") {
                
                for(auto &arg : arguments) {
                    ASTNode *a = arg.get();
                    if (auto strNode = dynamic_cast<StringNode*>(a)) {
                        if (strNode->value.length() == 1) {
                            visitor.usedStrings.insert("fmt_chr");
                            std::string v = visitor.eval(a);
                            visitor.emit2("print", "fmt_chr", v);
                            if (visitor.isReg(v)) visitor.freeReg(v);
                        } else {
                            visitor.usedStrings.insert("fmt_str");
                            std::string v = visitor.eval(a);
                            visitor.emit2("print", "fmt_str", v);
                            if (visitor.isReg(v)) visitor.freeReg(v);
                        }
                    }
                    else if (auto varNode = dynamic_cast<VariableNode*>(a)) {
                        if (visitor.getVarType(varNode->name) == CodeGenVisitor::VarType::CHAR) {
                            visitor.usedStrings.insert("fmt_chr");
                            std::string v = visitor.eval(a);
                            visitor.emit2("print", "fmt_chr", v);
                            if (visitor.isReg(v)) visitor.freeReg(v);
                        } 
                        else if (visitor.isStringVar(varNode->name)) {
                            visitor.usedStrings.insert("fmt_str");
                            std::string v = visitor.eval(a);
                            visitor.emit2("print", "fmt_str", v);
                            if (visitor.isReg(v)) visitor.freeReg(v);
                        }
                        else if (visitor.getVarType(varNode->name) == CodeGenVisitor::VarType::DOUBLE) {
                            visitor.usedStrings.insert("fmt_float");
                            std::string v = visitor.eval(a);
                            visitor.emit2("print", "fmt_float", v);
                            if (visitor.isReg(v)) visitor.freeReg(v);
                        }
                        else {
                            visitor.usedStrings.insert("fmt_int");
                            std::string v = visitor.eval(a);
                            visitor.emit2("print", "fmt_int", v);
                            if (visitor.isReg(v)) visitor.freeReg(v);
                        }
                    }

                    else {
                        visitor.usedStrings.insert("fmt_int");
                        std::string v = visitor.eval(a);
                        visitor.emit2("print", "fmt_int", v);
                        if (visitor.isReg(v)) visitor.freeReg(v);
                    }
                }

                if (funcName == "writeln") {
                    visitor.usedStrings.insert("newline");
                    visitor.emit1("print", "newline");
                }
            }
            else if (funcName == "readln") {
                if (arguments.empty()) {
                    visitor.emit1("getline", "input_buffer");
                    return;
                }
                
                for (auto &arg : arguments) {
                    if (auto varNode = dynamic_cast<VariableNode*>(arg.get())) {
                        std::string varName = varNode->name;
                        int slot = visitor.newSlotFor(varName);
                        std::string memLoc = visitor.slotVar(slot);
                        
                        auto varType = visitor.getVarType(varName);
                        visitor.emit1("getline", "input_buffer");
                        
                        if (varType == CodeGenVisitor::VarType::STRING || 
                            varType == CodeGenVisitor::VarType::PTR) {
                        } 
                        else if (varType == CodeGenVisitor::VarType::INT || 
                                 varType == CodeGenVisitor::VarType::UNKNOWN) {
                            visitor.emit2("to_int", memLoc, "input_buffer");
                        } 
                        else if (varType == CodeGenVisitor::VarType::DOUBLE) {
                            visitor.emit2("to_float", memLoc, "input_buffer");
                        }
                        else if (varType == CodeGenVisitor::VarType::CHAR) {
                            std::string charReg = visitor.allocReg();
                            visitor.emit2("load_char", charReg, "input_buffer");
                            visitor.emit2("mov", memLoc, charReg);
                            visitor.freeReg(charReg);
                        }
                        visitor.recordLocation(varName, {CodeGenVisitor::ValueLocation::MEMORY, memLoc});
                    } 
                    else {
                        throw std::runtime_error("readln argument must be a variable");
                    }
                }
            }
        }

}