#ifndef __CODEGEN_H_
#define __CODEGEN_H_
#include "ast.hpp"
#include <sstream>
#include <unordered_set>
#include <unordered_map>
#include <vector>
#include <string>
#include <utility>
#include <algorithm>

namespace pascal {
    class CodeGenVisitor : public ASTVisitor {
    public:
        CodeGenVisitor()
            : regInUse(registers.size(), false),
              currentFunctionName(),
              functionSetReturn(false),
              nextSlot(0), nextTemp(0), labelCounter(0) {}

        virtual ~CodeGenVisitor() = default;
        std::string name = "App";

        void generate(ASTNode* root) {
            if (!root) return;
            instructions.clear();
            usedStrings.clear();
            prolog.clear();
            deferredProcs.clear();
            deferredFuncs.clear();
            valueLocations.clear();
            varTypes.clear();  
            
            for (size_t i = 0; i < regInUse.size(); i++) {
                regInUse[i] = false;
            }
            
            if (auto prog = dynamic_cast<ProgramNode*>(root)) {
                name = prog->name;
            }
            
            root->accept(*this);
            emit("done");
            
            for (auto pn : deferredProcs) {
                emitLabel(funcLabel("function PROC_", pn->name));
                
                for (size_t i = 0; i < regInUse.size(); i++) {
                    regInUse[i] = false;
                }
                
                int paramIndex = 0;
                for (auto &p : pn->parameters) {
                    if (auto param = dynamic_cast<ParameterNode*>(p.get())) {
                        for (auto &id : param->identifiers) {
                            if (paramIndex < 6) {
                                if (!param->type.empty() && param->type == "string") {
                                    varSlot[id] = newSlotFor(ptrRegisters[paramIndex]);
                                    setVarType(id, VarType::STRING);
                                } else {
                                    varSlot[id] = -2 - (paramIndex + 1);
                                    regInUse[paramIndex + 1] = true;
                                    if (!param->type.empty()) {
                                        if (param->type == "integer" || param->type == "boolean") {
                                            setVarType(id, VarType::INT);
                                        } else if (param->type == "real") {
                                            setVarType(id, VarType::DOUBLE);
                                        }
                                   }
                                }
                                paramIndex++;
                            } else {
                                int slot = newSlotFor(id);
                                emit3("mov", slotVar(slot), "param" + std::to_string(paramIndex));
                                if (!param->type.empty()) {
                                    if (param->type == "string") {
                                        setVarType(id, VarType::STRING);
                                        setSlotType(slot, VarType::STRING);
                                    } else if (param->type == "integer" || param->type == "boolean") {
                                        setVarType(id, VarType::INT);
                                        setSlotType(slot, VarType::INT);
                                    } else if (param->type == "real") {
                                        setVarType(id, VarType::DOUBLE);
                                        setSlotType(slot, VarType::DOUBLE);
                                    }
                                }
                                paramIndex++;
                            }
                        }
                    }
                }
                
                if (pn->block) pn->block->accept(*this);
                emit("ret");
            }
            
            for (auto fn : deferredFuncs) {
                emitLabel(funcLabel("function FUNC_", fn->name));
                for (size_t i = 0; i < regInUse.size(); i++) {
                    regInUse[i] = false;
                }
                currentFunctionName = fn->name;
                functionSetReturn = false;

                if (!fn->returnType.empty()) { 
                    if (fn->returnType == "string") {
                        setVarType(fn->name, VarType::STRING);
                    }
                    else if (fn->returnType == "integer" || fn->returnType == "boolean") {
                        setVarType(fn->name, VarType::INT);
                    }
                    else if (fn->returnType == "real") {
                        setVarType(fn->name, VarType::DOUBLE);
                    }
                }

                int paramIndex = 0;
                for (auto &p : fn->parameters) {
                    if (auto param = dynamic_cast<ParameterNode*>(p.get())) {
                        for (auto &id : param->identifiers) {
                            if (paramIndex < 6) { 
                                varSlot[id] = -2 - (paramIndex + 1);
                                regInUse[paramIndex + 1] = true;
                                if (!param->type.empty()) {
                                    if (param->type == "string") {
                                        setVarType(id, VarType::STRING);
                                    }
                                    else if (param->type == "integer" || param->type == "boolean") {
                                        setVarType(id, VarType::INT);
                                    }
                                    else if (param->type == "real") {
                                        setVarType(id, VarType::DOUBLE);
                                    }
                                }
                                paramIndex++;
                            } else {
                                int slot = newSlotFor(id); 
                                emit3("mov", slotVar(slot), "param" + std::to_string(paramIndex));
                                if (!param->type.empty()) {
                                    if (param->type == "string") {
                                        setVarType(id, VarType::STRING);
                                    }
                                    else if (param->type == "integer" || param->type == "boolean") {
                                        setVarType(id, VarType::INT);
                                    }
                                    else if (param->type == "real") {
                                        setVarType(id, VarType::DOUBLE);
                                    }
                                }
                                paramIndex++;
                            }
                        }
                    }
                }

                if (fn->block) fn->block->accept(*this);

                if (!functionSetReturn) {
                    if (getVarType(fn->name) == VarType::STRING) {
                        emit3("mov", "arg0", emptyString());  
                    } else {
                        emit3("mov", "rax", "0");  
                    }
                }

                emit("ret");
                currentFunctionName.clear();
                functionSetReturn = false;
            }
           
        }

