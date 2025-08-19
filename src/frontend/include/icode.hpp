
#ifndef __CODEGEN_H_
#define __CODEGEN_H_
#include "ast.hpp"
#include <sstream>
#include <unordered_set>
#include <unordered_map>
#include <vector>
#include <string>
#include <utility>

namespace pascal {
    class CodeGenVisitor : public ASTVisitor {
    public:
        CodeGenVisitor()
            : nextSlot(0), nextTemp(0), labelCounter(0) {}

        virtual ~CodeGenVisitor() = default;
        std::string name = "App";

        void generate(ASTNode* root) {
            if (!root) return;
            instructions.clear();
            usedStrings.clear();
            if (auto prog = dynamic_cast<ProgramNode*>(root)) {
                name = prog->name;
            }
            root->accept(*this);
            emit("done");
        }

        void writeTo(std::ostream& out) const {
            out << "program " << name << " {\n";
            out << "    section data {\n";
            for (int i = 0; i < nextSlot; ++i) {
                out << "        int " << slotVar(i) << " = 0\n";
            }
            for (auto &s : stringLiterals) {
                out << "        string " << s.first << " = " << escapeStringForMxvm(s.second) << "\n";
            }
            if (usedStrings.count("fmt_int")) {
                out << "        string fmt_int = \"%lld \"\n";
            }
            if (usedStrings.count("fmt_str")) {
+               out << "        string fmt_str = \"%s \"\n";
+           }
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
            emitLabel(funcLabel("PROC_", node.name));
            for (auto &p : node.parameters) if (p) p->accept(*this);
            if (node.block) node.block->accept(*this);
            emit("ret");
        }

        void visit(FuncDeclNode& node) override {
            emitLabel(funcLabel("FUNC_", node.name));
            for (auto &p : node.parameters) if (p) p->accept(*this);
            if (node.block) node.block->accept(*this);
            emit("ret");
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
            int slot = newSlotFor(varPtr->name);
            emit3("mov", slotVar(slot), rhs);
        }

        void visit(IfStmtNode& node) override {
            std::string elseL = newLabel("ELSE");
            std::string endL  = newLabel("ENDIF");
            std::string c = eval(node.condition.get());
            emit2("cmp", c, "0");
            emit1("je", elseL);
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
            if (node.statement) node.statement->accept(*this);
            emit1("jmp", start);
            emitLabel(end);
        }

        void visit(ForStmtNode& node) override {
            int slot = newSlotFor(node.variable);
            std::string startV = eval(node.startValue.get());
            emit3("mov", slotVar(slot), startV);

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
                    } else {
                        usedStrings.insert("fmt_int");
                        std::string v = eval(a);
                        emit2("print", "fmt_int", v);
                    }
                }

                if (name == "writeln") {
                    usedStrings.insert("newline");
                    emit1("print", "newline");
                }
                return;
            }
            
            for (auto &arg : node.arguments) (void)eval(arg.get());
            emit1("call", funcLabel("PROC_", name));
        }

        void visit(FuncCallNode& node) override {
            for (auto &arg : node.arguments) (void)eval(arg.get());
            emit1("call", funcLabel("FUNC_", node.name));
        }

        void visit(BinaryOpNode& node) override {
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
                    std::string t = newTemp();
                    pushValue(t); // undefined; avoid crash
                }
            }
        }

        void visit(UnaryOpNode& node) override {
            std::string v = eval(node.operand.get());
            switch (node.operator_) {
                case UnaryOpNode::MINUS: {
                    std::string t = newTemp();
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
                    std::string t = newTemp();
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
                    break;
                }
            }
        }

        void visit(VariableNode& node) override {
            int slot = newSlotFor(node.name);
            pushValue(slotVar(slot));
        }

        void visit(NumberNode& node) override {
            std::string t = newTemp();
            emit3("mov", t, node.value);
            pushValue(t);
        }

        void visit(StringNode& node) override {
            std::string sym = internString(node.value);
            pushValue(sym);
        }

        void visit(BooleanNode& node) override {
            std::string t = newTemp();
            emit3("mov", t, node.value ? "1" : "0");
            pushValue(t);
        }

        void visit(EmptyStmtNode& node) override {
            // no-op
        }

    private:
        /* ----- state ----- */
        int nextSlot;
        int nextTemp;
        int labelCounter;

        std::unordered_map<std::string,int> varSlot;
        std::vector<std::string> prolog;
        std::vector<std::string> instructions;

        std::vector<std::string> evalStack;

        std::unordered_map<std::string,std::string> stringLiterals; // sym -> literal
        std::unordered_set<std::string> usedStrings;

        /* ----- helpers ----- */
        static bool endsWithColon(const std::string& s) {
            return !s.empty() && s.back() == ':';
        }

        std::string slotVar(int slot) const {
            return "v" + std::to_string(slot);
        }

        int newSlotFor(const std::string& name) {
            auto it = varSlot.find(name);
            if (it != varSlot.end()) return it->second;
            int slot = nextSlot++;
            varSlot[name] = slot;
            return slot;
        }

        std::string newTemp() {
            // temps are just more ints in data
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

        /* eval stack helpers */
        void pushValue(const std::string& v) { evalStack.push_back(v); }
        std::string popValue() {
            std::string v = evalStack.back();
            evalStack.pop_back();
            return v;
        }

        std::string eval(ASTNode* n) {
            if (!n) return "0";
            n->accept(*this);
            return popValue();
        }

        void pushTri(const char* op, const std::string& a, const std::string& b) {
            std::string t = newTemp();
            emit3(op, t, a, b);
            pushValue(t);
        }

        void pushCmpResult(const std::string& a, const std::string& b, const char* jop) {
            std::string t  = newTemp();
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
        }

        void pushLogicalAnd(const std::string& a, const std::string& b) {
            std::string t  = newTemp();
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
        }

        void pushLogicalOr(const std::string& a, const std::string& b) {
            std::string t  = newTemp();
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
        }
    };

} 
#endif