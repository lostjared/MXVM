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
#include <memory>
#include <set>
#include <map>
#include <cctype>
#include <cstdlib>
#include <optional>
#include <stdexcept>
#define MXVM_BOUNDS_CHECK

namespace pascal {

    class CodeGenVisitor;

    enum class VarType { INT, DOUBLE, STRING, CHAR, RECORD, PTR, BOOL, UNKNOWN, ARRAY_INT, ARRAY_DOUBLE, ARRAY_STRING };
    
    class BuiltinFunctionHandler {
    public:
        virtual ~BuiltinFunctionHandler() = default;
        virtual bool canHandle(const std::string& funcName) const = 0;
        virtual void generate(CodeGenVisitor& visitor, const std::string& funcName, const std::vector<std::unique_ptr<ASTNode>>& arguments) = 0;
        virtual bool generateWithResult(CodeGenVisitor& visitor, const std::string& funcName, const std::vector<std::unique_ptr<ASTNode>>& arguments) { return false; }
        virtual VarType getReturnType(const std::string& funcName) const { return VarType::UNKNOWN; }
    };

    class BuiltinFunctionRegistry {
    private:
        std::vector<std::unique_ptr<BuiltinFunctionHandler>> handlers;
    public:
        void registerHandler(std::unique_ptr<BuiltinFunctionHandler> handler) { handlers.push_back(std::move(handler)); }
        BuiltinFunctionHandler* findHandler(const std::string& funcName) {
            for (auto& h : handlers) if (h->canHandle(funcName)) return h.get();
            return nullptr;
        }
    };

    class IOFunctionHandler : public BuiltinFunctionHandler {
    public:
        bool canHandle(const std::string& funcName) const override;
        void generate(CodeGenVisitor& visitor, const std::string& funcName, const std::vector<std::unique_ptr<ASTNode>>& arguments) override;
    };

    class StdFunctionHandler : public BuiltinFunctionHandler {
    public:
        bool canHandle(const std::string& funcName) const override;
        void generate(CodeGenVisitor& visitor, const std::string& funcName, const std::vector<std::unique_ptr<ASTNode>>& arguments) override;
        bool generateWithResult(CodeGenVisitor& visitor, const std::string& funcName, const std::vector<std::unique_ptr<ASTNode>>& arguments) override;
    };

    class StringFunctionHandler : public BuiltinFunctionHandler { 
        bool canHandle(const std::string& funcName) const override;
        void generate(CodeGenVisitor& visitor, const std::string& funcName, const std::vector<std::unique_ptr<ASTNode>>& arguments) override;
        bool generateWithResult(CodeGenVisitor& visitor, const std::string& funcName, const std::vector<std::unique_ptr<ASTNode>>& arguments) override;
        VarType getReturnType(const std::string& funcName) const override;
    };

    class SDLFunctionHandler : public BuiltinFunctionHandler {
    public:
        bool canHandle(const std::string& funcName) const override;
        void generate(CodeGenVisitor& visitor, const std::string& funcName, const std::vector<std::unique_ptr<ASTNode>>& arguments) override;
        bool generateWithResult(CodeGenVisitor& visitor, const std::string& funcName, const std::vector<std::unique_ptr<ASTNode>>& arguments) override;
    };


    class CodeGenVisitor : public ASTVisitor {
    private:
        std::unordered_map<std::string, std::string> compileTimeConstants;
        std::unordered_set<std::string> usedRealConstants;
        int realConstantCounter = 0;
        std::unordered_map<std::string, std::string> currentParamTypes; 
        std::set<std::string> usedModules;
        std::vector<std::string> globalArrays;
        std::unordered_map<std::string, std::vector<std::string>> functionScopedArrays;
        std::vector<std::string> scopeHierarchy;
        struct Scope { std::string name; std::vector<ProcDeclNode*> nestedProcs; std::vector<FuncDeclNode*> nestedFuncs; };
        std::vector<Scope> scopeStack;
        std::unordered_map<std::string, std::string> typeAliases;
        std::map<std::string, std::pair<std::string, std::string>> constInitialValues;
        std::map<std::string, std::string> currentParamLocations;
        std::vector<std::pair<ProcDeclNode*, std::vector<std::string>>> deferredProcs;
        std::vector<std::string> allTempPtrs;
        std::vector<bool> tempPtrInUse;

        std::string generateRealConstantName() { return "real_const_" + std::to_string(realConstantCounter++); }

        bool isRealNumber(const std::string& s) const {
            if (s.empty()) return false;
            bool hasDot = false, hasExp = false;
            for (char c : s) { if (c == '.') hasDot = true; if (c == 'e' || c == 'E') hasExp = true; }
            if (!hasDot && !hasExp) return false;
            char* end = nullptr;
            std::strtod(s.c_str(), &end);
            return end && *end == '\0';
        }

        std::string mangleWithScope(const std::string& baseName, const std::vector<std::string>& scopePath) const {
            if (scopePath.empty() || scopePath.size() <= 1) {
                return baseName;
            }
            std::string m;
            for (size_t i = 1; i < scopePath.size(); ++i) { 
                if (i > 1) m += "_";
                m += scopePath[i];
            }
            m += "_" + baseName;
            return m;
        }

        std::string mangleVariableName(const std::string& varName) const {
            return mangleWithScope(varName, scopeHierarchy);
        }

        std::string getCurrentScopeName() const {
            if (scopeHierarchy.empty() || scopeHierarchy.back() == "__global__") return "";
            
            std::string currentScope = scopeHierarchy.back();
            auto& funcs = generatingDeferredCode ? deferredFuncs : std::vector<std::pair<FuncDeclNode*, std::vector<std::string>>>();
            bool isFunc = false;
            for(const auto& df : deferredFuncs) {
                if(df.first->name == currentScope) { isFunc = true; break; }
            }

            std::vector<std::string> parentScope(scopeHierarchy.begin(), scopeHierarchy.end() - 1);
            return (isFunc ? "FUNC_" : "PROC_") + mangleWithScope(currentScope, parentScope);
        }

        struct FuncInfo { std::vector<VarType> paramTypes; VarType returnType = VarType::UNKNOWN; };

        std::unordered_map<std::string, FuncInfo> funcSignatures;
        std::unordered_map<std::string, VarType> varTypes;
        std::unordered_map<int, VarType> slotToType;

        bool needsEmptyString = false;
        std::string funcLabel(const char* prefix, const std::string& name) { return std::string(prefix) + name; }
        
        VarType getVarType(const std::string& name) const {
            std::string mangledName = findMangledName(name);
            auto slotIt = varSlot.find(mangledName);
            if (slotIt != varSlot.end()) {
                auto typeIt = slotToType.find(slotIt->second);
                if (typeIt != slotToType.end()) return typeIt->second;
            }
            auto it = varTypes.find(name);
            return it != varTypes.end() ? it->second : VarType::UNKNOWN;
        }

        bool isRealConstSymbol(const std::string& s) const { return s.rfind("real_const_", 0) == 0; }

        std::string coerceToIntImmediate(const std::string& v) {
            if (isFloatLiteral(v)) {
                long long x = static_cast<long long>(std::stod(v));
                return std::to_string(x);
            }
            if (isRealConstSymbol(v)) {
                auto it = realConstants.find(v);
                if (it != realConstants.end()) {
                    long long x = static_cast<long long>(std::stod(it->second));
                    return std::to_string(x);
                }
            }
            return v;
        }

        void setVarType(const std::string& name, VarType t) { varTypes[name] = t; }
        void setSlotType(int slot, VarType t) { slotToType[slot] = t; }
        bool isStringVar(const std::string& name) const {
            auto t = getVarType(name);
            return t == VarType::STRING || t == VarType::PTR;
        }
        std::string emptyString() { needsEmptyString = true; return "empty_str"; }
        const std::vector<size_t> scratchOrder = {8,9,10,11,12,13};
        size_t scratchPtr = 0;
        const std::vector<std::string> registers = {"rax","rbx","rcx","rdx","rsi","rdi","r8","r9","r10","r11","r12","r13","r14","r15"};
        const std::vector<std::string> ptrRegisters = {
            "arg0","arg1","arg2","arg3","arg4","arg5","arg6","arg7","arg8","arg9"
        };
        const std::vector<std::string> floatRegisters = {"xmm0","xmm1","xmm2","xmm3","xmm4","xmm5","xmm6","xmm7","xmm8","xmm9"};

        std::vector<bool> regInUse;
        std::vector<bool> ptrRegInUse; 
        std::vector<bool> floatRegInUse;

        std::vector<std::pair<FuncDeclNode*, std::vector<std::string>>> deferredFuncs;
        std::string currentFunctionName;
        bool functionSetReturn = false;
        int nextSlot = 0;
        int nextTemp = 0;
        int labelCounter = 0;

        std::unordered_map<std::string,int> varSlot;
        std::vector<std::string> prolog;
        std::vector<std::string> instructions;
        std::vector<std::string> evalStack;
        std::vector<std::pair<std::string, std::string>> stringLiterals;
        std::unordered_set<std::string> usedStrings;
        std::unordered_map<int, std::string> slotToName;
        std::unordered_map<std::string, std::vector<std::string>> recordsToFreeInScope;
        std::unordered_map<std::string, std::vector<std::string>> tempPtrByScope;
        std::unordered_map<std::string, bool> declaredProcs;
        std::unordered_map<std::string, bool> declaredFuncs;
        bool generatingDeferredCode = false;

        void addTempPtr(const std::string& v) {
            tempPtrByScope[getCurrentScopeName()].push_back(v);
        }

        struct ValueLocation { enum Type { REGISTER, MEMORY, IMMEDIATE } type; std::string location; };
        std::unordered_map<std::string, ValueLocation> valueLocations;

        static bool endsWithColon(const std::string& s) { return !s.empty() && s.back() == ':'; }
        bool isRegisterSlot(int slot) const { return slot < -1; }
        bool isTempVar(const std::string& name) const { return name.find("__t") != std::string::npos; }

        std::string slotVar(int slot) const {
            auto it = slotToName.find(slot);
            if (it != slotToName.end()) return it->second;
            if (slot < -1 && slot >= - (int)registers.size()) {
                int regIndex = -2 - slot;
                if (regIndex >= 0 && regIndex < (int)registers.size()) return registers[regIndex];
            }
            if (slot <= -100) {
                int regIndex = -100 - slot;
                if (regIndex >= 0 && regIndex < (int)floatRegisters.size()) return floatRegisters[regIndex];
            }
            return "v" + std::to_string(slot);
        }

        std::string allocReg() {
            for (size_t i = 1; i < regInUse.size(); ++i)
                if (!regInUse[i]) { regInUse[i] = true; return registers[i]; }
            size_t idx = scratchOrder[scratchPtr];
            scratchPtr = (scratchPtr + 1) % scratchOrder.size();
            regInUse[idx] = true;
            return registers[idx];
        }
        