        void writeTo(std::ostream& out) const {
            out << "program " << name << " {\n";
            out << "    section data {\n";
            
            for (const auto& reg : registers) {
                out << "        int " << reg << " = 0\n";
            }
            
            for (const auto& preg : ptrRegisters) {
                out << "        ptr " << preg << " = null\n";
            }
            
            for (int i = 0; i < 8; ++i) {
                out << "        int param" << i << " = 0\n";
            }
            
            for (int i = 0; i < nextSlot; ++i) {
                if (!isRegisterSlot(i) && !isTempVar(slotVar(i)) && !isPtrReg(slotVar(i))) {
                    auto it = slotToType.find(i);
                    if (it != slotToType.end()) {
                        if (it->second == VarType::STRING) {
                            out << "        string " << slotVar(i) << " = \"\"\n";
                        } else if (it->second == VarType::PTR) {
                            out << "        ptr " << slotVar(i) << " = null\n";
                        } else if (it->second == VarType::CHAR) {
                            out << "        int " << slotVar(i) << " = 0\n"; 
                        } else {
                            out << "        int " << slotVar(i) << " = 0\n";
                        }
                    } else {
                        out << "        int " << slotVar(i) << " = 0\n";
                    }
                }
            }
            
            if (needsEmptyString) {
                out << "        string empty_str = \"\"\n";
            }
            
            for (auto &s : stringLiterals) {
                if (s.first != "empty_str") { 
                    out << "        string " << s.first << " = " << escapeStringForMxvm(s.second) << "\n";
                }
            }
            
            if (usedStrings.count("fmt_int")) {
                out << "        string fmt_int = \"%lld \"\n";
            }
            if (usedStrings.count("fmt_str")) {
                out << "        string fmt_str = \"%s \"\n";
            }
            if (usedStrings.count("fmt_chr")) {
                out << "        string fmt_chr = \"%c \"\n";
            }
            if (usedStrings.count("newline")) {
                out << "        string newline = \"\\n\"\n";
            }
            out << "        string input_buffer, 256\n";
            out << "    }\n";
            out << "    section code {\n";
            out << "    start:\n";
            
            for (auto &s : prolog) out << "        " << s << "\n";
            for (auto &s : instructions) {
                if (endsWithColon(s)) out << "    " << s << "\n";
                else out << "        " << s << "\n";
            }
            
            out << "    }\n";
            out << "}\n";
        }

        void visit(ProgramNode& node) override {
            name = node.name;
            if (node.block) node.block->accept(*this);
        }

        void visit(BlockNode& node) override {
            for (auto &d : node.declarations) if (d) d->accept(*this);
            if (node.compoundStatement) node.compoundStatement->accept(*this);
        }

        void visit(VarDeclNode& node) override {
            for (size_t i = 0; i < node.identifiers.size(); i++) {
                std::string id = node.identifiers[i];
                int slot = newSlotFor(id);
                
                
                if (!node.type.empty()) {
                    if (node.type == "string") {
                        setVarType(id, VarType::PTR);
                        setSlotType(slot, VarType::PTR);
                        
                
                        if (i < node.initializers.size() && node.initializers[i]) {
                            std::string init = eval(node.initializers[i].get());
                            emit3("mov", slotVar(slot), init);
                            if (isReg(init)) freeReg(init);
                        } else {
                            emit3("mov", slotVar(slot), emptyString());
                        }
                    }
                    else if (node.type == "char") {
                        setVarType(id, VarType::CHAR);
                        setSlotType(slot, VarType::CHAR);
                        
                
                        if (i < node.initializers.size() && node.initializers[i]) {
                            std::string init = eval(node.initializers[i].get());
                            emit3("mov", slotVar(slot), init);
                            if (isReg(init)) freeReg(init);
                        } else {
                            emit3("mov", slotVar(slot), "0");  
                        }
                    }
                    else if (node.type == "integer" || node.type == "boolean") {
                        setVarType(id, VarType::INT);
                        setSlotType(slot, VarType::INT);
                        
                        if (i < node.initializers.size() && node.initializers[i]) {
                            std::string init = eval(node.initializers[i].get());
                            emit3("mov", slotVar(slot), init);
                            if (isReg(init)) freeReg(init);
                        }
                    }
                    else if (node.type == "real") {
                        setVarType(id, VarType::DOUBLE);
                        setSlotType(slot, VarType::DOUBLE);
                        
                        if (i < node.initializers.size() && node.initializers[i]) {
                            std::string init = eval(node.initializers[i].get());
                            emit3("mov", slotVar(slot), init);
                            if (isReg(init)) freeReg(init);
                        }
                    }
                    else if (node.type == "record") {
                        setVarType(id, VarType::RECORD);
                        setSlotType(slot, VarType::RECORD);
                        
                    }
                }
            }
        }

