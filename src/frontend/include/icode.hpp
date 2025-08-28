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


    struct ArrayInfo {
        std::string elementType;
        int lowerBound = 0;
        int upperBound = -1;
        int size = 0;           
        int elementSize = 8;    
        bool elementIsArray = false;
        std::unique_ptr<ArrayInfo> elementArray; 

        
        ArrayInfo() = default;
        ArrayInfo(const ArrayInfo& other) 
            : elementType(other.elementType), lowerBound(other.lowerBound), 
              upperBound(other.upperBound), size(other.size), elementSize(other.elementSize),
              elementIsArray(other.elementIsArray) {
            if (other.elementArray) {
                elementArray = std::make_unique<ArrayInfo>(*other.elementArray);
            }
        }
        ArrayInfo(ArrayInfo&& other) noexcept = default;
        ArrayInfo& operator=(const ArrayInfo& other) {
            if (this != &other) {
                elementType = other.elementType;
                lowerBound = other.lowerBound;
                upperBound = other.upperBound;
                size = other.size;
                elementSize = other.elementSize;
                elementIsArray = other.elementIsArray;
                if (other.elementArray) {
                    elementArray = std::make_unique<ArrayInfo>(*other.elementArray);
                } else {
                    elementArray.reset();
                }
            }
            return *this;
        }
        ArrayInfo& operator=(ArrayInfo&& other) noexcept = default;
    };

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
            std::string scope = forScope.empty() ? getCurrentScopeName() : forScope;
            if (scope.empty()) {
                scope = "";
            }

            for (size_t i = 0; i < allTempPtrs.size(); ++i) {
                if (!tempPtrInUse[i]) {
                    tempPtrInUse[i] = true;
                    std::string tempName = allTempPtrs[i];
                    tempPtrByScope[scope].push_back(tempName);
                    return tempName;
                }
            }

            std::string tempName = "_tmpptr" + std::to_string(nextTemp++);
            allTempPtrs.push_back(tempName);
            tempPtrInUse.push_back(true);

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
        void emit3(const std::string& op, const std::string& a, const std::string& b, const std::string& c) {
            instructions.push_back(op + " " + a + ", " + b + ", " + c);
            if (op == "alloc") {
                allocatedPtrs.insert(a);
            }
        }
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

        void allocateRecordFieldArrays(const std::string& recordVarName, const std::string& recordTypeName) {
            auto recTypeIt = recordTypes.find(recordTypeName);
            if (recTypeIt == recordTypes.end()) return;
            auto& recInfo = recTypeIt->second;
            std::string currentScope = getCurrentScopeName();

            for (const auto& field : recInfo.fields) {
                if (!field.isArray) continue;

                std::string fieldArrayName = recordVarName + "_" + field.name;

                int slot = newSlotFor(fieldArrayName);
                setSlotType(slot, VarType::PTR);
                updateDataSectionInitialValue(slotVar(slot), "ptr", "null");

                emit3("alloc",
                      slotVar(slot),
                      std::to_string(field.arrayInfo.elementSize),
                      std::to_string(field.arrayInfo.size));

                std::string basePtr = ensurePtrBase(recordVarName);
                emit4("store", slotVar(slot), basePtr, std::to_string(field.offset), "1");

                if (currentScope.empty()) globalArrays.push_back(slotVar(slot));
                else functionScopedArrays[currentScope].push_back(slotVar(slot));
            }
        }

        ArrayInfo buildArrayInfoFromNode(ArrayTypeNode* atn) {
            auto lc = [](std::string s){ std::transform(s.begin(), s.end(), s.begin(),
                                        [](unsigned char c){ return std::tolower(c); }); return s; };
            auto resolveTypeName = [&](std::string t){
                t = lc(t);
                std::unordered_set<std::string> seen;
                while (typeAliases.count(t) && !seen.count(t)) {
                    seen.insert(t);
                    t = lc(typeAliases.at(t));
                }
                return t;
            };

            ArrayInfo info{};
            info.lowerBound = std::stoi(evaluateConstantExpression(atn->lowerBound.get()));
            info.upperBound = std::stoi(evaluateConstantExpression(atn->upperBound.get()));
            info.size = info.upperBound - info.lowerBound + 1;

            if (auto* childArr = dynamic_cast<ArrayTypeNode*>(atn->elementType.get())) {
                info.elementType   = "array";
                info.elementIsArray = true;
                info.elementArray  = std::make_unique<ArrayInfo>(buildArrayInfoFromNode(childArr));
                info.elementSize   = info.elementArray->size * info.elementArray->elementSize;
            } else if (auto* childSimple = dynamic_cast<SimpleTypeNode*>(atn->elementType.get())) {
                info.elementType = resolveTypeName(childSimple->typeName);
                info.elementSize = getArrayElementSize(info.elementType);
            } else if (dynamic_cast<RecordTypeNode*>(atn->elementType.get())) {
                info.elementType = "record";
                info.elementSize = 8; 
            } else {
                info.elementType = "integer";
                info.elementSize = 8;
            }
            return info;
        }


        int getArrayElementSize(const std::string& tIn) {
            auto t = resolveTypeName(lc(tIn));
            if (t == "integer" || t == "boolean") return 8;
            if (t == "real") return 8;
            if (t == "char") return 8;
            if (t == "string" || t == "ptr") return 8;
            auto it = recordTypes.find(t);
            if (it != recordTypes.end()) return it->second.size;
            return 8;
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

         
        void emitFree(const std::string& s) {
            if (allocatedPtrs.erase(s)) {        
                emit("free " + s);
            }
        }

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
            recordTypes.clear();
            varRecordType.clear();
            typeAliases.clear();
            arrayInfo.clear();

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
        
            emit("done");

            generatingDeferredCode = true; 
            
            for (size_t i = 0; i < deferredProcs.size(); ++i) {
                auto pn = deferredProcs[i].first;
                auto oldHierarchy = scopeHierarchy;
                scopeHierarchy = deferredProcs[i].second;

                std::vector<std::string> parentScope(scopeHierarchy.begin(), scopeHierarchy.end() - 1);
                std::string mangledName = mangleWithScope(pn->name, parentScope);

                std::string scopeName = "PROC_" + mangledName;

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
                                currentParamTypes[id] = resolveTypeName(param->type);
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

                for (const auto& recVar : recordsToFreeInScope[scopeName]) {
                    emit1("free", recVar);
                }
                for (const auto& tp : tempPtrByScope[scopeName]) {
                    if(allocatedPtrs.count(tp))
                        emitFree(tp);
                }

                emit("ret");
                scopeHierarchy = oldHierarchy;
            }

          for (size_t i = 0; i < deferredFuncs.size(); ++i) {
                auto fn = deferredFuncs[i].first;
                auto oldHierarchy = scopeHierarchy;
                scopeHierarchy = deferredFuncs[i].second;

                std::vector<std::string> parentScope(scopeHierarchy.begin(), scopeHierarchy.end() - 1);
                std::string mangledName = mangleWithScope(fn->name, parentScope);

                std::string scopeName = "FUNC_" + mangledName;

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

                for (const auto& recVar : recordsToFreeInScope[scopeName]) {
                    emit1("free", recVar);
                }
                for (const auto& tp : tempPtrByScope[scopeName]) {
                    if(allocatedPtrs.count(tp))
                        emitFree(tp);
                }

                emit("ret");
                scopeHierarchy = oldHierarchy;
            }

            generatingDeferredCode = false; 
            currentFunctionName.clear();
            currentParamLocations.clear();
        }

        void markAllocatedPtr(const std::string& p) { allocatedPtrs.insert(p); }

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
                if (dynamic_cast<TypeDeclNode*>(decl.get())) {
                    decl->accept(*this);
                }
            }

            for (auto& decl : node.declarations) {
                if (!decl) continue;
                if (dynamic_cast<TypeDeclNode*>(decl.get())) continue; 

                if (generatingDeferredCode) {
                    if (dynamic_cast<ProcDeclNode*>(decl.get()) ||
                        dynamic_cast<FuncDeclNode*>(decl.get())) {
                        continue;
                    }
                }
                decl->accept(*this);
            }

            if (node.compoundStatement) node.compoundStatement->accept(*this);
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
        for (const auto& varName : node.identifiers) {
            std::string mangledName = mangleVariableName(varName);
            std::string typeName;

            if (std::holds_alternative<std::string>(node.type)) {
                typeName = std::get<std::string>(node.type);
            } else if (std::holds_alternative<std::unique_ptr<ASTNode>>(node.type)) {
                auto& typeNode = std::get<std::unique_ptr<ASTNode>>(node.type);

                if (auto* arrayTypeNode = dynamic_cast<ArrayTypeNode*>(typeNode.get())) {
                    ArrayInfo info = buildArrayInfoFromNode(arrayTypeNode);
                    arrayInfo[mangledName] = std::move(info);

                    setVarType(varName, VarType::PTR);
                    int slot = newSlotFor(mangledName);
                    setSlotType(slot, VarType::PTR);
                    varSlot[varName] = slot;
                    varSlot[mangledName] = slot;

                    updateDataSectionInitialValue(slotVar(slot), "ptr", "null");
                    emit3("alloc", slotVar(slot),
                        std::to_string(arrayInfo[mangledName].elementSize),
                        std::to_string(arrayInfo[mangledName].size));

                    std::string currentScope = getCurrentScopeName();
                    if (currentScope.empty())  globalArrays.push_back(slotVar(slot));
                    else                       functionScopedArrays[currentScope].push_back(slotVar(slot));
                    continue;
                }

                if (auto* simpleTypeNode = dynamic_cast<SimpleTypeNode*>(typeNode.get())) {
                    typeName = simpleTypeNode->typeName;
                }

            } else {
                typeName = "unknown";
            }

            
            int slot = newSlotFor(mangledName);
            varSlot[varName] = slot;
            varSlot[mangledName] = slot;

            std::string normalizedTypeName = resolveTypeName(lc(typeName));
            auto recordTypeIt = recordTypes.find(normalizedTypeName);
            if (recordTypeIt != recordTypes.end()) {
                // RECORD variable
                varRecordType[mangledName] = normalizedTypeName;
                varRecordType[varName]     = normalizedTypeName;

                int recordSize = recordTypeIt->second.size;

                setVarType(varName, VarType::RECORD);
                setSlotType(slot, VarType::PTR);

                updateDataSectionInitialValue(slotVar(slot), "ptr", "null");
                emit3("alloc", slotVar(slot), std::to_string(recordSize), "1");

                allocateRecordFieldArrays(mangledName, normalizedTypeName);

                std::string currentScope = getCurrentScopeName();
                recordsToFreeInScope[currentScope].push_back(mangledName);

            } else {
                // Handle named array types
                auto arrayInfoIt = arrayInfo.find(normalizedTypeName);
                if (arrayInfoIt != arrayInfo.end()) {
                    arrayInfo[mangledName] = arrayInfoIt->second; // Copy array info
                    setVarType(varName, VarType::PTR);
                    setSlotType(slot, VarType::PTR);
                    updateDataSectionInitialValue(slotVar(slot), "ptr", "null");
                    emit3("alloc", slotVar(slot),
                        std::to_string(arrayInfo[mangledName].elementSize),
                        std::to_string(arrayInfo[mangledName].size));
                    std::string currentScope = getCurrentScopeName();
                    if (currentScope.empty()) globalArrays.push_back(slotVar(slot));
                    else functionScopedArrays[currentScope].push_back(slotVar(slot));
                    continue; // Skip to next variable
                }

                // non-record path unchanged
                auto arrayInfoIt_var = arrayInfo.find(varName);
                if (arrayInfoIt_var != arrayInfo.end()) {
                    setVarType(varName, VarType::PTR);
                    setSlotType(slot, VarType::PTR);
                } else {
                    VarType vType = getTypeFromString(typeName);
                    setVarType(varName, vType);
                    setSlotType(slot, vType);

                    if (vType == VarType::PTR || vType == VarType::RECORD) {
                        updateDataSectionInitialValue(slotVar(slot), "ptr", "null");
                    } else if (vType == VarType::DOUBLE) {
                        updateDataSectionInitialValue(slotVar(slot), "float", "0.0");
                    } else {
                        updateDataSectionInitialValue(slotVar(slot), "int", "0");
                    }
                }
            }
        }
    }


    std::string storageSymbolFor(const std::string& mangled) {
        auto it = varSlot.find(mangled);
        if (it != varSlot.end()) return slotVar(it->second);
        return mangled;
    }
    bool isRecordTypeName(const std::string& t) {
        auto base = resolveTypeName(lc(t));
        return recordTypes.find(base) != recordTypes.end();
    }

    int getTypeSizeByName(const std::string& t) {
        auto base = resolveTypeName(lc(t));
        if (base == "integer" || base == "boolean") return 8;
        if (base == "real") return 8;
        if (base == "char") return 8;
        if (base == "string" || base == "ptr") return 8;
        auto it = recordTypes.find(base);
        if (it != recordTypes.end()) return it->second.size;
        return 8;
    }

    VarType getTypeFromString(const std::string& typeStr) {
        auto base = resolveTypeName(lc(typeStr));
        if (base == "integer" || base == "boolean") return VarType::INT;
        if (base == "real") return VarType::DOUBLE;
        if (base == "string") return VarType::PTR;
        if (base == "char") return VarType::CHAR;
        if (isRecordTypeName(base)) return VarType::RECORD;
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

            FuncInfo funcInfo;
            funcInfo.returnType = getTypeFromString(node.returnType);
            for (auto& p : node.parameters) {
                if (auto pn = dynamic_cast<ParameterNode*>(p.get())) {
                    for (size_t i = 0; i < pn->identifiers.size(); ++i) {
                        funcInfo.paramTypes.push_back(getTypeFromString(pn->type));
                    }
                }
            }
            funcSignatures[node.name] = funcInfo;
            setVarType(node.name, funcInfo.returnType);

            auto path = scopeHierarchy;
            path.push_back(node.name);
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

            auto path = scopeHierarchy;
            path.push_back(node.name);
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
        void visit(TypeAliasNode& node) override {
            auto lc = [](std::string s){ std::transform(s.begin(), s.end(), s.begin(),
                                        [](unsigned char c){ return std::tolower(c); }); return s; };
            typeAliases[lc(node.typeName)] = lc(node.baseType);
        }

        void visit(ParameterNode& node) override {
            auto lc = [](std::string s){ std::transform(s.begin(), s.end(), s.begin(),
                                        [](unsigned char c){ return std::tolower(c); }); return s;
            };
            for (auto &idRaw : node.identifiers) {
                std::string id = idRaw;
                int slot = newSlotFor(id);
                std::string t = lc(node.type);
                if (t == "string") { 
                    setVarType(id, VarType::STRING); 
                    setSlotType(slot, VarType::STRING); 
                } else if (t == "integer" || t == "boolean") { 
                    setVarType(id, VarType::INT); 
                    setSlotType(slot, VarType::INT); 
                } else if (t == "real") { 
                    setVarType(id, VarType::DOUBLE); 
                    setSlotType(slot, VarType::DOUBLE); 
                } else {
                    std::string rt = resolveTypeName(t);
                    if (recordTypes.count(rt)) { 
                        setVarType(id, VarType::RECORD); 
                        setSlotType(slot, VarType::PTR); 
                        varRecordType[id] = rt;  
                        currentParamTypes[id] = rt;
                    }
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
            if (isReg(t)) freeReg(t);
#endif
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

            std::string left = evalOperand(node.left.get());
            std::string right = evalOperand(node.right.get());


            bool needsFloatOp = (lt == VarType::DOUBLE || rt == VarType::DOUBLE || 
                                isFloatLiteral(left) || isFloatLiteral(right) ||
                                node.operator_ == BinaryOpNode::DIVIDE);
            

            if (node.operator_ == BinaryOpNode::DIVIDE) {
                needsFloatOp = true;
                if (isIntegerLiteral(left) && !isFloatLiteral(left)) left += ".0";
                if (isIntegerLiteral(right) && !isFloatLiteral(right)) right += ".0";
            }
            

            if (node.operator_ == BinaryOpNode::DIV && (isFloatLiteral(left) || isFloatLiteral(right))) {
                if (isFloatLiteral(left)) left = std::to_string((long long)std::stod(left));
                if (isFloatLiteral(right)) right = std::to_string((long long)std::stod(right));
                needsFloatOp = false;
            }


            if (needsFloatOp && (node.operator_ == BinaryOpNode::PLUS || 
                                node.operator_ == BinaryOpNode::MINUS || 
                                node.operator_ == BinaryOpNode::MULTIPLY || 
                                node.operator_ == BinaryOpNode::DIVIDE)) {
                if (!isFloatLiteral(left) && !isFloatReg(left)) {
                    std::string floatReg = allocFloatReg();
                    emit2("to_float", floatReg, left);
                    if (isReg(left) && !isParmReg(left)) freeReg(left);
                    left = floatReg;
                }
                

                if (!isFloatLiteral(right) && !isFloatReg(right)) {
                    std::string floatReg = allocFloatReg();
                    emit2("to_float", floatReg, right);
                    if (isReg(right) && !isParmReg(right)) freeReg(right);
                    right = floatReg;
                }
                

                std::string dst;
                if (isFloatReg(left) && !isParmReg(left)) dst = left;
                else {
                    dst = allocFloatReg();
                    emit2("mov", dst, left);
                }
                
                emit2(node.operator_ == BinaryOpNode::DIVIDE ? "div" : 
                    node.operator_ == BinaryOpNode::MULTIPLY ? "mul" : 
                    node.operator_ == BinaryOpNode::MINUS ? "sub" : "add", dst, right);
                
                if (isReg(right) && !isParmReg(right)) freeReg(right);
                pushValue(dst);
                return;
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
                std::string recType = getVarRecordTypeNameFromExpr(fieldNode->recordExpr.get());
                 recType = resolveTypeName(lc(recType)); 
                auto it = recordTypes.find(recType);
                if (it != recordTypes.end()) {
                    auto jt = it->second.nameToIndex.find(lc(fieldNode->fieldName));
                    if (jt != it->second.nameToIndex.end()) {
                        const auto& f = it->second.fields[jt->second];
                        if (f.isArray) {
                            return VarType::PTR;
                        }
                        return getTypeFromString(f.typeName);
                    }
                }
            }
            if (auto a = dynamic_cast<ArrayAccessNode*>(node)) {
                ArrayInfo* info = getArrayInfoForArrayAccess(a);
                if (info) {
                    if (info->elementIsArray) return VarType::PTR;
                    const std::string& t = info->elementType;
                    if (t == "integer" || t == "boolean") return VarType::INT;
                    if (t == "real")    return VarType::DOUBLE;
                    if (t == "char")    return VarType::CHAR;
                    if (t == "string" || t == "ptr") return VarType::PTR;
                    if (isRecordTypeName(t)) return VarType::RECORD;
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
            
            if (auto pit = currentParamLocations.find(node.name); pit != currentParamLocations.end()) {
                pushValue(pit->second);
                return;
            }

            std::string mangled = findMangledName(node.name);

            
            if (auto ct = compileTimeConstants.find(mangled); ct != compileTimeConstants.end()) {
                pushValue(ct->second);
                return;
            }
            if (auto ct2 = compileTimeConstants.find(node.name); ct2 != compileTimeConstants.end()) {
                pushValue(ct2->second);
                return;
            }

            
            if (auto sit = varSlot.find(mangled); sit != varSlot.end()) {
                pushValue(slotVar(sit->second));
                return;
            }

            
            pushValue(mangled);
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
            if (auto un = dynamic_cast<UnaryOpNode*>(node)) {
                std::string v = evaluateConstantExpression(un->operand.get());
                if (v.empty()) throw std::runtime_error("Unsupported node type in constant expression.");
                double dv = std::stod(v);
                switch (un->operator_) {
                    case UnaryOpNode::MINUS: return std::to_string(-dv);
                    case UnaryOpNode::PLUS:  return v;
                    default: throw std::runtime_error("Unsupported unary op in constant expression.");
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
        std::unordered_set<std::string> allocatedPtrs;   

        std::unordered_map<std::string, std::string> realConstants;
        void visit(ArrayTypeNode& /*node*/) override {}
        
        std::unordered_map<std::string, ArrayInfo> arrayInfo;

    std::string getTypeString(ASTNode* typeNode) {
        if (auto simpleType = dynamic_cast<SimpleTypeNode*>(typeNode)) {
            return simpleType->typeName;
        } else if (dynamic_cast<ArrayTypeNode*>(typeNode)) {
            return "array"; 
        } else if (dynamic_cast<RecordTypeNode*>(typeNode)) {
            return "record";
        }
        return "unknown";
    }

    void visit(ArrayDeclarationNode& node) override {
        std::string mangledName = findMangledName(node.name);
        
        
        std::string elementType;
        if (auto simpleType = dynamic_cast<SimpleTypeNode*>(node.arrayType->elementType.get())) {
            elementType = simpleType->typeName;
        } else if (dynamic_cast<ArrayTypeNode*>(node.arrayType->elementType.get())) {
            elementType = "array";
        } else if (dynamic_cast<RecordTypeNode*>(node.arrayType->elementType.get())) {
            elementType = "record";
        } else {
            elementType = "integer"; 
        }
        
        int lowerBound = std::stoi(evaluateConstantExpression(node.arrayType->lowerBound.get()));
        int upperBound = std::stoi(evaluateConstantExpression(node.arrayType->upperBound.get()));
        int size = upperBound - lowerBound + 1;
        
        int elementSize = 8; 
        
        if (isRecordTypeName(elementType)) {
            auto it = recordTypes.find(elementType);
            if (it != recordTypes.end()) {
                elementSize = it->second.size;
            }
        } else {
            elementSize = getArrayElementSize(elementType);
        }
        
        ArrayInfo info;
        info.elementType = elementType;
        info.lowerBound = lowerBound;
        info.upperBound = upperBound;
        info.size = size;
        info.elementSize = elementSize;
        mangledName = findMangledName(node.name);
        arrayInfo[mangledName] = std::move(info);
        int slot = newSlotFor(mangledName);
        varSlot[node.name] = slot;
        varSlot[mangledName] = slot;
        setVarType(node.name, VarType::PTR);
        setSlotType(slot, VarType::PTR);
        updateDataSectionInitialValue(slotVar(slot), "ptr", "null");
        emit3("alloc",
              slotVar(slot),
              std::to_string(elementSize),
              std::to_string(size));
        std::string currentScope = getCurrentScopeName();
        if (currentScope.empty())  globalArrays.push_back(slotVar(slot));
        else                       functionScopedArrays[currentScope].push_back(slotVar(slot));
    }

    static inline std::string lc(std::string s) {
        std::transform(s.begin(), s.end(), s.begin(),
                    [](unsigned char c){ return std::tolower(c); });
        return s;
    }

    std::string resolveTypeName(std::string t) const {
        if (t.empty()) return t;
        t = lc(t);
        std::unordered_set<std::string> seen;
        while (true) {
            if (!typeAliases.count(t)) break;
            if (seen.count(t)) break;          
            seen.insert(t);
            t = lc(typeAliases.at(t));
        }
        return t;
    }


    void visit(FieldAccessNode& node) override {
        std::string baseName;            
        std::string recType;             
        bool baseIsDirectPointer = false;

        if (auto* v = dynamic_cast<VariableNode*>(node.recordExpr.get())) {
            baseName = findMangledName(v->name);
            recType  = getVarRecordTypeName(v->name);               
        } else if (dynamic_cast<ArrayAccessNode*>(node.recordExpr.get())) {
            node.recordExpr->accept(*this);
            baseName = popValue();                                  
            baseIsDirectPointer = true;
            recType  = getVarRecordTypeNameFromExpr(node.recordExpr.get());
        } else {
            node.recordExpr->accept(*this);
            baseName = popValue();
            baseIsDirectPointer = true;
            recType  = getVarRecordTypeNameFromExpr(node.recordExpr.get());
        }

        recType = resolveTypeName(lc(recType));
        if (recType.empty()) {
            throw std::runtime_error("Unknown record type: (empty) for field " + node.fieldName);
        }

        auto recTypeIt = recordTypes.find(recType);
        if (recTypeIt == recordTypes.end()) {
            throw std::runtime_error("Unknown record type: " + recType + " for field " + node.fieldName);
        }

        const std::string fieldName = lc(node.fieldName);
        auto& recInfo = recTypeIt->second;
        auto  it      = recInfo.nameToIndex.find(fieldName);
        if (it == recInfo.nameToIndex.end()) {
            throw std::runtime_error("Field not found in record: " + fieldName + " in type " + recType);
        }

        const auto& fieldInfo = recInfo.fields[it->second];
        const int   byteOffset = fieldInfo.offset;

        std::string basePtr = baseIsDirectPointer ? baseName : ensurePtrBase(baseName);

        if (fieldInfo.isArray || isRecordTypeName(fieldInfo.typeName)) {
            std::string p = allocTempPtr();
            emit2("mov", p, basePtr);
            emit2("add", p, std::to_string(byteOffset));
            pushValue(p);
            return;
        }

        VarType fieldType = getTypeFromString(fieldInfo.typeName);
        std::string dst;
        if (fieldType == VarType::DOUBLE) {
            dst = allocFloatReg();
        } else if (fieldType == VarType::PTR || fieldType == VarType::STRING) {
            dst = allocTempPtr();
        } else {
            dst = allocReg();
        }

        emit4("load", dst, basePtr, std::to_string(byteOffset), "1");
        pushValue(dst);
    }
   
    void visit(AssignmentNode& node) override {
        if (auto arr = dynamic_cast<ArrayAccessNode*>(node.variable.get())) {
            ArrayInfo* info = getArrayInfoForArrayAccess(arr);
            if (!info) throw std::runtime_error("Unknown array: " + getArrayNameFromBase(arr->base.get()));

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
                emitArrayBoundsCheck(idxCopy, info->lowerBound, info->upperBound);
            }
        #endif

            std::string elemIndex = allocReg();
            emit2("mov", elemIndex, idx);
            if (info->lowerBound != 0) emit2("sub", elemIndex, std::to_string(info->lowerBound));
            std::string base;
            if (auto var = dynamic_cast<VariableNode*>(arr->base.get())) {
                std::string mangled = findMangledArrayName(var->name);
                base = ensurePtrBase(storageSymbolFor(mangled));
            } else if (auto field = dynamic_cast<FieldAccessNode*>(arr->base.get())) {
                field->recordExpr->accept(*this);
                std::string recPtr = popValue();
                std::string recType = getVarRecordTypeNameFromExpr(field->recordExpr.get());
                auto ofs_sz = getRecordFieldOffsetAndSize(recType, field->fieldName);
                int fieldOffset = ofs_sz.first;

                std::string arrayPtr = allocTempPtr();
                emit2("mov", arrayPtr, recPtr);
                emit2("add", arrayPtr, std::to_string(fieldOffset));
                base = arrayPtr;
            } else if (dynamic_cast<ArrayAccessNode*>(arr->base.get())) {
                arr->base->accept(*this);
                base = popValue();             
                base = ensurePtrBase(base);    
            } else {
                throw std::runtime_error("Unsupported array base in assignment");
            }
            
            VarType elemType = getTypeFromString(info->elementType);
            VarType rhsType  = getExpressionType(node.expression.get());
            if (elemType == VarType::DOUBLE && rhsType != VarType::DOUBLE && !isFloatReg(rhs)) {
                std::string floatReg = allocFloatReg();
                emit2("to_float", floatReg, rhs);
                if (isReg(rhs) && !isParmReg(rhs)) freeReg(rhs);
                rhs = floatReg;
            } else if (elemType != VarType::DOUBLE && rhsType == VarType::DOUBLE && isFloatReg(rhs)) {
                std::string intReg = allocReg();
                emit2("to_int", intReg, rhs);
                if (isReg(rhs) && !isParmReg(rhs)) freeReg(rhs);
                rhs = intReg;
            }

            
            if (isRecordTypeName(info->elementType)) {
                throw std::runtime_error("Cannot assign directly to record array element");
            } else {
                emit4("store", rhs, base, elemIndex, std::to_string(info->elementSize));
            }

            if (isReg(rhs) && !isParmReg(rhs)) freeReg(rhs);
            if (isReg(idx) && !isParmReg(idx)) freeReg(idx);
            freeReg(elemIndex);
            return;
        }

        
        if (auto field = dynamic_cast<FieldAccessNode*>(node.variable.get())) {
            std::string rhs = eval(node.expression.get());
            std::string baseName;
            std::string recType;
            bool baseIsDirectPointer = false;

            if (auto v = dynamic_cast<VariableNode*>(field->recordExpr.get())) {
                baseName = findMangledName(v->name);
                recType = getVarRecordTypeName(v->name);
            } else {
                field->recordExpr->accept(*this);
                baseName = popValue();
                baseIsDirectPointer = true;
                recType = getVarRecordTypeNameFromExpr(field->recordExpr.get());
            }

            recType = resolveTypeName(lc(recType)); 

            auto recTypeIt = recordTypes.find(recType);
            if (recTypeIt == recordTypes.end()) {
                throw std::runtime_error("Unknown record type: " + recType);
            }
            auto& recInfo = recTypeIt->second;
            auto fieldIndexIt = recInfo.nameToIndex.find(lc(field->fieldName)); 
            if (fieldIndexIt == recInfo.nameToIndex.end()) {
                throw std::runtime_error("Field not found in record: " + field->fieldName);
            }
            auto& fieldInfo = recInfo.fields[fieldIndexIt->second];
            int byteOffset  = fieldInfo.offset;

            VarType fieldType = getTypeFromString(fieldInfo.typeName);
            VarType rhsType   = getExpressionType(node.expression.get());
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

            std::string base = baseIsDirectPointer ? baseName : ensurePtrBase(baseName);

            
            emit4("store", rhs, base, std::to_string(byteOffset), "1");

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
                emit2("mov", "arg0", rhs);
            else if (rt == VarType::DOUBLE)
                emit2("mov", "xmm0", rhs);
            else
                emit2("mov", "rax", rhs);
            functionSetReturn = true;
        } else {
            std::string mangled = findMangledName(varName);
            auto it = varSlot.find(mangled);
            if (it != varSlot.end()) {
                emit2("mov", slotVar(it->second), rhs);
                recordLocation(varName, {ValueLocation::MEMORY, slotVar(it->second)});
            } else {
                emit2("mov", mangled, rhs);
                recordLocation(varName, {ValueLocation::MEMORY, mangled});
            }
        }

        if (isReg(rhs) && !isParmReg(rhs)) freeReg(rhs);
    }


    int getRecordTypeSize(const std::string& typeName) {
        auto it = recordTypes.find(typeName);
        if (it != recordTypes.end()) {
            return it->second.size;
        }
        return 8;  
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

    #ifdef MXVM_BOUNDS_CHECK
        {
            std::string idxCopy = allocReg();
            emit2("mov", idxCopy, index);
            emitArrayBoundsCheck(idxCopy, info.lowerBound, info.upperBound);
            if (isReg(idxCopy)) freeReg(idxCopy);
        }
    #endif

        std::string elementIndex = allocReg();
        emit2("mov", elementIndex, index);
        if (info.lowerBound != 0) emit2("sub", elementIndex, std::to_string(info.lowerBound));

        VarType elemType = VarType::INT;
        if (info.elementType == "real") elemType = VarType::DOUBLE;
        else if (info.elementType == "string" || info.elementType == "ptr") elemType = VarType::PTR;
        else if (isRecordTypeName(info.elementType)) elemType = VarType::RECORD;

        VarType rhsType = getExpressionType(node.value.get());
        if (elemType == VarType::DOUBLE && rhsType != VarType::DOUBLE && !isFloatReg(value)) {
            std::string f = allocFloatReg();
            emit2("to_float", f, value);
            if (isReg(value) && !isParmReg(value)) freeReg(value);
            value = f;
        } else if (elemType != VarType::DOUBLE && rhsType == VarType::DOUBLE && isFloatReg(value)) {
            std::string i = allocReg();
            emit2("to_int", i, value);
            if (isReg(value) && !isParmReg(value)) freeReg(value);
            value = i;
        }

        if (isRecordTypeName(info.elementType)) {
            throw std::runtime_error("Cannot assign directly to record array element");
        }

        std::string mangled = findMangledArrayName(node.arrayName);
        std::string base = ensurePtrBase(mangled);

        emit4("store", value, base, elementIndex, std::to_string(info.elementSize));

        if (isReg(value) && !isParmReg(value)) freeReg(value);
        if (isReg(index) && !isParmReg(index)) freeReg(index);
        freeReg(elementIndex);
    }


    
    void visit(ArrayAccessNode& node) override {
        ArrayInfo* info = getArrayInfoForArrayAccess(&node);
        if (!info) {
            std::string arrayName = getArrayNameFromBase(node.base.get());
            throw std::runtime_error("Unknown array: " + arrayName);
        }

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
            emitArrayBoundsCheck(idxCopy, info->lowerBound, info->upperBound);
            if (isReg(idxCopy)) freeReg(idxCopy);
        }
    #endif

        std::string elemIndex = allocReg();
        emit2("mov", elemIndex, idx);
        if (info->lowerBound != 0) emit2("sub", elemIndex, std::to_string(info->lowerBound));

        std::string base;
        if (auto var = dynamic_cast<VariableNode*>(node.base.get())) {
            std::string mangledArrayName = findMangledArrayName(var->name);
            base = ensurePtrBase(mangledArrayName);
        } else if (auto field = dynamic_cast<FieldAccessNode*>(node.base.get())) {
            field->recordExpr->accept(*this);
            std::string recPtr = popValue();
            std::string recType = getVarRecordTypeNameFromExpr(field->recordExpr.get());
            auto ofs_sz = getRecordFieldOffsetAndSize(recType, field->fieldName);
            int fieldOffset = ofs_sz.first;

            std::string arrayPtr = allocTempPtr();
            emit2("mov", arrayPtr, recPtr);
            emit2("add", arrayPtr, std::to_string(fieldOffset));
            base = arrayPtr;
        } else if (dynamic_cast<ArrayAccessNode*>(node.base.get())) {
            node.base->accept(*this);
            base = popValue();
        } else {
            throw std::runtime_error("Unsupported array base in access");
        }

        if (info->elementIsArray || isRecordTypeName(info->elementType)) {
            std::string offsetBytes = allocReg();
            emit2("mov", offsetBytes, elemIndex);
            emit2("mul", offsetBytes, std::to_string(info->elementSize));

            std::string elemPtr = allocTempPtr();
            emit2("mov", elemPtr, base);
            emit2("add", elemPtr, offsetBytes);
            pushValue(elemPtr);

            if (isReg(idx) && !isParmReg(idx)) freeReg(idx);
            freeReg(elemIndex);
            freeReg(offsetBytes);
            return;
        }

        VarType elemType = getExpressionType(&node);
        std::string dst;
        if (elemType == VarType::DOUBLE)      dst = allocFloatReg();
        else if (elemType == VarType::PTR ||
                elemType == VarType::STRING) dst = allocTempPtr();
        else                                  dst = allocReg();

        emit4("load", dst, base, elemIndex, std::to_string(info->elementSize));
        pushValue(dst);

        if (isReg(idx) && !isParmReg(idx)) freeReg(idx);
        freeReg(elemIndex);
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
                    case BinaryOpNode::PLUS:     return (isFloatLiteral(L)||isFloatLiteral(R)) ? std::to_string(toD(L)+toD(R)) : std::to_string(toI(L)+toI(R));
                    case BinaryOpNode::MINUS:    return (isFloatLiteral(L)||isFloatLiteral(R)) ? std::to_string(toD(L)-toD(R)) : std::to_string(toI(L)-toI(R));
                    case BinaryOpNode::MULTIPLY: return (isFloatLiteral(L)||isFloatLiteral(R)) ? std::to_string(toD(L)*toD(R)) : std::to_string(toI(L)*toI(R));
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


        struct RecordField { 
            std::string name; 
            std::string typeName; 
            int offset; 
            int size; 
            bool isArray = false; 
            ArrayInfo arrayInfo; 

            
            RecordField() = default;
            RecordField(const RecordField& other) = default;
            RecordField(RecordField&& other) noexcept = default;
            RecordField& operator=(const RecordField& other) = default;
            RecordField& operator=(RecordField&& other) noexcept = default;
        };
        struct RecordTypeInfo { int size = 0; std::vector<RecordField> fields; std::unordered_map<std::string,int> nameToIndex; };
        std::unordered_map<std::string, RecordTypeInfo> recordTypes;
        std::unordered_map<std::string, std::string> varRecordType;

        std::pair<int,int> getRecordFieldOffsetAndSize(const std::string& recType, const std::string& field) {
            std::string normalizedRecType = resolveTypeName(lc(recType));
            std::string normalizedField = lc(field);
            
            auto it = recordTypes.find(normalizedRecType);
            if (it == recordTypes.end()) return {0,8};
            
            auto jt = it->second.nameToIndex.find(normalizedField);
            if (jt == it->second.nameToIndex.end()) return {0,8};
            
            const auto& f = it->second.fields[jt->second];
            return {f.offset, f.size};
        }

        
        std::string getVarRecordTypeNameFromExpr(ASTNode* expr) {
            auto lc = [](std::string s){
                std::transform(s.begin(), s.end(), s.begin(),
                            [](unsigned char c){ return std::tolower(c); });
                return s;
            };
            auto resolveAlias = [&](std::string t){
                t = lc(t);
                std::unordered_set<std::string> seen;
                while (typeAliases.count(t) && !seen.count(t)) {
                    seen.insert(t);
                    t = lc(typeAliases.at(t));
                }
                return t;
            };
            if (auto* v = dynamic_cast<VariableNode*>(expr)) {
                if (auto it = currentParamTypes.find(v->name); it != currentParamTypes.end()) {
                    auto resolved = resolveAlias(it->second);
                    if (recordTypes.count(resolved)) return resolved;
                }
                std::string mangled = findMangledName(v->name);
                if (auto it2 = varRecordType.find(mangled); it2 != varRecordType.end()) {
                    auto resolved = resolveAlias(it2->second);
                    if (recordTypes.count(resolved)) return resolved;
                }
                if (auto it3 = varRecordType.find(v->name); it3 != varRecordType.end()) {
                    auto resolved = resolveAlias(it3->second);
                    if (recordTypes.count(resolved)) return resolved;
                }
                return "";
            }
            if (auto* arrAccess = dynamic_cast<ArrayAccessNode*>(expr)) {
                ArrayInfo* info = getArrayInfoForArrayAccess(arrAccess);
                if (info) {
                    auto elem = resolveAlias(info->elementType);
                    if (recordTypes.count(elem)) return elem;
                }
                return "";
            }

            if (auto* fa = dynamic_cast<FieldAccessNode*>(expr)) {
                std::string owner = getVarRecordTypeNameFromExpr(fa->recordExpr.get());
                owner = resolveTypeName(lc(owner));
                if (owner.empty()) return "";

                auto rit = recordTypes.find(owner);
                if (rit == recordTypes.end()) return "";
               auto idxIt = rit->second.nameToIndex.find(lc(fa->fieldName));
                if (idxIt == rit->second.nameToIndex.end()) return "";

                const auto& f = rit->second.fields[idxIt->second];
                std::string fieldType = resolveTypeName(lc(f.typeName));
                if (recordTypes.count(fieldType)) {
                    return fieldType;   
                }
                return "";              
            }

            return "";
        }


        void visit(RecordTypeNode& /*node*/) override {}
        
        std::string getVarRecordTypeName(const std::string& varName) {
            
            
            if (auto it = currentParamTypes.find(varName); it != currentParamTypes.end()) {
                std::string resolved = resolveTypeName(it->second);
                return resolved;
            }
            
            std::string mangled = findMangledName(varName);
            if (auto it2 = varRecordType.find(mangled); it2 != varRecordType.end()) {
                std::string resolved = resolveTypeName(it2->second);
                return resolved;
            }
            
            if (auto it3 = varRecordType.find(varName); it3 != varRecordType.end()) {
                std::string resolved = resolveTypeName(it3->second);
                return resolved;
            }
            
            
            return "";
        }

        void visit(RecordDeclarationNode& node) override {
            RecordTypeInfo info;
            int offset = 0;

            auto lc = [](std::string s){ 
                std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c){ return std::tolower(c); }); 
                return s;
            };
            
            auto resolveTypeName = [&](std::string t){
                t = lc(t);
                std::unordered_set<std::string> seen;
                while (typeAliases.count(t) && !seen.count(t)) {
                    seen.insert(t);
                    t = lc(typeAliases.at(t));
                }
                return t;
            };

            for (auto& f : node.recordType->fields) {
                auto& fieldDecl = static_cast<VarDeclNode&>(*f);
                bool fieldIsArray = false;
                ArrayInfo arrInfo{};
                std::string fieldTypeName = "unknown";

                if (std::holds_alternative<std::unique_ptr<ASTNode>>(fieldDecl.type)) {
                    auto& typeNode = std::get<std::unique_ptr<ASTNode>>(fieldDecl.type);
                    if (auto* at = dynamic_cast<ArrayTypeNode*>(typeNode.get())) {
                        fieldIsArray = true;
                        arrInfo = buildArrayInfoFromNode(at);
                        fieldTypeName = "array";
                    } else if (dynamic_cast<RecordTypeNode*>(typeNode.get())) {
                        fieldTypeName = "record";
                    } else if (auto* st = dynamic_cast<SimpleTypeNode*>(typeNode.get())) {
                        fieldTypeName = resolveTypeName(st->typeName);
                    }
                } else {
                    fieldTypeName = resolveTypeName(std::get<std::string>(fieldDecl.type));
                }

                for (const auto& rawName : fieldDecl.identifiers) {
                    RecordField rf;
                    rf.name = lc(rawName);
                    rf.offset = offset;
                    rf.isArray = fieldIsArray;
                    
                    if (fieldIsArray) {
                        rf.arrayInfo = arrInfo;
                        rf.typeName = "array";
                        rf.size = rf.arrayInfo.size * rf.arrayInfo.elementSize;
                    } else if (isRecordTypeName(fieldTypeName)) {
                        rf.typeName = fieldTypeName;
                        auto it = recordTypes.find(fieldTypeName);
                        rf.size = (it != recordTypes.end()) ? it->second.size : 8;
                    } else {
                        rf.typeName = fieldTypeName;
                        rf.size = getTypeSizeByName(rf.typeName);
                    }
                    
                    info.nameToIndex[rf.name] = (int)info.fields.size();
                    info.fields.push_back(rf);
                    offset += rf.size;
                    
                    std::cout << "Added field: " << rf.name << " type: " << rf.typeName 
                            << " offset: " << rf.offset << " size: " << rf.size << std::endl;
                }
            }
            
            info.size = offset;
            std::string recordName = lc(node.name);
            recordTypes[recordName] = info;
            
            // Debug output
            std::cout << "Registered record type: " << recordName 
                    << " with size: " << info.size << std::endl;
        }

        void visit(TypeDeclNode& node) override {
            for (auto& typeDecl : node.typeDeclarations) {
                typeDecl->accept(*this);
            }
        }

        void visit(SimpleTypeNode& node) override {

        }
      
        void visit(ArrayTypeDeclarationNode& node) override {
            std::string mangledName = findMangledName(node.name);
            ArrayInfo info = buildArrayInfoFromNode(node.arrayType.get());
            arrayInfo[node.name] = std::move(info);
            int slot = newSlotFor(mangledName);
            varSlot[node.name] = slot;
            varSlot[mangledName] = slot;
            setVarType(node.name, VarType::PTR);
            setSlotType(slot, VarType::PTR);
            updateDataSectionInitialValue(slotVar(slot), "ptr", "null");
            emit3("alloc",
                  slotVar(slot),
                  std::to_string(arrayInfo[mangledName].elementSize),
                  std::to_string(arrayInfo[mangledName].size));
            std::string currentScope = getCurrentScopeName();
            if (currentScope.empty())  globalArrays.push_back(slotVar(slot));
            else                       functionScopedArrays[currentScope].push_back(slotVar(slot));
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

        static inline bool isPtrLike(VarType t) {
            return t == VarType::PTR || t == VarType::RECORD || t == VarType::STRING;
        }

        std::string ensurePtrBase(const std::string& v) {
            auto isNamedPtr = [&](const std::string& s)->bool {
                if (isPtrReg(s)) return true;                      
                if (s.rfind("_tmpptr", 0) == 0) return true;       
                auto it = varSlot.find(s);
                if (it != varSlot.end()) {
                    auto jt = slotToType.find(it->second);
                    if (jt != slotToType.end()) {
                        auto t = jt->second;
                        return t == VarType::PTR || t == VarType::RECORD || t == VarType::STRING;
                    }
                }
                return false;
            };
            if (isNamedPtr(v)) return v;
            std::string p = allocTempPtr();  
            emit2("mov", p, v);
            return p;
        }
        
        std::string getArrayNameFromBase(ASTNode* base) {
            if (auto var = dynamic_cast<VariableNode*>(base)) {
                return var->name;
            }
            if (auto field = dynamic_cast<FieldAccessNode*>(base)) {
                return getArrayNameFromBase(field->recordExpr.get()) + "." + field->fieldName;
            }
            if (auto arr = dynamic_cast<ArrayAccessNode*>(base)) {
                return getArrayNameFromBase(arr->base.get());
            }
            
            throw std::runtime_error("Array base is not a simple variable or field");
        }
        
          ArrayInfo* getArrayInfoForArrayAccess(ArrayAccessNode* arr) {
                if (auto var = dynamic_cast<VariableNode*>(arr->base.get())) {
                    std::string mangled = findMangledName(var->name);
                    auto it = arrayInfo.find(mangled);
                    if (it != arrayInfo.end()) return &it->second;
                    it = arrayInfo.find(var->name);
                    if (it != arrayInfo.end()) return &it->second;
                    return nullptr;
                }
                if (auto field = dynamic_cast<FieldAccessNode*>(arr->base.get())) {
                    std::string recTypeName = getVarRecordTypeNameFromExpr(field->recordExpr.get());
                    recTypeName = resolveTypeName(lc(recTypeName)); 
                    auto recTypeInfoIt = recordTypes.find(recTypeName);
                    if (recTypeInfoIt != recordTypes.end()) {
                        auto& recInfo = recTypeInfoIt->second;
                        auto fieldIndexIt = recInfo.nameToIndex.find(lc(field->fieldName)); 
                        if (fieldIndexIt != recInfo.nameToIndex.end()) {
                            auto& f = recInfo.fields[fieldIndexIt->second];
                            if (f.isArray) return const_cast<ArrayInfo*>(&f.arrayInfo);
                        }
                    }
                }
                if (auto inner = dynamic_cast<ArrayAccessNode*>(arr->base.get())) {
                    ArrayInfo* baseInfo = getArrayInfoForArrayAccess(inner);
                    if (baseInfo && baseInfo->elementIsArray) {
                        return baseInfo->elementArray.get();
                    }
                }
                return nullptr;
        }
    };

  
    std::string mxvmOpt(const std::string &text);

} 
#endif