        std::string allocPtrReg() {
            for (size_t i = 0; i < ptrRegInUse.size(); ++i)
                if (!ptrRegInUse[i]) { ptrRegInUse[i] = true; return ptrRegisters[i]; }
            
            size_t i = ptrRegisters.size() - 1;
            ptrRegInUse[i] = true;
            return ptrRegisters[i];
        }
        
        void freePtrReg(const std::string& reg) {
            for (size_t i = 0; i < ptrRegisters.size(); ++i)
                if (ptrRegisters[i] == reg) { ptrRegInUse[i] = false; return; }
        }
        
        void freeReg(const std::string& reg) {
            for (size_t i = 0; i < registers.size(); ++i)
                if (registers[i] == reg) { regInUse[i] = false; return; }
            freePtrReg(reg);
        }
        std::string allocFloatReg() {
            for (size_t i = 0; i < floatRegInUse.size(); ++i)
                if (!floatRegInUse[i]) { floatRegInUse[i] = true; return floatRegisters[i]; }
            size_t i = floatRegisters.size()-1;
            floatRegInUse[i] = true;
            return floatRegisters[i];
        }
        void freeFloatReg(const std::string& reg) {
            for (size_t i = 0; i < floatRegisters.size(); ++i)
                if (floatRegisters[i] == reg) { floatRegInUse[i] = false; return; }
        }
        bool isFloatReg(const std::string& name) const { return std::find(floatRegisters.begin(), floatRegisters.end(), name) != floatRegisters.end(); }
        bool isPtrReg(const std::string& name) const { return std::find(ptrRegisters.begin(), ptrRegisters.end(), name) != ptrRegisters.end(); }
        bool isReg(const std::string& name) const {
            return std::find(registers.begin(), registers.end(), name) != registers.end()
                || std::find(floatRegisters.begin(), floatRegisters.end(), name) != floatRegisters.end()
                || isPtrReg(name); 
        }

        int newSlotFor(const std::string& name) {
            auto it = varSlot.find(name);
            if (it != varSlot.end()) return it->second;
            int slot = nextSlot++;
            varSlot[name] = slot;
            if (!name.empty() && name.find("__t") == std::string::npos) slotToName[slot] = name;
            return slot;
        }

        std::string newTemp() { return slotVar(newSlotFor("__t" + std::to_string(nextTemp++))); }
        std::string newLabel(const std::string& prefix = "L") {
            return prefix + "_" + std::to_string(labelCounter++);
        }
        
        void freeTempPtr(const std::string& ptrName) {
            for (size_t i = 0; i < allTempPtrs.size(); ++i) {
                if (allTempPtrs[i] == ptrName) {
                    if (tempPtrInUse[i]) {
                        emit1("free", ptrName);
                        tempPtrInUse[i] = false;
                    }
                    return;
                }
            }
        }

        std::string allocTempPtr(const std::string& forScope = "") {
            for (size_t i = 0; i < allTempPtrs.size(); ++i) {
                if (!tempPtrInUse[i]) {
                    tempPtrInUse[i] = true;
                    std::string tempName = allTempPtrs[i];
                    std::string scope = forScope.empty() ? getCurrentScopeName() : forScope;
                    tempPtrByScope[scope].push_back(tempName);
                    return tempName;
                }
            }

            std::string tempName = "_tmpptr" + std::to_string(nextTemp++);
            allTempPtrs.push_back(tempName);
            tempPtrInUse.push_back(true);
            
            std::string scope = forScope.empty() ? getCurrentScopeName() : forScope;
            tempPtrByScope[scope].push_back(tempName);
            return tempName;
        }

        void emit(const std::string& s) {
            instructions.push_back(s);
        }
        void emitLabel(const std::string& label) { instructions.push_back(label + ":"); }
        void emit1(const std::string& op, const std::string& a) { emit(op + " " + a); }
        void emit2(const std::string& op, const std::string& a, const std::string& b) {
            std::string A = a;
            std::string B = b;
            if (isRealNumber(B) && B.rfind("real_const_",0)!=0)
                B = ensureFloatConstSymbol(B);
            emit(op + " " + A + ", " + B);
        }
        void emit3(const std::string& op, const std::string& a, const std::string& b, const std::string& c) { emit(op + " " + a + ", " + b + ", " + c); }
        void emit4(const std::string& op, const std::string& a, const std::string& b, const std::string& c, const std::string& d) { emit(op + " " + a + ", " + b + ", " + c + ", " + d); }

        std::string escapeStringForMxvm(const std::string& raw) const {
            std::ostringstream o; o << "\"";
            for (char c : raw) {
                if (c=='\\') o<<"\\\\";
                else if (c=='\"') o<<"\\\"";
                else if (c=='\n') o<<"\\n";
                else if (c=='\t') o<<"\\t";
                else o<<c;
            }
            o << "\"";
            return o.str();
        }

        std::string ensureFloatConstSymbol(const std::string& value) {
            if (!isRealNumber(value)) return value;
            if (value.rfind("real_const_", 0) == 0) return value;
            for (const auto &p : realConstants) if (p.second == value) return p.first;
            std::string n = generateRealConstantName();
            realConstants[n] = value;
            usedRealConstants.insert(n);
            return n;
        }

        std::string internString(const std::string& val) {
            for (const auto& p : stringLiterals) if (p.first == val) return p.second;
            std::string key = "str_" + std::to_string(stringLiterals.size());
            stringLiterals.push_back({val, key});
            return key;
        }

        std::string convertCharLiteral(const std::string& c) {
            if (c.length()==3 && c[0]=='\'' && c[2]=='\'') return std::to_string((int)c[1]);
            if (c.length()==4 && c[0]=='\'' && c[1]=='\\' && c[3]=='\'') {
                char e=c[2]; int v=0; switch(e){
                    case 'n':v=10;break;case 't':v=9;break;case 'r':v=13;break;case '\\':v=92;break;case '\'':v=39;break;case '0':v=0;break;default:v=(int)e;break;
                }
                return std::to_string(v);
            }
            return c;
        }

        void pushValue(const std::string& v) { evalStack.push_back(v); if (isReg(v)) recordLocation(v,{ValueLocation::REGISTER,v}); }
        std::string popValue() { if (evalStack.empty()) throw std::runtime_error("Evaluation stack underflow"); std::string v = evalStack.back(); evalStack.pop_back(); return v; }

        std::string eval(ASTNode* n) {
            if (!n) return "0";
            if (auto num = dynamic_cast<NumberNode*>(n)) {
                if (!num->isReal && !isRealNumber(num->value)) return num->value;
            }
            n->accept(*this);
            if (evalStack.empty()) throw std::runtime_error("Expression produced no value");
            return popValue();
        }

        void recordLocation(const std::string& var, ValueLocation loc) { valueLocations[var] = loc; }

        void pushTri(const char* op, const std::string& a, const std::string& b) {
            bool isFloatOp =
                isFloatReg(a) || isFloatReg(b) ||
                isRealNumber(a) || isRealNumber(b) ||
                a.find("real_const_") != std::string::npos ||
                b.find("real_const_") != std::string::npos;

            std::string leftOp = a;
            std::string rightOp = b;
            
            if (isFloatOp) {
                if (!isFloatReg(a) && !isRealNumber(a)) {
                    std::string floatReg = allocFloatReg();
                    emit2("to_float", floatReg, a);
                    leftOp = floatReg;
                }
                if (!isFloatReg(b) && !isRealNumber(b)) {
                    std::string floatReg = allocFloatReg();
                    emit2("to_float", floatReg, b);
                    rightOp = floatReg;
                }
            }

            bool mustCopy = isParmReg(leftOp);
            if (isReg(leftOp) && !mustCopy) {
                emit2(op, leftOp, rightOp);
                pushValue(leftOp);
                if (isReg(b) && !isParmReg(b)) freeReg(b);
                if (rightOp != b) freeReg(rightOp);
            } else {
                std::string result = isFloatOp ? allocFloatReg() : allocReg();
                emit2("mov", result, leftOp);
                emit2(op, result, rightOp);
                pushValue(result);
                if (isReg(b) && !isParmReg(b)) freeReg(b);
                if (leftOp != a) freeReg(leftOp);
                if (rightOp != b) freeReg(rightOp);
            }
        }

        void pushCmpResult(const std::string& a, const std::string& b, const char* jop) {
            std::string t = allocReg();
            std::string L1 = newLabel("CMP_TRUE");
            std::string L2 = newLabel("CMP_END");
            std::string opA = a, opB = b;
            if (isReg(a)) { std::string tempA = allocReg(); emit2("mov", tempA, a); opA = tempA; }
            if (isReg(b)) { std::string tempB = allocReg(); emit2("mov", tempB, b); opB = tempB; }
            if (opA == opB) { std::string dup = allocReg(); emit2("mov", dup, opB); opB = dup; }
            emit2("cmp", opA, opB);
            emit1(jop, L1);
            emit2("mov", t, "0");
            emit1("jmp", L2);
            emitLabel(L1);
            emit2("mov", t, "1");
            emitLabel(L2);
            pushValue(t);
            if (isReg(a) && opA != a) freeReg(opA);
            if (isReg(b) && opB != b) freeReg(opB);
            if (a == b) {
                if (isReg(a) && !isParmReg(a)) freeReg(a);
            } else {
                if (isReg(a) && !isParmReg(a)) freeReg(a);
                if (isReg(b) && !isParmReg(b)) freeReg(b);
            }
        }

        void pushFloatCmpResult(const std::string& aIn, const std::string& bIn, const char* jop) {
            std::string a = aIn, b = bIn;
            if (isRealNumber(a) && a.find("real_const_") != 0) { std::string n = generateRealConstantName(); realConstants[n]=a; usedRealConstants.insert(n); a = n; }
            if (isRealNumber(b) && b.find("real_const_") != 0) { std::string n = generateRealConstantName(); realConstants[n]=b; usedRealConstants.insert(n); b = n; }
            std::string t = allocReg();
            std::string L1 = newLabel("CMP_TRUE");
            std::string L2 = newLabel("CMP_END");
            if (a == b) { std::string dup = allocReg(); emit2("mov", dup, b); b = dup; }
            emit2("fcmp", a, b);
            emit1(jop, L1);
            emit2("mov", t, "0");
            emit1("jmp", L2);
            emitLabel(L1);
            emit2("mov", t, "1");
            emitLabel(L2);
            pushValue(t);
            if (isReg(a) && a != aIn && !isParmReg(aIn)) freeReg(a);
            if (isReg(b) && b != bIn && !isParmReg(bIn)) freeReg(b);
        }