        void visit(ProcDeclNode& node) override {
            deferredProcs.push_back(&node);
            
            FuncInfo info;
            for (auto &p : node.parameters) {
                if (auto param = dynamic_cast<ParameterNode*>(p.get())) {
                    VarType t = VarType::INT; 
                    if (!param->type.empty()) {
                        if (param->type == "string") t = VarType::STRING;
                        else if (param->type == "char") t = VarType::CHAR;
                        else if (param->type == "real") t = VarType::DOUBLE;
                    }
                    for (size_t i = 0; i < param->identifiers.size(); ++i) {
                        info.paramTypes.push_back(t);
                    }
                }
            }
            funcSignatures[node.name] = info;
        }

        void visit(FuncDeclNode& node) override {
            deferredFuncs.push_back(&node);

            FuncInfo info;
            if (!node.returnType.empty()) {
                if (node.returnType == "string") info.returnType = VarType::STRING;
                else if (node.returnType == "char") info.returnType = VarType::CHAR;
                else if (node.returnType == "real") info.returnType = VarType::DOUBLE;
                else info.returnType = VarType::INT;
            }
            
            for (auto &p : node.parameters) {
                if (auto param = dynamic_cast<ParameterNode*>(p.get())) {
                    VarType t = VarType::INT; 
                    if (!param->type.empty()) {
                        if (param->type == "string") t = VarType::STRING;
                        else if (param->type == "char") t = VarType::CHAR;
                        else if (param->type == "real") t = VarType::DOUBLE;
                    }
                    for (size_t i = 0; i < param->identifiers.size(); ++i) {
                        info.paramTypes.push_back(t);
                    }
                }
            }
            funcSignatures[node.name] = info;
        }

        void visit(ParameterNode& node) override {
            for (auto &id : node.identifiers) {
                int slot = newSlotFor(id);
                
                if (!node.type.empty()) {
                    if (node.type == "string") {
                        setVarType(id, VarType::STRING);
                        setSlotType(slot, VarType::STRING);
                    }
                    else if (node.type == "integer" || node.type == "boolean") {
                        setVarType(id, VarType::INT);
                        setSlotType(slot, VarType::INT);
                    }
                    else if (node.type == "real") {
                        setVarType(id, VarType::DOUBLE);
                        setSlotType(slot, VarType::DOUBLE);
                    }
                }
            }
        }

        void visit(CompoundStmtNode& node) override {
            for (auto &stmt : node.statements) if (stmt) stmt->accept(*this);
        }

        void visit(AssignmentNode& node) override {
            std::string rhs = eval(node.expression.get());
            auto varPtr = dynamic_cast<VariableNode*>(node.variable.get());
            if (!varPtr) return;
            
            std::string varName = varPtr->name;
            
            if (!currentFunctionName.empty() && varName == currentFunctionName) {
                // Function return value handling is fine
                if (getVarType(currentFunctionName) == VarType::STRING) {
                    emit3("mov", "arg0", rhs);  
                } else {
                    emit3("mov", "rax", rhs);   
                }
                functionSetReturn = true;
            } else {
                int slot = newSlotFor(varName);
                std::string memLoc = slotVar(slot);
                
                auto it = valueLocations.find(varName);
                if (it != valueLocations.end() && it->second.type == ValueLocation::REGISTER) {
                    freeReg(it->second.location);
                }
                
                if (!isReg(rhs) && rhs != memLoc) {
                    std::string tempReg = allocReg();
                    emit3("mov", tempReg, rhs);
                    emit3("mov", memLoc, tempReg);
                    freeReg(tempReg);
                } else if (rhs != memLoc) {
                    emit3("mov", memLoc, rhs);
                }
                
                // Update the tracking for this variable
                recordLocation(varName, {ValueLocation::MEMORY, memLoc});
            }
            
            if (isReg(rhs)) freeReg(rhs);
        }

