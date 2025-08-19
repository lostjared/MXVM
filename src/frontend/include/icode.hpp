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
                                varSlot[id] = -2 - (paramIndex + 1);
                                paramIndex++;
                            } else {
                                int slot = newSlotFor(id);
                                emit3("mov", slotVar(slot), "param" + std::to_string(paramIndex));
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
                
                int paramIndex = 0;
                for (auto &p : fn->parameters) {
                    if (auto param = dynamic_cast<ParameterNode*>(p.get())) {
                        for (auto &id : param->identifiers) {
                            if (paramIndex < 6) { 
                                varSlot[id] = -2 - (paramIndex + 1);
                                paramIndex++;
                            } else {
                                int slot = newSlotFor(id);
                                emit3("mov", slotVar(slot), "param" + std::to_string(paramIndex));
                                paramIndex++;
                            }
                        }
                    }
                }
                
                if (fn->block) fn->block->accept(*this);
                
                if (!functionSetReturn) {
                    emit3("mov", "rax", "0");
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
            
            for (int i = 0; i < 8; ++i) {
                out << "        int param" << i << " = 0\n";
            }
            
            for (int i = 0; i < nextSlot; ++i) {
                if (!isRegisterSlot(i) && !isTempVar(slotVar(i))) {
                    out << "        int " << slotVar(i) << " = 0\n";
                }
            }
            
            for (auto &s : stringLiterals) {
                out << "        string " << s.first << " = " << escapeStringForMxvm(s.second) << "\n";
            }
            
            if (usedStrings.count("fmt_int")) {
                out << "        string fmt_int = \"%lld \"\n";
            }
            if (usedStrings.count("fmt_str")) {
                out << "        string fmt_str = \"%s \"\n";
            }
            if (usedStrings.count("newline")) {
                out << "        string newline = \"\\n\"\n";
            }
            
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
            for (auto &id : node.identifiers) {
                newSlotFor(id);
            }
        }

        void visit(ProcDeclNode& node) override {
            deferredProcs.push_back(&node);
        }

        void visit(FuncDeclNode& node) override {
            deferredFuncs.push_back(&node);
        }

        void visit(ParameterNode& node) override {
            for (auto &id : node.identifiers) newSlotFor(id);
        }

        void visit(CompoundStmtNode& node) override {
            for (auto &stmt : node.statements) if (stmt) stmt->accept(*this);
        }

        void visit(AssignmentNode& node) override {
            std::string rhs = eval(node.expression.get());
            auto varPtr = dynamic_cast<VariableNode*>(node.variable.get());
            if (!varPtr) return;
            
            if (!currentFunctionName.empty() && varPtr->name == currentFunctionName) {
                emit3("mov", "rax", rhs);
                functionSetReturn = true;
            } else {
                int slot = newSlotFor(varPtr->name);
                emit3("mov", slotVar(slot), rhs);
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
                    if(dynamic_cast<StringNode*>(a)) {
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

                if (name == "writeln") {
                    usedStrings.insert("newline");
                    emit1("print", "newline");
                }
                return;
            }
            
            std::vector<std::string> argValues;
            for (auto &arg : node.arguments) {
                argValues.push_back(eval(arg.get()));
            }
            
            // Visit ProcCallNode changes: avoid self-moves
            for (size_t i = 0; i < argValues.size(); i++) {
                if (i < 6) {
                    // only emit move if source != destination (avoid mov rbx, rbx)
                    if (registers[i+1] != argValues[i])
                        emit3("mov", registers[i+1], argValues[i]);
                } else {
                    std::string paramVar = "param" + std::to_string(i);
                    int paramSlot = newSlotFor(paramVar);
                    emit3("mov", slotVar(paramSlot), argValues[i]);
                }
                if (isReg(argValues[i])) freeReg(argValues[i]);
            }
            
            emit1("call", funcLabel("PROC_", name));
        }

        void visit(FuncCallNode& node) override {
            std::vector<std::string> argValues;
            for (auto &arg : node.arguments) {
                argValues.push_back(eval(arg.get()));
            }
            
            // Visit FuncCallNode changes: avoid self-moves
            for (size_t i = 0; i < argValues.size(); i++) {
                if (i < 6) {
                    if (registers[i+1] != argValues[i])
                        emit3("mov", registers[i+1], argValues[i]);
                } else {
                    std::string paramVar = "param" + std::to_string(i);
                    int paramSlot = newSlotFor(paramVar);
                    emit3("mov", slotVar(paramSlot), argValues[i]);
                }
                if (isReg(argValues[i])) freeReg(argValues[i]);
            }
            
            emit1("call", funcLabel("FUNC_", node.name));
            
            pushValue("rax");
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
            int slot = newSlotFor(node.name);
            pushValue(slotVar(slot));
        }

        void visit(NumberNode& node) override {
            std::string reg = allocReg();
            emit3("mov", reg, node.value);
            pushValue(reg);
        }

        void visit(StringNode& node) override {
            std::string sym = internString(node.value);
            pushValue(sym);
        }

        void visit(BooleanNode& node) override {
            std::string reg = allocReg();
            emit3("mov", reg, node.value ? "1" : "0");
            pushValue(reg);
        }

        void visit(EmptyStmtNode& node) override {
        }

    private:
        
        const std::vector<std::string> registers = {"rax", "rbx", "rcx", "rdx", "rsi", "rdi", "r8", "r9", "r10", "r11"};
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
        
        bool isReg(const std::string& name) const {
            return std::find(registers.begin(), registers.end(), name) != registers.end();
        }

        int newSlotFor(const std::string& name) {
            auto it = varSlot.find(name);
            if (it != varSlot.end()) return it->second;
            int slot = nextSlot++;
            varSlot[name] = slot;
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

        
        void pushValue(const std::string& v) { evalStack.push_back(v); }
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
            n->accept(*this);
            if (evalStack.empty()) {
                throw std::runtime_error("Expression produced no value");
            }
            return popValue();
        }

        void pushTri(const char* op, const std::string& a, const std::string& b) {
            std::string result = allocReg();
            emit3("mov", result, a);
            emit3(op, result, b);
            pushValue(result);
            
            if (isReg(a) && a != result) freeReg(a);
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
    };

} 
#endif