        void pushLogicalAnd(const std::string& a, const std::string& b) {
            std::string t = allocReg();
            std::string L0 = newLabel("AND_ZERO");
            std::string L1 = newLabel("AND_END");
            emit2("cmp", a, "0");
            emit1("je", L0);
            emit2("cmp", b, "0");
            emit1("je", L0);
            emit2("mov", t, "1");
            emit1("jmp", L1);
            emitLabel(L0);
            emit2("mov", t, "0");
            emitLabel(L1);
            pushValue(t);
            if (isReg(a) && !isParmReg(a)) freeReg(a);
            if (isReg(b) && !isParmReg(b)) freeReg(b);
        }

        void pushLogicalOr(const std::string& a, const std::string& b) {
            std::string t = allocReg();
            std::string L1 = newLabel("OR_ONE");
            std::string L2 = newLabel("OR_END");
            emit2("cmp", a, "0");
            emit1("jne", L1);
            emit2("cmp", b, "0");
            emit1("jne", L1);
            emit2("mov", t, "0");
            emit1("jmp", L2);
            emitLabel(L1);
            emit2("mov", t, "1");
            emitLabel(L2);
            pushValue(t);
            if (isReg(a) && !isParmReg(a)) freeReg(a);
            if (isReg(b) && !isParmReg(b)) freeReg(b);
        }

        bool isParmReg(const std::string& name) const {
            for (size_t i = 1; i <= 6 && i < registers.size(); ++i) if (registers[i] == name) return true;
            for (size_t i = 0; i < ptrRegisters.size(); ++i) if (ptrRegisters[i] == name) return true;
            return false;
        }

    public:
        friend class IOFunctionHandler;
        friend class MathFunctionHandler;
        friend class StdFunctionHandler;
        friend class SDLFunctionHandler;
        friend class StringFunctionHandler;

        BuiltinFunctionRegistry builtinRegistry;

        CodeGenVisitor()
            : regInUse(registers.size(), false),
              ptrRegInUse(ptrRegisters.size(), false), 
              floatRegInUse(floatRegisters.size(), false) {
            initializeBuiltins();
        }
        virtual ~CodeGenVisitor() = default;

        void initializeBuiltins() {
            builtinRegistry.registerHandler(std::make_unique<IOFunctionHandler>());
            builtinRegistry.registerHandler(std::make_unique<StdFunctionHandler>());
            builtinRegistry.registerHandler(std::make_unique<SDLFunctionHandler>());
            builtinRegistry.registerHandler(std::make_unique<StringFunctionHandler>());
        }

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
            globalArrays.clear();
            compileTimeConstants.clear();
            functionScopedArrays.clear();
            scopeStack.clear();
            scopeHierarchy.clear();
            scopeStack.push_back({"__global__"});
            scopeHierarchy.push_back("__global__");
            generatingDeferredCode = false; 
            functionSetReturn = false;

            for (size_t i = 0; i < regInUse.size(); ++i) regInUse[i] = false;
            for (size_t i = 0; i < ptrRegInUse.size(); ++i) ptrRegInUse[i] = false; 
            for (size_t i = 0; i < floatRegInUse.size(); ++i) floatRegInUse[i] = false;
            scratchPtr = 0;

            if (auto prog = dynamic_cast<ProgramNode*>(root)) name = prog->name;

            root->accept(*this);

            for (const auto& arrayName : globalArrays) emit1("free", arrayName);
            
            if (recordsToFreeInScope.count("")) {
                for (const auto& recordVar : recordsToFreeInScope[""]) {
                    emit1("free", recordVar);
                }
            }
            if (tempPtrByScope.count("")) {
                for (auto &tp : tempPtrByScope[""]) freeTempPtr(tp);
            }
            emit("done");

            generatingDeferredCode = true; 
            
            for (size_t i = 0; i < deferredProcs.size(); ++i) {
                auto pn = deferredProcs[i].first;
                auto oldHierarchy = scopeHierarchy;  
                scopeHierarchy = deferredProcs[i].second;
                
                std::vector<std::string> parentScope(scopeHierarchy.begin(), scopeHierarchy.end() - 1);
                std::string mangledName = mangleWithScope(pn->name, parentScope);
                
                emitLabel(funcLabel("function PROC_", mangledName)); 
                currentFunctionName = pn->name;
                currentParamLocations.clear();
                currentParamTypes.clear(); 

                if (!pn->parameters.empty()) {
                    size_t intParamIndex = 1;      
                    size_t ptrParamIndex = 0;      
                    size_t floatParamIndex = 0;
                    for (auto& p_node : pn->parameters) {
                        if (auto param = dynamic_cast<ParameterNode*>(p_node.get())) {
                            VarType declType = getTypeFromString(param->type);
                            for (const auto& id : param->identifiers) {
                                VarType useType = declType;
                                if (isRecordTypeName(param->type)) {
                                    useType = VarType::PTR;
                                }
                                std::string incomingReg;
                                if (declType == VarType::DOUBLE) {
                                    if (floatParamIndex < floatRegisters.size())
                                        incomingReg = floatRegisters[floatParamIndex++];
                                } else if (declType == VarType::PTR || declType == VarType::STRING || declType == VarType::RECORD) {
                                    useType = VarType::PTR;
                                    if (ptrParamIndex < ptrRegisters.size())
                                        incomingReg = ptrRegisters[ptrParamIndex++];
                                } else {
                                    if (intParamIndex < registers.size())
                                        incomingReg = registers[intParamIndex++];
                                }
                                std::string mangledId = mangleVariableName(id);
                                int localSlot = newSlotFor(mangledId);
                                setSlotType(localSlot, useType);
                                setVarType(id, useType);
                                if (!incomingReg.empty())
                                    emit2("mov", slotVar(localSlot), incomingReg);
                                currentParamTypes[id] = param->type;
                            }
                        }
                    }
                }

                for (size_t r=0;r<regInUse.size();++r) regInUse[r]=false;
                for (size_t r=0;r<ptrRegInUse.size();++r) ptrRegInUse[r]=false; 
                for (size_t r=0;r<floatRegInUse.size();++r) floatRegInUse[r]=false;

                for(const auto& pair : currentParamLocations) {
                    for(size_t reg_idx = 0; reg_idx < registers.size(); ++reg_idx) {
                        if (registers[reg_idx] == pair.second) { regInUse[reg_idx] = true; break; }
                    }
                }

                if (pn->block) pn->block->accept(*this);
                
                emitLabel("PROC_END_" + mangledName);
                emit("ret");
                scopeHierarchy = oldHierarchy;  
            }

            for (size_t i = 0; i < deferredFuncs.size(); ++i) {
                auto fn = deferredFuncs[i].first;
                auto oldHierarchy = scopeHierarchy; 
                scopeHierarchy = deferredFuncs[i].second;
                
                std::vector<std::string> parentScope(scopeHierarchy.begin(), scopeHierarchy.end() - 1);
                std::string mangledName = mangleWithScope(fn->name, parentScope);

                emitLabel(funcLabel("function FUNC_", mangledName)); 
                currentFunctionName = fn->name;
                currentParamLocations.clear();
                currentParamTypes.clear(); 

                int intParamIndex = 1;
                int ptrParamIndex = 0;
                int floatParamIndex = 0;
                functionSetReturn = false;

                if (!fn->parameters.empty()) {
                    for (auto& p_node : fn->parameters) {
                        if (auto param = dynamic_cast<ParameterNode*>(p_node.get())) {
                            for (const auto& id : param->identifiers) {
                                VarType paramType = getTypeFromString(param->type);
                                std::string incomingReg;
                                if (paramType == VarType::STRING || paramType == VarType::RECORD) {
                                    if (static_cast<size_t>(ptrParamIndex) < ptrRegisters.size()) incomingReg = ptrRegisters[ptrParamIndex++];
                                    paramType = VarType::PTR;
                                } else if (paramType == VarType::DOUBLE) {
                                    if (static_cast<size_t>(floatParamIndex) < floatRegisters.size()) incomingReg = floatRegisters[floatParamIndex++];
                                } else {
                                    if (static_cast<size_t>(intParamIndex) < registers.size()) incomingReg = registers[intParamIndex++];
                                }

                                if (!incomingReg.empty()) {
                                    std::string mangledId = mangleVariableName(id);
                                    int localSlot = newSlotFor(mangledId);
            
                                    setSlotType(localSlot, paramType);
                                    emit2("mov", slotVar(localSlot), incomingReg);
                                }
                                currentParamTypes[id] = param->type; 
                            }
                        }
                    }
                }

                for (size_t r=0;r<regInUse.size();++r) regInUse[r]=false;
                for (size_t r=0;r<ptrRegInUse.size();++r) ptrRegInUse[r]=false; 
                for (size_t r=0;r<floatRegInUse.size();++r) floatRegInUse[r]=false;
                for(const auto& pair : currentParamLocations) {
                    for(size_t reg_idx = 0; reg_idx < registers.size(); ++reg_idx) {
                        if (registers[reg_idx] == pair.second) { regInUse[reg_idx] = true; break; }
                    }
                }

                if (fn->block) fn->block->accept(*this);
            
                emitLabel("FUNC_END_" + mangledName);
                
                if (!functionSetReturn) {
                    VarType rt = getVarType(fn->name);
                    if (rt == VarType::STRING || rt == VarType::PTR || rt == VarType::RECORD) {
                        emit2("mov","arg0",emptyString());
                    } else if (rt == VarType::DOUBLE) {
                        std::string zc = generateRealConstantName();
                        realConstants[zc] = "0.0";
                        usedRealConstants.insert(zc);
                        emit2("mov","xmm0",zc);
                    } else {
                        emit2("mov","rax","0");
                    }
                }
                emit("ret");
                scopeHierarchy = oldHierarchy;  
            }

            generatingDeferredCode = false; 
            currentFunctionName.clear();
            currentParamLocations.clear();
        }

        void emit_invoke(const std::string& funcName, const std::vector<std::string>& params) {
            std::string instruction = "invoke " + funcName;
            for (const auto& p : params) instruction += ", " + p;
            emit(instruction);
        }