        void visit(IfStmtNode& node) override {
            std::string elseL = newLabel("ELSE");
            std::string endL  = newLabel("ENDIF");
            std::string c = eval(node.condition.get());
            emit2("cmp", c, "0");
            emit1("je", elseL);
            if (isReg(c)) freeReg(c);
            
            if (node.thenStatement) node.thenStatement->accept(*this);
            emit1("jmp", endL);
            emitLabel(elseL);
            if (node.elseStatement) node.elseStatement->accept(*this);
            emitLabel(endL);
        }

        void visit(WhileStmtNode& node) override {
            std::string start = newLabel("WHILE");
            std::string end   = newLabel("ENDWHILE");
            emitLabel(start);
            std::string c = eval(node.condition.get());
            emit2("cmp", c, "0");
            emit1("je", end);
            if (isReg(c)) freeReg(c);
            
            if (node.statement) node.statement->accept(*this);
            emit1("jmp", start);
            emitLabel(end);
        }

        void visit(ForStmtNode& node) override {
            int slot = newSlotFor(node.variable);
            std::string startV = eval(node.startValue.get());
            emit3("mov", slotVar(slot), startV);
            if (isReg(startV)) freeReg(startV);

            std::string loop = newLabel("FOR");
            std::string end  = newLabel("ENDFOR");
            emitLabel(loop);

            std::string endV = eval(node.endValue.get());
            std::string vr   = slotVar(slot);

            if (node.isDownto) {
                emit2("cmp", vr, endV);
                emit1("jl", end);
            } else {
                emit2("cmp", vr, endV);
                emit1("jg", end);
            }
            if (isReg(endV)) freeReg(endV);

            if (node.statement) node.statement->accept(*this);

            if (node.isDownto) {
                emit3("sub", vr, "1");
            } else {
                emit3("add", vr, "1");
            }

            emit1("jmp", loop);
            emitLabel(end);
        }

        void visit(ProcCallNode& node) override {
            std::string name = node.name;
            if (name == "writeln" || name == "write") {
                for(auto &arg : node.arguments) {
                    ASTNode *a = arg.get();
                    if (auto strNode = dynamic_cast<StringNode*>(a)) {
                        if (strNode->value.length() == 1) {
                            usedStrings.insert("fmt_chr");  
                            std::string v = eval(a);
                            emit2("print", "fmt_chr", v);   
                            if (isReg(v)) freeReg(v);
                        } else {
                            usedStrings.insert("fmt_str");
                            std::string v = eval(a);
                            emit2("print", "fmt_str", v);
                            if (isReg(v)) freeReg(v);
                        }
                    } 
                    else if (auto varNode = dynamic_cast<VariableNode*>(a)) {
                        if (getVarType(varNode->name) == VarType::CHAR) {
                            usedStrings.insert("fmt_chr");
                            std::string v = eval(a);
                            emit2("print", "fmt_chr", v);
                            if (isReg(v)) freeReg(v);
                        } else if (isStringVar(varNode->name)) {
                            usedStrings.insert("fmt_str");
                            std::string v = eval(a);
                            emit2("print", "fmt_str", v);
                            if (isReg(v)) freeReg(v);
                        } else {
                            usedStrings.insert("fmt_int");
                            std::string v = eval(a);
                            emit2("print", "fmt_int", v);
                            if (isReg(v)) freeReg(v);
                        }
                    } 
                    else {
                        usedStrings.insert("fmt_int");
                        std::string v = eval(a);
                        emit2("print", "fmt_int", v);
                        if (isReg(v)) freeReg(v);
                    }
                }

                if (name == "writeln") {
                    usedStrings.insert("newline");
                    emit1("print", "newline");
                }
                return;
            }
            
            if (name == "readln") {
                for (auto &arg : node.arguments) {
                    if (auto varNode = dynamic_cast<VariableNode*>(arg.get())) {
                        std::string varName = varNode->name;
                        int slot = newSlotFor(varName);
                        std::string memLoc = slotVar(slot);
                        
                        VarType varType = getVarType(varName);
                        emit1("getline", "input_buffer");                
                        if (varType == VarType::STRING || varType == VarType::PTR) {
                            //emit2("mov", memLoc, tempStr);
                        } else if (varType == VarType::INT || varType == VarType::UNKNOWN) {
                            emit2("to_int", memLoc, "input_buffer");
                        } else if (varType == VarType::DOUBLE) {
                            emit2("to_float", memLoc,"input_buffer");
                        } 
                        recordLocation(varName, {ValueLocation::MEMORY, memLoc});
                    } else {
                        
                        throw std::runtime_error("readln argument must be a variable");
                    }
                }
                return;
            }
            
            for (size_t i = 1; i <= 6; i++) {
                regInUse[i] = false;
            }
            
            std::vector<std::string> argValues;
            for (auto &arg : node.arguments) {
                argValues.push_back(eval(arg.get()));
            }
            
            for (size_t i = 0; i < argValues.size(); i++) {
                std::string paramName;
                VarType paramType = VarType::UNKNOWN;

                if (i < node.arguments.size()) {
                    if (auto varNode = dynamic_cast<VariableNode*>(node.arguments[i].get())) {
                        paramName = varNode->name;
                        paramType = getVarType(paramName);
                    } else if (dynamic_cast<StringNode*>(node.arguments[i].get())) {
                        paramType = VarType::STRING;
                    }
                }

                auto sig_it = funcSignatures.find(name);
                if (sig_it != funcSignatures.end() && i < sig_it->second.paramTypes.size()) {
                    if (sig_it->second.paramTypes[i] == VarType::STRING) {
                        paramType = VarType::STRING;
                    }
                }

                if (i < 6) {
                    if (paramType == VarType::STRING) {
                        if (i < ptrRegisters.size()) {
                            if (ptrRegisters[i] != argValues[i]) {
                                emit3("mov", ptrRegisters[i], argValues[i]);
                            }
                        } else {
                            const std::string dest = registers[i+1];
                            if (dest != argValues[i]) {
                                emit3("mov", dest, argValues[i]);
                            }
                            regInUse[i+1] = true;
                        }
                    } else {
                        const std::string dest = registers[i+1];
                        if (dest != argValues[i]) {
                            emit3("mov", dest, argValues[i]);
                        }
                        regInUse[i+1] = true;
                    }
                } else {
                    std::string paramVar = "param" + std::to_string(i);
                    int paramSlot = newSlotFor(paramVar);
                    emit3("mov", slotVar(paramSlot), argValues[i]);
                }
                if (isReg(argValues[i]) && !isParmReg(argValues[i])) {
                    freeReg(argValues[i]);
                }
            }
            
            emit1("call", funcLabel("PROC_", name));
        }