        void writeTo(std::ostream& out) const {
            out << "program " << name << " {\n";
            out << "\tsection module {\n ";
            bool first = true;
            for (const auto& mod : usedModules) { if (!first) out << ",\n"; out << "\t\t" << mod; first = false; }
            out << "\n\t}\n";
            out << "\tsection data {\n";
            for (const auto& reg : registers) out << "\t\tint " << reg << " = 0\n";
            for (const auto& floatReg : floatRegisters) out << "\t\tfloat " << floatReg << " = 0.0\n";
            {
                std::set<std::string> temps;
                for (const auto& p : ptrRegisters) temps.insert(p);
                for (const auto& t : allTempPtrs) temps.insert(t);
                for (const auto& kv : tempPtrByScope) for (const auto& t : kv.second) temps.insert(t);
                for (const auto& tempPtr : temps) out << "\t\tptr " << tempPtr << " = null\n";
            }

            for (const auto& constant : realConstants) out << "\t\tfloat " << constant.first << " = " << constant.second << "\n";
            if (needsEmptyString) out << "\t\tstring empty_str = \"\"\n";
            for (const auto& pair : stringLiterals) out << "\t\tstring " << pair.second << " = " << escapeStringForMxvm(pair.first) << "\n";
            if (usedStrings.count("fmt_int")) out << "\t\tstring fmt_int = \"%lld \"\n";
            if (usedStrings.count("fmt_str")) out << "\t\tstring fmt_str = \"%s \"\n";
            if (usedStrings.count("fmt_chr")) out << "\t\tstring fmt_chr = \"%c \"\n";
            if (usedStrings.count("fmt_float")) out << "\t\tstring fmt_float = \"%.6f \"\n";
            if (usedStrings.count("newline")) out << "\t\tstring newline = \"\\n\"\n";
            out << "\t\tstring input_buffer, 256\n";
            for (int i = 0; i < nextSlot; ++i) {
                if (!isRegisterSlot(i) && !isTempVar(slotVar(i)) && !isPtrReg(slotVar(i))) {
                    auto it = slotToType.find(i);
                    if (it != slotToType.end()) {
                        std::string varName = slotVar(i);
                        auto constIt = constInitialValues.find(varName);
                        if (constIt != constInitialValues.end()) out << "\t\t" << constIt->second.first << " " << varName << " = " << constIt->second.second << "\n";
                        else {
                            if (it->second == VarType::STRING) out << "\t\tstring " << varName << " = \"\"\n";
                            else if (it->second == VarType::PTR) out << "\t\tptr " << varName << " = null\n";
                            else if (it->second == VarType::CHAR) out << "\t\tint " << varName << " = 0\n";
                            else if (it->second == VarType::DOUBLE) out << "\t\tfloat " << varName << " = 0.0\n";
                            else out << "\t\tint " << varName << " = 0\n";
                        }
                    } else out << "\t\tint " << slotVar(i) << " = 0\n";
                }
            }
            out << "\t}\n";
            out << "\tsection code {\n";
            out << "\tstart:\n";
            for (auto &s : prolog) out << "\t\t" << s << "\n";
            for (auto &s : instructions) { if (endsWithColon(s)) out << "\t" << s << "\n"; else out << "\t\t" << s << "\n"; }
            out << "\t}\n";
            out << "}\n";
        }

       
        void visit(ProgramNode& node) override { name = node.name; if (node.block) node.block->accept(*this); }
        
        void visit(BlockNode& node) override {
            
            for (auto& decl : node.declarations) {
                if (!decl) continue;
                if (generatingDeferredCode) {
                    if (dynamic_cast<ProcDeclNode*>(decl.get()) ||
                        dynamic_cast<FuncDeclNode*>(decl.get())) {
                        continue;
                    }
                }
                decl->accept(*this);
            }
            if (node.compoundStatement) {
                node.compoundStatement->accept(*this);
            }
        }

       
        std::string getTypeString(const VarDeclNode& node) {
            if (std::holds_alternative<std::string>(node.type))
                return std::get<std::string>(node.type);
            if (std::holds_alternative<std::unique_ptr<ASTNode>>(node.type)) {
                return "record";
            }
            return "";
        }

        void visit(VarDeclNode& node) override {
            for (size_t i = 0; i < node.identifiers.size(); i++) {
                std::string id = node.identifiers[i];
                std::string mangledId = mangleVariableName(id);
                int slot = newSlotFor(mangledId);
                varSlot[id] = slot;
                varSlot[mangledId] = slot;

                std::string typeStr = getTypeString(node);
                VarType vType = getTypeFromString(typeStr);
                setVarType(id, vType);
                setSlotType(slot, vType);

                bool hasInitializer = (i < node.initializers.size() && node.initializers[i]);
                if (!hasInitializer) {
                    
                    if (vType == VarType::PTR || vType == VarType::RECORD) {
                        updateDataSectionInitialValue(slotVar(slot), "ptr", "null");
                    } else if (vType == VarType::DOUBLE) {
                        updateDataSectionInitialValue(slotVar(slot), "float", "0.0");
                    } else {
                        updateDataSectionInitialValue(slotVar(slot), "int", "0");
                    }
                    if (vType == VarType::RECORD) {
                        varRecordType[mangledId] = typeStr;
                        varRecordType[id] = typeStr;
                        int bytes = getTypeSizeByName(typeStr);
                        emit3("alloc", slotVar(slot), "8", std::to_string(bytes));
                        std::string currentScope = getCurrentScopeName();
                        recordsToFreeInScope[currentScope].push_back(mangledId);
                    }
                    continue;
                }

                
                auto initNode = node.initializers[i].get();

                
                if (auto stringNode = dynamic_cast<StringNode*>(initNode)) {
                    std::string sym = internString(stringNode->value);
                
                    if (vType == VarType::PTR) {
                        updateDataSectionInitialValue(slotVar(slot), "ptr", "null");
                        prolog.push_back("mov " + slotVar(slot) + ", " + sym);
                    } else {
                        updateDataSectionInitialValue(slotVar(slot), "string", escapeStringForMxvm(stringNode->value));
                    }
                    continue;
                }

                
                if (auto numNode = dynamic_cast<NumberNode*>(initNode)) {
                    if (vType == VarType::DOUBLE || numNode->isReal) {
                        std::string val = ensureFloatConstSymbol(numNode->value);
                        updateDataSectionInitialValue(slotVar(slot), "float", realConstants[val]);
                    } else {
                        updateDataSectionInitialValue(slotVar(slot), "int", numNode->value);
                    }
                    continue;
                }
                if (auto boolNode = dynamic_cast<BooleanNode*>(initNode)) {
                    updateDataSectionInitialValue(slotVar(slot), "int", boolNode->value ? "1" : "0");
                    continue;
                }

                std::string init = eval(initNode);
                emit2("mov", slotVar(slot), init);
                if (isReg(init) && !isParmReg(init)) freeReg(init);
            }
        }

        
        VarType getTypeFromString(const std::string& typeStr) {
            if (typeStr == "integer" || typeStr == "boolean") return VarType::INT;
            if (typeStr == "real") return VarType::DOUBLE;
            if (typeStr == "string") return VarType::PTR;          
            if (typeStr == "char") return VarType::CHAR;
            if (isRecordTypeName(typeStr)) return VarType::RECORD;
            return VarType::UNKNOWN;
        }

        void visit(ProcCallNode& node) override {
            auto handler = builtinRegistry.findHandler(node.name);
            if (handler) { handler->generate(*this, node.name, node.arguments); return; }

            std::vector<std::string> evaluated_args;
            size_t intRegIdx = 1;      
            size_t ptrRegIdx = 0;      
            size_t floatRegIdx = 0;

            for (auto& arg : node.arguments) {
                std::string argVal = eval(arg.get());
                evaluated_args.push_back(argVal);
                VarType argType = getExpressionType(arg.get());

                std::string targetReg;
                if (argType == VarType::STRING || argType == VarType::PTR || argType == VarType::RECORD) {
                    if (ptrRegIdx < ptrRegisters.size()) targetReg = ptrRegisters[ptrRegIdx++];
                } else if (argType == VarType::DOUBLE) {
                    if (floatRegIdx < floatRegisters.size()) targetReg = floatRegisters[floatRegIdx++];
                } else {
                    if (intRegIdx < registers.size()) targetReg = registers[intRegIdx++];
                }
                if (!targetReg.empty()) emit2("mov", targetReg, argVal);
            }

            std::string mangledName = findMangledFuncName(node.name, true);
            emit1("call", "PROC_" + mangledName);

            for (const auto& arg : evaluated_args)
                if (isReg(arg) && !isParmReg(arg)) freeReg(arg);
        }

        void visit(FuncCallNode& node) override {
            auto handler = builtinRegistry.findHandler(node.name);
            if (handler) {
                if (handler->generateWithResult(*this, node.name, node.arguments)) return;
            }

            std::vector<std::string> evaluated_args;
            size_t intRegIdx = 1;
            size_t ptrRegIdx = 0;
            size_t floatRegIdx = 0;

            for (auto& arg : node.arguments) {
                std::string argVal = eval(arg.get());
                evaluated_args.push_back(argVal);
                VarType argType = getExpressionType(arg.get());
                std::string targetReg;
                if (argType == VarType::STRING || argType == VarType::PTR || argType == VarType::RECORD) {
                    if (ptrRegIdx < ptrRegisters.size()) targetReg = ptrRegisters[ptrRegIdx++];
                } else if (argType == VarType::DOUBLE) {
                    if (floatRegIdx < floatRegisters.size()) targetReg = floatRegisters[floatRegIdx++];
                } else {
                    if (intRegIdx < registers.size()) targetReg = registers[intRegIdx++];
                }
                if (!targetReg.empty()) emit2("mov", targetReg, argVal);
            }

            std::string mangledName = findMangledFuncName(node.name, false);
            emit1("call", "FUNC_" + mangledName);

            auto it = funcSignatures.find(node.name);
            VarType returnType = (it != funcSignatures.end()) ? it->second.returnType : VarType::INT;

            std::string resultLocation;
            if (returnType == VarType::DOUBLE) {
                resultLocation = allocFloatReg();
                emit2("mov", resultLocation, "xmm0");
            } else if (returnType == VarType::STRING || returnType == VarType::PTR || returnType == VarType::RECORD) {
                resultLocation = allocTempPtr();
                emit2("mov", resultLocation, "arg0");
            } else {
                resultLocation = allocReg();
                emit2("mov", resultLocation, "rax");
            }
            pushValue(resultLocation);

            for (const auto& arg : evaluated_args)
                if (isReg(arg) && !isParmReg(arg)) freeReg(arg);
        }

        void visit(FuncDeclNode& node) override {
            std::string mangledName = mangleVariableName(node.name);
            if (declaredFuncs.count(mangledName)) {
                return; 
            }
            declaredFuncs[mangledName] = true;

            FuncInfo funcInfo; funcInfo.returnType = getTypeFromString(node.returnType);
            for (auto& p : node.parameters)
                if (auto pn = dynamic_cast<ParameterNode*>(p.get()))
                    for (size_t i = 0; i < pn->identifiers.size(); ++i)
                        funcInfo.paramTypes.push_back(getTypeFromString(pn->type));
            funcSignatures[node.name] = funcInfo;
            setVarType(node.name, funcInfo.returnType);

            auto path = scopeHierarchy; path.push_back(node.name);
            deferredFuncs.push_back({&node, path});

            scopeHierarchy.push_back(node.name);
            if (node.block) {
                if (auto blockNode = dynamic_cast<BlockNode*>(node.block.get())) {
                    for (auto& decl : blockNode->declarations) {
                        if (dynamic_cast<ProcDeclNode*>(decl.get()) || dynamic_cast<FuncDeclNode*>(decl.get())) {
                            decl->accept(*this);
                        }
                    }
                }
            }
            scopeHierarchy.pop_back();
        }

        void visit(ProcDeclNode& node) override { 
            std::string mangledName = mangleVariableName(node.name);
            if (declaredProcs.count(mangledName)) {
                return; 
            }
            declaredProcs[mangledName] = true;

            auto path = scopeHierarchy; path.push_back(node.name); 
            deferredProcs.push_back({&node, path}); 

            scopeHierarchy.push_back(node.name);
            if (node.block) {
                if (auto blockNode = dynamic_cast<BlockNode*>(node.block.get())) {
                    for (auto& decl : blockNode->declarations) {
                        if (dynamic_cast<ProcDeclNode*>(decl.get()) || dynamic_cast<FuncDeclNode*>(decl.get())) {
                            decl->accept(*this);
                        }
                    }
                }
            }
            scopeHierarchy.pop_back();
        }

        void visit(ParameterNode& node) override {
            for (auto &id : node.identifiers) {
                int slot = newSlotFor(id);
                if (!node.type.empty()) {
                    if (node.type == "string") { setVarType(id, VarType::STRING); setSlotType(slot, VarType::STRING); }
                    else if (node.type == "integer" || node.type == "boolean") { setVarType(id, VarType::INT); setSlotType(slot, VarType::INT); }
                    else if (node.type == "real") { setVarType(id, VarType::DOUBLE); setSlotType(slot, VarType::DOUBLE); }
                    else if (isRecordTypeName(node.type)) { setVarType(id, VarType::RECORD); setSlotType(slot, VarType::PTR); varRecordType[id]=node.type; }
                }
            }
        }

        void visit(CompoundStmtNode& node) override {
            for (auto &stmt : node.statements) {
                if (!stmt) continue;
                if (auto v = dynamic_cast<VariableNode*>(stmt.get())) {
                    std::string mangled = findMangledFuncName(v->name, true);
                    if (declaredProcs.count(mangled)) {
                        emit1("call", "PROC_" + mangled);
                        continue; 
                    }
                }
                stmt->accept(*this);
            }
        }

        void visit(AssignmentNode& node) override {
            if (auto arr = dynamic_cast<ArrayAccessNode*>(node.variable.get())) {
                auto it = arrayInfo.find(arr->arrayName);
                if (it == arrayInfo.end()) throw std::runtime_error("Unknown array: " + arr->arrayName);
                ArrayInfo &info = it->second;

                std::string rhs = eval(node.expression.get());
                std::string idx = eval(arr->index.get());
                if (getExpressionType(arr->index.get()) == VarType::DOUBLE) {
                    std::string intIdx = allocReg();
                    emit2("to_int", intIdx, idx);  
                    if (isReg(idx) && !isParmReg(idx)) freeReg(idx);
                    idx = intIdx;
                }

            #ifdef MXVM_BOUNDS_CHECK
                {
                    std::string idxCopy = allocReg();
                    emit2("mov", idxCopy, idx);
                    emitArrayBoundsCheck(idxCopy, info.lowerBound, info.upperBound);
                }
            #endif

                std::string offset = allocReg();
                emit2("mov", offset, idx);
                if (info.lowerBound != 0) emit2("sub", offset, std::to_string(info.lowerBound));
                std::string mangled = findMangledArrayName(arr->arrayName);
                emit4("store", rhs, mangled, offset, "8");

                if (isReg(rhs) && !isParmReg(rhs)) freeReg(rhs);
                if (isReg(idx) && !isParmReg(idx)) freeReg(idx);
                freeReg(offset);
                return;
            }

            if (auto field = dynamic_cast<FieldAccessNode*>(node.variable.get())) {
                std::string rhs = eval(node.expression.get());
                std::string baseName;
                std::string baseRawName;
                if (auto v = dynamic_cast<VariableNode*>(field->recordExpr.get())) {
                    baseRawName = v->name;
                    baseName = findMangledName(v->name);
                } else {
                    field->recordExpr->accept(*this);
                    baseName = popValue();
                }
                std::string recType = getVarRecordTypeName(baseRawName.empty() ? baseName : baseRawName);
                auto ofs_sz = getRecordFieldOffsetAndSize(recType, field->fieldName);

                int byteOffset = ofs_sz.first;

                VarType fieldType = getTypeFromString(field->fieldName);
                VarType rhsType = getExpressionType(node.expression.get());
                
                if (fieldType == VarType::DOUBLE && rhsType != VarType::DOUBLE && !isFloatReg(rhs)) {
                    std::string floatReg = allocFloatReg();
                    emit2("to_float", floatReg, rhs);
                    if (isReg(rhs) && !isParmReg(rhs)) freeReg(rhs);
                    rhs = floatReg;
                } else if (fieldType != VarType::DOUBLE && rhsType == VarType::DOUBLE && isFloatReg(rhs)) {
                    std::string intReg = allocReg();
                    emit2("to_int", intReg, rhs);
                    if (isReg(rhs) && !isParmReg(rhs)) freeReg(rhs);
                    rhs = intReg;
                }

                emit4("store", rhs, baseName, std::to_string(byteOffset), "8");
                if (isReg(rhs) && !isParmReg(rhs)) freeReg(rhs);
                return;
            }

            
            auto varPtr = dynamic_cast<VariableNode*>(node.variable.get());
            if (!varPtr) return;
            std::string varName = varPtr->name;
            std::string rhs = eval(node.expression.get());
            
            
            VarType varType = getVarType(varName);
            VarType exprType = getExpressionType(node.expression.get());
    
            if (varType == VarType::DOUBLE && exprType != VarType::DOUBLE && !isFloatReg(rhs)) {
                std::string floatReg = allocFloatReg();
                emit2("to_float", floatReg, rhs);
                if (isReg(rhs) && !isParmReg(rhs)) freeReg(rhs);
                rhs = floatReg;
            } else if (varType != VarType::DOUBLE && exprType == VarType::DOUBLE && isFloatReg(rhs)) {
                std::string intReg = allocReg();
                emit2("to_int", intReg, rhs);
                if (isReg(rhs) && !isParmReg(rhs)) freeReg(rhs);
                rhs = intReg;
            }
    
            auto it = currentParamLocations.find(varName);
            if (it != currentParamLocations.end()) {
                emit2("mov", it->second, rhs);
                if (isReg(rhs) && !isParmReg(rhs)) freeReg(rhs);
                return;
            }
            if (!currentFunctionName.empty() && varName == currentFunctionName) {
                VarType rt = getVarType(currentFunctionName);
                if (rt == VarType::STRING || rt == VarType::PTR || rt == VarType::RECORD)
                    emit2("mov","arg0",rhs);      
                else if (rt == VarType::DOUBLE)
                    emit2("mov","xmm0",rhs);
                else
                    emit2("mov","rax",rhs);
                functionSetReturn = true;
            } else {
                std::string mangled = findMangledName(varName);
                emit2("mov", mangled, rhs);
                recordLocation(varName, {ValueLocation::MEMORY, mangled});
            }
            if (isReg(rhs) && !isParmReg(rhs)) freeReg(rhs);
        }

        void visit(IfStmtNode& node) override {
            std::string elseL = newLabel("ELSE");
            std::string endL  = newLabel("ENDIF");
            
            std::string condResult = eval(node.condition.get());

            emit2("cmp", condResult, "0");
            emit1("je", elseL);

            if (isReg(condResult) && !isParmReg(condResult)) {
                freeReg(condResult);
            }
            if (node.thenStatement) {
                node.thenStatement->accept(*this);
            }
            emit1("jmp", endL);
            emitLabel(elseL);
            if (node.elseStatement) {
                node.elseStatement->accept(*this);
            }
            emitLabel(endL);
        }

        void visit(WhileStmtNode& node) override {
            std::string start = newLabel("WHILE");
            std::string end   = newLabel("ENDWHILE");
            emitLabel(start);
            std::string c = eval(node.condition.get());
            emit2("cmp", c, "0");
            emit1("je", end);
            if (isReg(c) && !isParmReg(c)) freeReg(c);
            if (node.statement) node.statement->accept(*this);
            emit1("jmp", start);
            emitLabel(end);
        }

        void visit(ForStmtNode& node) override {
            std::string startVal = eval(node.startValue.get());
            std::string endVal   = eval(node.endValue.get());

            startVal = coerceToIntImmediate(startVal);
            endVal   = coerceToIntImmediate(endVal);

            std::string mangledLoopVar = mangleVariableName(node.variable);
            int slot = newSlotFor(mangledLoopVar);
            emit2("mov", slotVar(slot), startVal);
            if (isReg(startVal) && !isParmReg(startVal)) freeReg(startVal);

            std::string loopStartLabel = newLabel("FOR");
            std::string loopEndLabel   = newLabel("ENDFOR");
            emitLabel(loopStartLabel);

            std::string endReg = allocReg();
            emit2("mov", endReg, endVal);
            emit2("cmp", slotVar(slot), endReg);
            if (node.isDownto) emit1("jl", loopEndLabel); else emit1("jg", loopEndLabel);

            if (node.statement) node.statement->accept(*this);
            if (node.isDownto) emit2("sub", slotVar(slot), "1"); else emit2("add", slotVar(slot), "1");
            emit1("jmp", loopStartLabel);
            emitLabel(loopEndLabel);

            if (isReg(endReg) && !isParmReg(endReg)) freeReg(endReg);
            if (isReg(endVal) && !isParmReg(endVal)) freeReg(endVal);
        }