        void visit(FuncCallNode& node) override {
            for (size_t i = 1; i <= 6; i++) {
                regInUse[i] = false;
            }
            
            std::vector<std::string> argValues;
            for (auto &arg : node.arguments) {
                argValues.push_back(eval(arg.get()));
            }
            
            for (size_t i = 0; i < argValues.size(); i++) {
                std::string paramName;
                VarType paramType = VarType::UNKNOWN;

                if (i < node.arguments.size()) {
                    if (auto varNode = dynamic_cast<VariableNode*>(node.arguments[i].get())) {
                        paramName = varNode->name;
                        paramType = getVarType(paramName);
                    } else if (dynamic_cast<StringNode*>(node.arguments[i].get())) {
                        paramType = VarType::STRING;
                    }
                }

                if (i < 6) {
                    const std::string dest = registers[i+1];

                    if (paramType == VarType::STRING) {
                        emit3("mov", dest, argValues[i]); 
                    } else {
                        emit3("mov", dest, argValues[i]);
                    }
                    regInUse[i+1] = true;
                } else {
                    std::string paramVar = "param" + std::to_string(i);
                    int paramSlot = newSlotFor(paramVar);
                    emit3("mov", slotVar(paramSlot), argValues[i]);
                }
                if (isReg(argValues[i]) && !isParmReg(argValues[i])) {
                    freeReg(argValues[i]);
                }
            }
            
            emit1("call", funcLabel("FUNC_", node.name));
            
            VarType returnType = VarType::INT;
            auto sig_it = funcSignatures.find(node.name);
            if (sig_it != funcSignatures.end()) {
                returnType = sig_it->second.returnType;
            }
            
            if (returnType == VarType::STRING) {
                pushValue("arg0");  
            } else {
                pushValue("rax");   
            }
        }