        void visit(BinaryOpNode& node) override {
            auto isStrLike = [&](VarType v){ return v == VarType::STRING || v == VarType::PTR; };
            VarType lt = getExpressionType(node.left.get());
            VarType rt = getExpressionType(node.right.get());

            if (node.operator_ == BinaryOpNode::PLUS && (isStrLike(lt) || isStrLike(rt))) {
                usedModules.insert("string");
                std::string left  = eval(node.left.get());
                std::string right = eval(node.right.get());
                std::string len1 = allocReg(), len2 = allocReg(), totalLen = allocReg();
                emit_invoke("strlen", {left});
                emit("return " + len1);
                emit_invoke("strlen", {right});
                emit("return " + len2);
                emit2("mov", totalLen, len1);
                emit2("add", totalLen, len2);
                emit2("add", totalLen, "1");

                std::string result_str = allocTempPtr();
                emit3("alloc", result_str, "1", totalLen);
                emit_invoke("strncpy", {result_str, left, len1});
                emit_invoke("strncat", {result_str, right, len2});

                pushValue(result_str);
                freeReg(len1); freeReg(len2); freeReg(totalLen);
                if (isReg(left) && !isParmReg(left)) freeReg(left);
                if (isReg(right) && !isParmReg(right)) freeReg(right);
                return;
            }

            try {
                std::string folded = foldNumeric(&node);
                if (!folded.empty()) {
                    if (node.operator_ == BinaryOpNode::DIVIDE && isIntegerLiteral(folded)) folded += ".0";
                    if (isFloatLiteral(folded)) pushValue(ensureFloatConstSymbol(folded));
                    else pushValue(folded);
                    return;
                }
            } catch (...) { }

            auto evalOperand = [&](ASTNode* n)->std::string {
                if (auto var = dynamic_cast<VariableNode*>(n)) {
                    std::string v;
                    if (tryGetConstNumeric(var->name, v)) return v;
                }
                return eval(n);
            };

            std::string left  = evalOperand(node.left.get());
            std::string right = evalOperand(node.right.get());


            bool isRealDivide = (node.operator_ == BinaryOpNode::DIVIDE);
            if (isRealDivide) {
                bool leftIsNum  = isIntegerLiteral(left)  || isFloatLiteral(left);
                bool rightIsNum = isIntegerLiteral(right) || isFloatLiteral(right);
                if (leftIsNum && rightIsNum) {
                    if (isIntegerLiteral(left) && isIntegerLiteral(right)) {
                        left += ".0";
                        right += ".0";
                    }
                } else {
                    if (lt != VarType::DOUBLE && rt != VarType::DOUBLE) {
                        if (isIntegerLiteral(left) && !isFloatLiteral(left))  left += ".0";
                        else if (isIntegerLiteral(right) && !isFloatLiteral(right)) right += ".0";
                    }
                }
            }

            if (node.operator_ == BinaryOpNode::DIV && (isFloatLiteral(left) || isFloatLiteral(right))) {
                if (isFloatLiteral(left))  left  = std::to_string((long long)std::stod(left));
                if (isFloatLiteral(right)) right = std::to_string((long long)std::stod(right));
            }

            auto emitBinary = [&](const char* op) {
                std::string dst;
                bool leftIsUsableReg = isReg(left) && !isParmReg(left);
                if (leftIsUsableReg) dst = left;
                else { dst = allocReg(); emit2("mov", dst, left); }
                emit2(op, dst, right);
                if (isReg(right) && !isParmReg(right)) freeReg(right);
                pushValue(dst);
            };

            switch (node.operator_) {
                case BinaryOpNode::PLUS:      emitBinary("add"); break;
                case BinaryOpNode::MINUS:     emitBinary("sub"); break;
                case BinaryOpNode::MULTIPLY:  emitBinary("mul"); break;
                case BinaryOpNode::DIVIDE:    emitBinary("div"); break;
                case BinaryOpNode::DIV:       emitBinary("div"); break;
                case BinaryOpNode::MOD:       emitBinary("mod"); break;
                case BinaryOpNode::AND:       pushLogicalAnd(left, right); break;
                case BinaryOpNode::OR:        pushLogicalOr(left, right); break;
                case BinaryOpNode::EQUAL:         pushCmpResult(left, right, "je");  break;
                case BinaryOpNode::NOT_EQUAL:     pushCmpResult(left, right, "jne"); break;
                case BinaryOpNode::LESS:          pushCmpResult(left, right, "jl");  break;
                case BinaryOpNode::LESS_EQUAL:    pushCmpResult(left, right, "jle"); break;
                case BinaryOpNode::GREATER:       pushCmpResult(left, right, "jg");  break;
                case BinaryOpNode::GREATER_EQUAL: pushCmpResult(left, right, "jge"); break;
                default: throw std::runtime_error("Unsupported binary operator");
            }
        }

        VarType getExpressionType(ASTNode* node) {
            if (dynamic_cast<NumberNode*>(node)) { auto n = static_cast<NumberNode*>(node); return n->isReal ? VarType::DOUBLE : VarType::INT; }
            if (dynamic_cast<StringNode*>(node)) { return VarType::STRING; }
            if (dynamic_cast<BooleanNode*>(node)) return VarType::BOOL;
            if (auto varNode = dynamic_cast<VariableNode*>(node)) return getVarType(varNode->name);
            if (auto funcCall = dynamic_cast<FuncCallNode*>(node)) {
                auto handler = builtinRegistry.findHandler(funcCall->name);
                if (handler) return handler->getReturnType(funcCall->name);
                auto it = funcSignatures.find(funcCall->name);
                if (it != funcSignatures.end()) return it->second.returnType;
            }
            if (auto fieldNode = dynamic_cast<FieldAccessNode*>(node)) {
                std::string baseRawName;
                if (auto v = dynamic_cast<VariableNode*>(fieldNode->recordExpr.get())) baseRawName = v->name;
                std::string recType = getVarRecordTypeName(baseRawName);
                auto it = recordTypes.find(recType);
                if (it != recordTypes.end()) {
                    auto jt = it->second.nameToIndex.find(fieldNode->fieldName);
                    if (jt != it->second.nameToIndex.end()) {
                        const auto& f = it->second.fields[jt->second];
                        return getTypeFromString(f.typeName);
                    }
                }
            }
            if (auto a = dynamic_cast<ArrayAccessNode*>(node)) {
                auto it = arrayInfo.find(a->arrayName);
                if (it != arrayInfo.end()) {
                    const std::string& t = it->second.elementType;
                    if (t == "integer") return VarType::INT;
                    if (t == "real") return VarType::DOUBLE;
                    if (t == "char") return VarType::CHAR;
                    if (t == "string") return VarType::STRING;
                    if (t == "boolean") return VarType::BOOL;
                }
            }
            if (auto b = dynamic_cast<BinaryOpNode*>(node)) {
                VarType lt = getExpressionType(b->left.get());
                VarType rt = getExpressionType(b->right.get());
                if (b->operator_ == BinaryOpNode::PLUS) {
                    auto isStrLike = [](VarType v){ return v == VarType::STRING || v == VarType::PTR; };
                    if (isStrLike(lt) || isStrLike(rt)) return VarType::PTR;
                }
                if (lt == VarType::DOUBLE || rt == VarType::DOUBLE) return VarType::DOUBLE;
                return lt;
            }
            return VarType::INT;
        }

        void visit(UnaryOpNode& node) override {
            std::string v = eval(node.operand.get());
            switch (node.operator_) {
                case UnaryOpNode::MINUS: { std::string t = allocReg(); emit2("mov", t, "0"); emit2("sub", t, v); pushValue(t); break; }
                case UnaryOpNode::PLUS: { pushValue(v); break; }
                case UnaryOpNode::NOT: {
                    emit("not " + v);
                    pushValue(v);
                    break;
                }
            }
        }

        void visit(VariableNode& node) override {
            auto it = currentParamLocations.find(node.name);
            if (it != currentParamLocations.end()) { pushValue(it->second); return; }

            std::string mangledName = findMangledName(node.name);
            auto constIt = compileTimeConstants.find(mangledName);
            if (constIt != compileTimeConstants.end()) { pushValue(constIt->second); return; }
            constIt = compileTimeConstants.find(node.name);
            if (constIt != compileTimeConstants.end()) { pushValue(constIt->second); return; }
            pushValue(findMangledName(node.name));
        }

        void visit(NumberNode& node) override {
            if (node.isReal || isRealNumber(node.value)) {
                std::string reg = allocFloatReg();
                emit2("mov", reg, ensureFloatConstSymbol(node.value));
                pushValue(reg);
            } else {
                pushValue(node.value);
            }
        }
        void visit(StringNode& node) override { std::string sym = internString(node.value); pushValue(sym); }
        void visit(BooleanNode& node) override { std::string reg = allocReg(); emit2("mov", reg, node.value ? "1" : "0"); pushValue(reg); }
        void visit(EmptyStmtNode& /*node*/) override {}

    private:
        std::string evaluateConstantExpression(ASTNode* node) {
            if (!node) return "";
            if (auto numNode = dynamic_cast<NumberNode*>(node)) {
                return numNode->value;
            }
            if (auto varNode = dynamic_cast<VariableNode*>(node)) {
                auto it = compileTimeConstants.find(findMangledName(varNode->name));
                if (it != compileTimeConstants.end()) return it->second;
                it = compileTimeConstants.find(varNode->name);
                if (it != compileTimeConstants.end()) return it->second;
                throw std::runtime_error("Cannot use non-constant variable '" + varNode->name + "' in a constant expression.");
            }
            if (auto binOp = dynamic_cast<BinaryOpNode*>(node)) {
                std::string L = evaluateConstantExpression(binOp->left.get());
                std::string R = evaluateConstantExpression(binOp->right.get());
                if (L.empty() || R.empty()) throw std::runtime_error("Unsupported node type in constant expression.");

                bool isFloatOp = isFloatLiteral(L) || isFloatLiteral(R) || binOp->operator_ == BinaryOpNode::DIVIDE;

                auto toD = [&](const std::string& s){ return std::stod(s); };
                auto toI = [&](const std::string& s){
                    return isFloatLiteral(s) ? static_cast<long long>(std::stod(s)) : std::stoll(s);
                };

                if (isFloatOp) {
                    double result;
                    switch (binOp->operator_) {
                        case BinaryOpNode::PLUS:     result = toD(L) + toD(R); break;
                        case BinaryOpNode::MINUS:    result = toD(L) - toD(R); break;
                        case BinaryOpNode::MULTIPLY: result = toD(L) * toD(R); break;
                        case BinaryOpNode::DIVIDE:
                            if (toD(R) == 0.0) throw std::runtime_error("division by zero in constant expression");
                            result = toD(L) / toD(R); break;
                        default: throw std::runtime_error("Unsupported operator for floats in constant expression.");
                    }
                    return std::to_string(result);
                } else { 
                    long long result;
                    switch (binOp->operator_) {
                        case BinaryOpNode::PLUS:     result = toI(L) + toI(R); break;
                        case BinaryOpNode::MINUS:       result = toI(L) - toI(R); break;
                        case BinaryOpNode::MULTIPLY: result = toI(L) * toI(R); break;
                        case BinaryOpNode::DIV:
                            if (toI(R) == 0) throw std::runtime_error("division by zero in constant expression");
                            result = toI(L) / toI(R); break;
                        case BinaryOpNode::MOD:
                            if (toI(R) == 0) throw std::runtime_error("mod by zero in constant expression");
                            result = toI(L) % toI(R); break;
                        default: throw std::runtime_error("Unsupported operator for integers in constant expression.");
                    }
                    return std::to_string(result);
                }
            }
            throw std::runtime_error("Unsupported node type in constant expression.");
        }

    public:
        void visit(ConstDeclNode& node) override {
            for (const auto& assignment : node.assignments) {
                if (auto strNode = dynamic_cast<StringNode*>(assignment->value.get())) {
                    std::string sym = internString(strNode->value);

                    std::string mangledName = mangleVariableName(assignment->identifier);
                    compileTimeConstants[mangledName] = mangledName;
                    compileTimeConstants[assignment->identifier] = mangledName;

                    int slot = newSlotFor(mangledName);
                    setVarType(assignment->identifier, VarType::PTR);
                    setSlotType(slot, VarType::PTR);
                    updateDataSectionInitialValue(slotVar(slot), "ptr", "null");
                    prolog.push_back("mov " + slotVar(slot) + ", " + sym);
                    continue;
                }

                try {
                    std::string literalValue = evaluateConstantExpression(assignment->value.get());
                    bool isFloat = isRealNumber(literalValue);
                    if (isFloat) literalValue = ensureFloatConstSymbol(literalValue);

                    std::string varType = isFloat ? "float" : "int";
                    std::string mangledName = mangleVariableName(assignment->identifier);

                    compileTimeConstants[mangledName] = (isFloat ? realConstants[literalValue] : literalValue);
                    compileTimeConstants[assignment->identifier] = (isFloat ? realConstants[literalValue] : literalValue);

                    int slot = newSlotFor(mangledName);
                    VarType vType = isFloat ? VarType::DOUBLE : VarType::INT;
                    setVarType(assignment->identifier, vType);
                    setSlotType(slot, vType);

                    updateDataSectionInitialValue(slotVar(slot), varType,
                        isFloat ? realConstants[literalValue] : literalValue);
                } catch (const std::runtime_error&) {
                    std::string valueReg = eval(assignment->value.get());
                    int slot = newSlotFor(assignment->identifier);
                    VarType exprType = getExpressionType(assignment->value.get());
                    setVarType(assignment->identifier, exprType);
                    setSlotType(slot, exprType);
                    std::string varLocation = slotVar(slot);
                    emit2("mov", varLocation, valueReg);
                    if (isReg(valueReg) && !isParmReg(valueReg)) freeReg(valueReg);
                }
            }
        }

        void updateDataSectionInitialValue(const std::string& varName, const std::string& type, const std::string& value) { constInitialValues[varName] = {type, value}; }

        void visit(RepeatStmtNode& node) override {
            std::string startLabel = newLabel("REPEAT");
            std::string endLabel = newLabel("UNTIL");
            emitLabel(startLabel);
            for (auto& stmt : node.statements) if (stmt) stmt->accept(*this);
            std::string condResult = eval(node.condition.get());
            emit2("cmp", condResult, "0");
            emit1("je", startLabel);
            if (isReg(condResult) && !isParmReg(condResult)) freeReg(condResult);
        }

       

        void visit(CaseStmtNode& node) override {
            std::string switchExpr = eval(node.expression.get());
            std::string endLabel = newLabel("CASE_END");
            std::vector<std::string> branchLabels;
            for (size_t i = 0; i < node.branches.size(); i++) branchLabels.push_back(newLabel("CASE_" + std::to_string(i)));
            std::string elseLabel = newLabel("CASE_ELSE");
            for (size_t i = 0; i < node.branches.size(); i++) {
                auto& branch = node.branches[i];
                for (auto& value : branch->values) {
                    std::string caseValue = eval(value.get());
                    emit2("cmp", switchExpr, caseValue);
                    emit1("je", branchLabels[i]);
                    if (isReg(caseValue) && !isParmReg(caseValue)) freeReg(caseValue);
                }
            }
            if (node.elseStatement) emit1("jmp", elseLabel); else emit1("jmp", endLabel);
            for (size_t i = 0; i < node.branches.size(); i++) {
                emitLabel(branchLabels[i]);
                if (node.branches[i]->statement) node.branches[i]->statement->accept(*this);
                emit1("jmp", endLabel);
            }
            if (node.elseStatement) { emitLabel(elseLabel); node.elseStatement->accept(*this); }
            emitLabel(endLabel);
            if (isReg(switchExpr) && !isParmReg(switchExpr)) freeReg(switchExpr);
        }

    private:
        std::unordered_map<std::string, std::string> realConstants;
        void visit(ArrayTypeNode& /*node*/) override {}
        struct ArrayInfo {
            std::string elementType;
            int lowerBound;
            int upperBound;
            int size;
            int elementSize;
        };
        std::unordered_map<std::string, ArrayInfo> arrayInfo;

        int getArrayElementSize(const std::string& elementType) {
            if (elementType == "integer" || elementType == "boolean") return 8;
            if (elementType == "real") return 8;
            if (elementType == "char") return 8;
            if (elementType == "string" || elementType == "ptr") return 8;
            auto it = recordTypes.find(elementType);
            if (it != recordTypes.end()) return it->second.size; 
            return 8;
        }

    void emitArrayBoundsCheck(const std::string& idxReg, int lower, int upper) {
        #ifdef MXVM_BOUNDS_CHECK
            
            std::string L_ok = newLabel("IDX_OK");
            std::string L_fail = newLabel("IDX_OOB");
            std::string t = allocReg();
            emit2("mov", t, idxReg);
            emit2("cmp", t, std::to_string(lower));
            emit1("jl", L_fail);
            emit2("cmp", t, std::to_string(upper));
            emit1("jg", L_fail);
            emit1("jmp", L_ok);
            emitLabel(L_fail);
            emit1("exit", "1");        
            emitLabel(L_ok);
            if (!isParmReg(idxReg) && isReg(idxReg)) freeReg(idxReg);
        #endif
    }


    void visit(ArrayDeclarationNode& node) override {
            std::string arrayName = node.name;
            std::string mangledArrayName = mangleVariableName(arrayName);

            std::string lowerBoundStr = eval(node.arrayType->lowerBound.get());
            std::string upperBoundStr = eval(node.arrayType->upperBound.get());
            lowerBoundStr = coerceToIntImmediate(lowerBoundStr);
            upperBoundStr = coerceToIntImmediate(upperBoundStr);

            int lowerBound = std::stoi(lowerBoundStr);
            int upperBound = std::stoi(upperBoundStr);
            int size = upperBound - lowerBound + 1;
            int elementSize = getArrayElementSize(node.arrayType->elementType);

            ArrayInfo info{node.arrayType->elementType, lowerBound, upperBound, size, elementSize};
            arrayInfo[arrayName] = info;

            std::string currentScope = getCurrentScopeName();
            if (currentScope.empty()) globalArrays.push_back(mangledArrayName);
            else functionScopedArrays[currentScope].push_back(mangledArrayName);

            setVarType(arrayName, VarType::PTR);
            int slot = newSlotFor(mangledArrayName);
            setSlotType(slot, VarType::PTR);
            varSlot[arrayName] = slot;
            varSlot[mangledArrayName] = slot;

            emit3("alloc", mangledArrayName, "8", std::to_string(size));

            for (size_t i = 0; i < node.initializers.size() && i < (size_t)size; ++i) {
                std::string value = eval(node.initializers[i].get());
                std::string byteOffset = std::to_string(i);
                emit4("store", value, mangledArrayName, byteOffset, "8");
                if (isReg(value) && !isParmReg(value)) freeReg(value);
            }
        }
        void visit(ArrayAccessNode& node) override {
            std::string mangledArrayName = findMangledArrayName(node.arrayName);
            auto it = arrayInfo.find(node.arrayName);
            if (it == arrayInfo.end()) throw std::runtime_error("Unknown array: " + node.arrayName);
            ArrayInfo &info = it->second;

            std::string idx = eval(node.index.get());
            if (getExpressionType(node.index.get()) == VarType::DOUBLE) {
                std::string intIdx = allocReg();
                emit2("to_int", intIdx, idx);  
                if (isReg(idx) && !isParmReg(idx)) freeReg(idx);
                idx = intIdx;
            }

#ifdef MXVM_BOUNDS_CHECK
    {
        std::string idxCopy = allocReg();
        emit2("mov", idxCopy, idx);
        emitArrayBoundsCheck(idxCopy, info.lowerBound, info.upperBound);
    }
#endif

    std::string offset = allocReg();
    emit2("mov", offset, idx);
    if (info.lowerBound != 0) emit2("sub", offset, std::to_string(info.lowerBound));
    

    VarType elemType = getExpressionType(&node);
    std::string dst = (elemType == VarType::DOUBLE) ? allocFloatReg()
                : (elemType == VarType::PTR || elemType == VarType::STRING || elemType == VarType::RECORD) ? allocPtrReg()
                : allocReg();

    emit4("load", dst, mangledArrayName, offset, "8");
    pushValue(dst);

    if (isReg(idx) && !isParmReg(idx)) freeReg(idx);
    freeReg(offset);
        }


        void visit(ArrayAssignmentNode& node) override {
            auto it = arrayInfo.find(node.arrayName);
            if (it == arrayInfo.end()) throw std::runtime_error("Unknown array: " + node.arrayName);
            ArrayInfo &info = it->second;

            std::string value = eval(node.value.get());
            std::string index = eval(node.index.get());
            if (getExpressionType(node.index.get()) == VarType::DOUBLE) {
                std::string intIndex = allocReg();
                emit2("to_int", intIndex, index);  
                if (isReg(index) && !isParmReg(index)) freeReg(index);
                index = intIndex;
            }

            std::string mangled = findMangledArrayName(node.arrayName);
            std::string offsetReg = allocReg();
            emit2("mov", offsetReg, index);
            if (info.lowerBound != 0) emit2("sub", offsetReg, std::to_string(info.lowerBound));
            emit4("store", value, mangled, offsetReg, "8");

            if (isReg(value) && !isParmReg(value)) freeReg(value);
            if (isReg(index) && !isParmReg(index)) freeReg(index);
            freeReg(offsetReg);
        }

        void visitArrayAssignment(ArrayAccessNode& node, const std::string& value) {
            std::string mangledArrayName = findMangledArrayName(node.arrayName);
            auto it = arrayInfo.find(node.arrayName);
            if (it == arrayInfo.end()) throw std::runtime_error("Unknown array: " + node.arrayName);
            ArrayInfo& info = it->second;
            std::string indexValue = eval(node.index.get());
            VarType indexType = getExpressionType(node.index.get());
            if (indexType == VarType::DOUBLE) {
                std::string intIndex = allocReg();
                emit2("to_int", intIndex, indexValue);
                if (isReg(indexValue) && !isParmReg(indexValue)) freeReg(indexValue);
                indexValue = intIndex;
            }
            std::string adjustedIndexReg = allocReg();
            emit2("mov", adjustedIndexReg, indexValue);
            emit2("sub", adjustedIndexReg, std::to_string(info.lowerBound));
            emit4("store", value, mangledArrayName, adjustedIndexReg, "8");
            freeReg(adjustedIndexReg);
            if (isReg(indexValue) && !isParmReg(indexValue)) freeReg(indexValue);
        }

        std::string findMangledFuncName(const std::string& name, bool isProc) const {
            auto& lookupMap = isProc ? declaredProcs : declaredFuncs;
            for (int depth = (int)scopeHierarchy.size() - 1; depth >= 0; --depth) {
                std::vector<std::string> tempScope(scopeHierarchy.begin(), scopeHierarchy.begin() + depth + 1);
                std::string candidate = mangleWithScope(name, tempScope);
                if (lookupMap.count(candidate)) {
                    return candidate;
                }
            }
            return name; 
        }