        void visit(BinaryOpNode& node) override {
            if (!node.left || !node.right) {
                throw std::runtime_error("Binary operation missing operand");
            }

            std::string a = eval(node.left.get());
            std::string b = eval(node.right.get());

            switch (node.operator_) {
                case BinaryOpNode::PLUS:      pushTri("add", a, b); break;
                case BinaryOpNode::MINUS:     pushTri("sub", a, b); break;
                case BinaryOpNode::MULTIPLY:  pushTri("mul", a, b); break;
                case BinaryOpNode::DIVIDE:    pushTri("div", a, b); break;
                case BinaryOpNode::DIV:       pushTri("idiv", a, b); break;
                case BinaryOpNode::MOD:       pushTri("mod", a, b); break;

                case BinaryOpNode::EQUAL:         pushCmpResult(a, b, "je");  break;
                case BinaryOpNode::NOT_EQUAL:     pushCmpResult(a, b, "jne"); break;
                case BinaryOpNode::LESS:          pushCmpResult(a, b, "jl");  break;
                case BinaryOpNode::LESS_EQUAL:    pushCmpResult(a, b, "jle"); break;
                case BinaryOpNode::GREATER:       pushCmpResult(a, b, "jg");  break;
                case BinaryOpNode::GREATER_EQUAL: pushCmpResult(a, b, "jge"); break;

                case BinaryOpNode::AND: pushLogicalAnd(a, b); break;
                case BinaryOpNode::OR:  pushLogicalOr(a, b);  break;

                default: {
                    std::string t = allocReg();
                    pushValue(t); 
                    if (isReg(a)) freeReg(a);
                    if (isReg(b)) freeReg(b);
                }
            }
        }

        void visit(UnaryOpNode& node) override {
            std::string v = eval(node.operand.get());
            switch (node.operator_) {
                case UnaryOpNode::MINUS: {
                    std::string t = allocReg();
                    emit3("mov", t, "0");
                    emit3("sub", t, v);
                    pushValue(t);
                    break;
                }
                case UnaryOpNode::PLUS: {
                    pushValue(v);
                    break;
                }
                case UnaryOpNode::NOT: {
                    std::string t = allocReg();
                    std::string L1 = newLabel("NOT_TRUE");
                    std::string L2 = newLabel("NOT_END");
                    emit2("cmp", v, "0");
                    emit1("je", L1);
                    emit3("mov", t, "0");
                    emit1("jmp", L2);
                    emitLabel(L1);
                    emit3("mov", t, "1");
                    emitLabel(L2);
                    pushValue(t);
                    if (isReg(v)) freeReg(v);
                    break;
                }
            }
        }

        void visit(VariableNode& node) override {
            auto it = valueLocations.find(node.name);
            if (it != valueLocations.end()) {
                if (it->second.type == ValueLocation::REGISTER) {
                    pushValue(it->second.location);
                    return;
                }
            }
            
            int slot = newSlotFor(node.name);
            pushValue(slotVar(slot));
        }

        void visit(NumberNode& node) override {
            if (node.value.length() < 10) {
                pushValue(node.value); 
                return;
            }
            
            std::string reg = allocReg();
            emit3("mov", reg, node.value);
            pushValue(reg);
        }

        void visit(StringNode& node) override {
            if (node.value.length() == 1) {
                int asciiValue = static_cast<int>(node.value[0]);
                pushValue(std::to_string(asciiValue));
            } else {
                std::string sym = internString(node.value);
                pushValue(sym);
            }
        }

        void visit(BooleanNode& node) override {
            std::string reg = allocReg();
            emit3("mov", reg, node.value ? "1" : "0");
            pushValue(reg);
        }

        void visit(EmptyStmtNode& node) override {
        }

        void visit(RepeatStmtNode& node) override {
            std::string startLabel = newLabel("REPEAT");
            std::string endLabel = newLabel("UNTIL");
            
            emitLabel(startLabel);
            
            for (auto& stmt : node.statements) {
                if (stmt) stmt->accept(*this);
            }
            
            std::string condResult = eval(node.condition.get());
            emit2("cmp", condResult, "0");
            emit1("je", startLabel);
            
            if (isReg(condResult)) freeReg(condResult);
        }

    private:
        
        enum class VarType {
            INT,
            DOUBLE,
            STRING,
            CHAR,
            RECORD,
            PTR,     
            UNKNOWN
        };

        struct FuncInfo {
            std::vector<VarType> paramTypes;
            VarType returnType = VarType::UNKNOWN;
        };
        std::unordered_map<std::string, FuncInfo> funcSignatures;

        std::unordered_map<std::string, VarType> varTypes;
        std::unordered_map<int, VarType> slotToType;
        bool needsEmptyString = false;
        
        VarType getVarType(const std::string& name) const {
            auto it = varTypes.find(name);
            return it != varTypes.end() ? it->second : VarType::UNKNOWN;
        }
        
        void setVarType(const std::string& name, VarType type) {
            varTypes[name] = type;
        }
        
        void setSlotType(int slot, VarType type) {
            slotToType[slot] = type;
        }
        