        std::string findMangledName(const std::string& name) const {
            if (scopeHierarchy.empty()) return name;
            for (int depth = (int)scopeHierarchy.size() - 1; depth >= 0; --depth) {
                std::string candidate;
                if (depth == 0) candidate = name;
                else {
                    for (int i = 1; i <= depth; ++i) { if (i > 1) candidate += "_"; candidate += scopeHierarchy[i]; }
                    candidate += "_" + name;
                }
                auto it = varSlot.find(candidate);
                if (it != varSlot.end()) return candidate;
            }
            return name;
        }

        std::string findMangledArrayName(const std::string& name) const {
            if (scopeHierarchy.empty()) return name;
            for (int depth = (int)scopeHierarchy.size() - 1; depth >= 1; --depth) {
                std::string candidate;
                for (int i = 1; i <= depth; ++i) { if (i > 1) candidate += "_"; candidate += scopeHierarchy[i]; }
                candidate += "_" + name;
                if (varSlot.count(candidate)) return candidate;
            }
            return name;
        }

        bool isIntegerLiteral(const std::string& s) const {
            if (s.empty()) return false;
            size_t i = (s[0]=='+'||s[0]=='-') ? 1 : 0;
            if (i >= s.size()) return false;
            for (; i < s.size(); ++i) if (!std::isdigit(static_cast<unsigned char>(s[i]))) return false;
            return true;
        }

        bool isFloatLiteral(const std::string& s) const {
            if (s.find_first_of(".eE") == std::string::npos) return false;
            char *end = nullptr;
            std::strtod(s.c_str(), &end);
            return end && *end == '\0';
        }

        bool tryGetConstNumeric(const std::string& name, std::string& out) {
            auto itM = compileTimeConstants.find(findMangledName(name));
            if (itM != compileTimeConstants.end() && (isIntegerLiteral(itM->second) || isFloatLiteral(itM->second))) {
                out = itM->second; return true;
            }
            auto it = compileTimeConstants.find(name);
            if (it != compileTimeConstants.end() && (isIntegerLiteral(it->second) || isFloatLiteral(it->second))) {
                out = it->second; return true;
            }
            return false;
        }

        std::string foldNumeric(ASTNode* n) {
            if (!n) return "";
            if (auto num = dynamic_cast<NumberNode*>(n)) return num->value;
            if (auto var = dynamic_cast<VariableNode*>(n)) {
                std::string v;
                if (tryGetConstNumeric(var->name, v)) return v;
                return "";
            }
            if (auto bin = dynamic_cast<BinaryOpNode*>(n)) {
                std::string L = foldNumeric(bin->left.get());
                std::string R = foldNumeric(bin->right.get());
                if (L.empty() || R.empty()) return "";
                auto toD = [&](const std::string& s)->double { return std::stod(s); };
                auto toI = [&](const std::string& s)->long long {
                    return isFloatLiteral(s) ? static_cast<long long>(std::stod(s)) : std::stoll(s);
                               };
                switch (bin->operator_) {
                    case BinaryOpNode::PLUS:
                        return (isFloatLiteral(L)||isFloatLiteral(R)) ? std::to_string(toD(L)+toD(R)) : std::to_string(toI(L)+toI(R));
                                       case BinaryOpNode::MINUS:
                        return (isFloatLiteral(L)||isFloatLiteral(R)) ? std::to_string(toD(L)-toD(R)) : std::to_string(toI(L)-toI(R));
                    case BinaryOpNode::MULTIPLY:
                        return (isFloatLiteral(L)||isFloatLiteral(R)) ? std::to_string(toD(L)*toD(R)) : std::to_string(toI(L)*toI(R));
                    case BinaryOpNode::DIVIDE: {
                        double rv = toD(R); if (rv==0.0) throw std::runtime_error("division by zero");
                        return std::to_string(toD(L)/rv);
                    }
                    case BinaryOpNode::DIV: {
                        long long ri = toI(R); if (ri==0) throw std::runtime_error("division by zero");
                        return std::to_string(toI(L)/ri);
                    }
                    case BinaryOpNode::MOD: {
                        long long ri = toI(R); if (ri==0) throw std::runtime_error("mod by zero");
                        return std::to_string(toI(L)%ri);
                    }
                    default: return "";
                }
            }
            return "";
        }


        struct RecordField { std::string name; std::string typeName; int offset; int size; };
        struct RecordTypeInfo { int size = 0; std::vector<RecordField> fields; std::unordered_map<std::string,int> nameToIndex; };
        std::unordered_map<std::string, RecordTypeInfo> recordTypes;
        std::unordered_map<std::string, std::string> varRecordType;

        int getTypeSizeByName(const std::string& t) {
            if (t == "integer" || t == "boolean") return 8;
            if (t == "real") return 8;
            if (t == "char") return 8;        
            if (t == "string" || t == "ptr") return 8;
            auto it = recordTypes.find(t);
            if (it != recordTypes.end()) return it->second.size;
            return 8;       
        }

        bool isRecordTypeName(const std::string& t) { return recordTypes.find(t) != recordTypes.end(); }

        std::pair<int,int> getRecordFieldOffsetAndSize(const std::string& recType, const std::string& field) {
            auto it = recordTypes.find(recType);
            if (it == recordTypes.end()) return {0,8};
            auto jt = it->second.nameToIndex.find(field);
            if (jt == it->second.nameToIndex.end()) return {0,8};
            const auto& f = it->second.fields[jt->second];
            return {f.offset, f.size};
        }

        std::string getVarRecordTypeName(const std::string& varName) {
            auto param_it = currentParamTypes.find(varName);
            if (param_it != currentParamTypes.end()) {
                return param_it->second;
            }
            auto it = varRecordType.find(findMangledName(varName));
            if (it != varRecordType.end()) {
                return it->second;
            }
        
            it = varRecordType.find(varName);
            if (it != varRecordType.end()) {
                return it->second;
            }
            return ""; 
        }

        void visit(RecordTypeNode& /*node*/) override {}
        
        void visit(RecordDeclarationNode& node) override {
            RecordTypeInfo info;
            int offset = 0;
            for (auto& f : node.recordType->fields) {
                auto& fieldDecl = static_cast<VarDeclNode&>(*f);
                for (const auto& fieldName : fieldDecl.identifiers) {
                    std::string typeStr = getTypeString(fieldDecl);
                    int sz = getTypeSizeByName(typeStr);  
                    RecordField rf{fieldName, typeStr, offset, sz};
                    info.nameToIndex[rf.name] = (int)info.fields.size();
                    info.fields.push_back(rf);
                    offset += sz;
                }
            }
            info.size = offset;
            recordTypes[node.name] = info;
        }

        void visit(FieldAccessNode& node) override {
            std::string baseName;
            std::string baseRawName;
            if (auto v = dynamic_cast<VariableNode*>(node.recordExpr.get())) {
                baseRawName = v->name;
                baseName = findMangledName(v->name);
            } else {
                node.recordExpr->accept(*this);
                baseName = popValue();
            }
            std::string recType = getVarRecordTypeName(baseRawName.empty() ? baseName : baseRawName);
            auto ofs_sz = getRecordFieldOffsetAndSize(recType, node.fieldName);

            int byteOffset = ofs_sz.first;
            int elementSize = ofs_sz.second;

            VarType fieldType = getTypeFromString(node.fieldName);
            VarType rhsType = getExpressionType(node.recordExpr.get());
            
            if (fieldType == VarType::DOUBLE && rhsType != VarType::DOUBLE && !isFloatReg(baseName)) {
                std::string floatReg = allocFloatReg();
                emit2("to_float", floatReg, baseName);
                if (isReg(baseName) && !isParmReg(baseName)) freeReg(baseName);
                baseName = floatReg;
            } else if (fieldType != VarType::DOUBLE && rhsType == VarType::DOUBLE && isFloatReg(baseName)) {
                std::string intReg = allocReg();
                emit2("to_int", intReg, baseName);
                if (isReg(baseName) && !isParmReg(baseName)) freeReg(baseName);
                baseName = intReg;
            }

            std::string dst = allocReg();
            emit4("load", dst, baseName, std::to_string(byteOffset), std::to_string(elementSize));
            pushValue(dst);
        }
        void visit(TypeDeclNode& node) override {
            for (auto& typeDecl : node.typeDeclarations) {
                typeDecl->accept(*this);
            }
        }

        void visit(TypeAliasNode& node) override {
            typeAliases[node.typeName] = node.baseType;
        }

        void visit(ExitNode& node) override {
            if (node.expr) {
                std::string retVal = eval(node.expr.get());
                VarType rt = VarType::UNKNOWN;
                if (!currentFunctionName.empty()) {
                    rt = getVarType(currentFunctionName);
                }
                     
                VarType exprType = getExpressionType(node.expr.get());
                if (rt == VarType::DOUBLE && exprType != VarType::DOUBLE && !isFloatReg(retVal)) {
                    std::string floatReg = allocFloatReg();
                    emit2("to_float", floatReg, retVal);
                    if (isReg(retVal) && !isParmReg(retVal)) freeReg(retVal);
                    retVal = floatReg;
                } else if (rt != VarType::DOUBLE && exprType == VarType::DOUBLE && isFloatReg(retVal)) {
                    std::string intReg = allocReg();
                    emit2("to_int", intReg, retVal);
                    if (isReg(retVal) && !isParmReg(retVal)) freeReg(retVal);
                    retVal = intReg;
                }
               
                if (rt == VarType::STRING || rt == VarType::PTR || rt == VarType::RECORD) {
                    emit2("mov", "arg0", retVal);
                } else if (rt == VarType::DOUBLE) {
                    emit2("mov", "xmm0", retVal);
                } else {
                    emit2("mov", "rax", retVal);
                }
                
                functionSetReturn = true;
                if (isReg(retVal) && !isParmReg(retVal)) freeReg(retVal);
            }    
            std::string endLabel = getCurrentEndLabel();
            if (!endLabel.empty()) {
                emit1("jmp", endLabel);
            } else {
                emit("ret");
            }
        }

    private:
        std::string getCurrentEndLabel() const {
            if (currentFunctionName.empty()) return "";
            
            
            std::string mangledName = findMangledFuncName(currentFunctionName, false); 
            if (declaredFuncs.count(mangledName)) {
                return "FUNC_END_" + mangledName;
            }
            
            mangledName = findMangledFuncName(currentFunctionName, true); 
            if (declaredProcs.count(mangledName)) {
                return "PROC_END_" + mangledName;
            }
            
            return "";
        }
    };

  
    std::string mxvmOpt(const std::string &text);

} 
#endif