        bool isStringVar(const std::string& name) const {
            VarType type = getVarType(name);
            return type == VarType::STRING || type == VarType::PTR;
        }
        
        std::string emptyString() {
            needsEmptyString = true;
            return "empty_str";
        }
        
        const std::vector<std::string> registers = {"rax", "rbx", "rcx", "rdx", "rsi", "rdi", "r8", "r9", "r10", "r11"};
        const std::vector<std::string> ptrRegisters = {"arg0", "arg1", "arg2", "arg3", "arg4", "arg5", "arg6"};
        std::vector<bool> regInUse;
        
        std::vector<ProcDeclNode*> deferredProcs;
        std::vector<FuncDeclNode*> deferredFuncs;
        std::string currentFunctionName;
        bool functionSetReturn;
        
        int nextSlot;
        int nextTemp;
        int labelCounter;

        std::unordered_map<std::string,int> varSlot;
        std::vector<std::string> prolog;
        std::vector<std::string> instructions;

        std::vector<std::string> evalStack;

        std::unordered_map<std::string,std::string> stringLiterals; 
        std::unordered_set<std::string> usedStrings;

        std::unordered_map<int, std::string> slotToName;

         struct ValueLocation {
             enum Type { REGISTER, MEMORY, IMMEDIATE } type;
             std::string location; 
         };
         std::unordered_map<std::string, ValueLocation> valueLocations;

        static bool endsWithColon(const std::string& s) {
            return !s.empty() && s.back() == ':';
        }
        
        bool isRegisterSlot(int slot) const {
            return slot < -1; 
        }
        
        bool isTempVar(const std::string& name) const {
            return name.find("__t") != std::string::npos;
        }
        
        std::string slotVar(int slot) const {
            auto it = slotToName.find(slot);
            if (it != slotToName.end()) return it->second;

            if (slot < -1) {
                int regIndex = -2 - slot;
                if (regIndex >= 0 && regIndex < static_cast<int>(registers.size())) {
                    return registers[regIndex];
                }
            }
            return "v" + std::to_string(slot);
        }
        
        std::string allocReg() {
            for (size_t i = 1; i < regInUse.size(); i++) { 
                if (!regInUse[i]) {
                    regInUse[i] = true;
                    return registers[i];
                }
            }
            return newTemp();  
        }
        void freeReg(const std::string& reg) {
            for (size_t i = 0; i < registers.size(); i++) {
                if (registers[i] == reg) {
                    regInUse[i] = false;
                    return;
                }
            }
        }

        bool isPtrReg(const std::string& name) const {
            return std::find(ptrRegisters.begin(), ptrRegisters.end(), name) != ptrRegisters.end();
        }
        
        bool isReg(const std::string& name) const {
            return std::find(registers.begin(), registers.end(), name) != registers.end();
        }

        int newSlotFor(const std::string& name) {
            auto it = varSlot.find(name);
            if (it != varSlot.end()) return it->second;
            int slot = nextSlot++;
            varSlot[name] = slot;
            if (!name.empty() && name.find("__t") == std::string::npos) {
                slotToName[slot] = name;
            }
            return slot;
        }

        std::string newTemp() {
            return slotVar(newSlotFor("__t" + std::to_string(nextTemp++)));
        }

        std::string newLabel(const std::string& prefix) {
            std::ostringstream ss;
            ss << prefix << "_" << (labelCounter++);
            return ss.str();
        }

        std::string funcLabel(const char* prefix, const std::string& name) {
            return std::string(prefix) + name;
        }

        void emit(const std::string& line) {
            instructions.push_back(line);
        }
        void emitLabel(const std::string& label) {
            instructions.push_back(label + ":");
        }
        void emit1(const std::string& op, const std::string& a) {
            emit(op + " " + a);
        }
        void emit2(const std::string& op, const std::string& a, const std::string& b) {
            emit(op + " " + a + ", " + b);
        }
        void emit3(const std::string& op, const std::string& a, const std::string& b) {
            emit(op + " " + a + ", " + b);
        }
        void emit3(const std::string& op, const std::string& dst, const std::string& a, const std::string& b) {
            emit(op + " " + dst + ", " + a + ", " + b);
        }

        std::string escapeStringForMxvm(const std::string& raw) const {
            std::ostringstream o;
            o << "\"";
            for (char c : raw) {
                if (c == '\\') o << "\\\\";
                else if (c == '\"') o << "\\\"";
                else if (c == '\n') o << "\\n";
                else if (c == '\t') o << "\\t";
                else o << c;
            }
            o << "\"";
            return o.str();
        }

        std::string internString(const std::string& val) {
            std::string key = "str_" + std::to_string((int)stringLiterals.size());
            stringLiterals.emplace(key, val);
            return key;
        }

        std::string convertCharLiteral(const std::string& charLit) {
            if (charLit.length() == 3 && charLit[0] == '\'' && charLit[2] == '\'') {
                int asciiValue = static_cast<int>(charLit[1]);
                return std::to_string(asciiValue);
            }
            
            
            if (charLit.length() == 4 && charLit[0] == '\'' && charLit[1] == '\\' && charLit[3] == '\'') {
                char escapeChar = charLit[2];
                int asciiValue = 0;
                switch (escapeChar) {
                    case 'n': asciiValue = 10; break;  
                    case 't': asciiValue = 9; break;   
                    case 'r': asciiValue = 13; break;  
                    case '\\': asciiValue = 92; break; 
                    case '\'': asciiValue = 39; break; 
                    case '0': asciiValue = 0; break;   
                    default: asciiValue = static_cast<int>(escapeChar); break;
                }
                return std::to_string(asciiValue);
            }
            
            
            return charLit;
        }
        
        void pushValue(const std::string& v) { 
            evalStack.push_back(v);
            
            if (isReg(v)) {
                recordLocation(v, {ValueLocation::REGISTER, v});
            }
            else if (std::all_of(v.begin(), v.end(), [](char c) { 
                return std::isdigit(c) || c == '-'; })) {
            }
        }
        std::string popValue() {
            if (evalStack.empty()) {
                throw std::runtime_error("Evaluation stack underflow");
            }
            std::string v = evalStack.back();
            evalStack.pop_back();
            return v;
        }

        std::string eval(ASTNode* n) {
            if (!n) return "0";
            
            if (auto num = dynamic_cast<NumberNode*>(n)) {
                if (num->value.length() < 10) {
                    return num->value;
                }
            }
            
            n->accept(*this);
            if (evalStack.empty()) {
                throw std::runtime_error("Expression produced no value");
            }
            return popValue();
        }

        void recordLocation(const std::string& var, ValueLocation loc) {
            valueLocations[var] = loc;
        }

        void pushTri(const char* op, const std::string& a, const std::string& b) {
            std::string result = allocReg();
            if (std::string(op) =="mul") {
                emit3("mov", "rax", a);
                emit1("mul", b);  
                emit3("mov", result, "rax");
            } else {
                emit3("mov", result, a);
                emit3(op, result, b);
            }
            pushValue(result);
            if (isReg(a)) freeReg(a);
            if (isReg(b)) freeReg(b);
        }

        void pushCmpResult(const std::string& a, const std::string& b, const char* jop) {
            std::string t = allocReg();
            std::string L1 = newLabel("CMP_TRUE");
            std::string L2 = newLabel("CMP_END");
            emit2("cmp", a, b);
            emit1(jop, L1);
            emit3("mov", t, "0");
            emit1("jmp", L2);
            emitLabel(L1);
            emit3("mov", t, "1");
            emitLabel(L2);
            pushValue(t);
            
            if (isReg(a)) freeReg(a);
            if (isReg(b)) freeReg(b);
        }

        void pushLogicalAnd(const std::string& a, const std::string& b) {
            std::string t = allocReg();
            std::string L0 = newLabel("AND_ZERO");
            std::string L1 = newLabel("AND_END");
            emit2("cmp", a, "0");
            emit1("je", L0);
            emit2("cmp", b, "0");
            emit1("je", L0);
            emit3("mov", t, "1");
            emit1("jmp", L1);
            emitLabel(L0);
            emit3("mov", t, "0");
            emitLabel(L1);
            pushValue(t);
            
            if (isReg(a)) freeReg(a);
            if (isReg(b)) freeReg(b);
        }

        void pushLogicalOr(const std::string& a, const std::string& b) {
            std::string t = allocReg();
            std::string L1 = newLabel("OR_ONE");
            std::string L2 = newLabel("OR_END");
            emit2("cmp", a, "0");
            emit1("jne", L1);
            emit2("cmp", b, "0");
            emit1("jne", L1);
            emit3("mov", t, "0");
            emit1("jmp", L2);
            emitLabel(L1);
            emit3("mov", t, "1");
            emitLabel(L2);
            pushValue(t);
            
            if (isReg(a)) freeReg(a);
            if (isReg(b)) freeReg(b);
        }

        bool isParmReg(const std::string& name) const {
            for (size_t i = 1; i <= 6; i++) {
                if (registers[i] == name) return true;
            }
            return false;
        }
    };

    std::string mxvmOpt(const std::string &text);

} 
#endif