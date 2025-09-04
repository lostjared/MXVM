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
        const std::vector<std::string> floatRegisters = {"xmm0","xmm1","xmm2","xmm3","xmm4","xmm5","xmm6","xmm7","xmm8","xmm9", "xmm10", "xmm11", "xmm12", "xmm13", "xmm14", "xmm15"}; 
        
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
                        if (auto* param = dynamic_cast<ParameterNode*>(p_node.get())) {
                            
                            const std::string normType = resolveTypeName(lc(param->type));
                            const bool isArrayParam = (arrayInfo.find(normType) != arrayInfo.end());
                            
                            VarType declType = getTypeFromString(param->type);
                            std::unordered_map<std::string, bool> declaredFuncs;

                            for (const auto& id : param->identifiers) {
                                VarType useType = declType;
                                std::string incomingReg;

                                if (declType == VarType::DOUBLE) {
                                    if (floatParamIndex < floatRegisters.size())
                                        incomingReg = floatRegisters[floatParamIndex++];
                                } else if (declType == VarType::PTR || declType == VarType::STRING ||
                                        declType == VarType::RECORD || isArrayParam) {
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

                                if (isArrayParam) {
                                    arrayInfo[mangledId] = arrayInfo.at(normType);
                                }

                                currentParamTypes[id] = normType;
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
                auto temp1 = tempPtrByScope[scopeName];
                for (const auto& tp : temp1) {
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
                functionSetReturn = false;

                if (!fn->parameters.empty()) {
                    int intParamIndex = 1;
                    int ptrParamIndex = 0;
                    int floatParamIndex = 0;
                    functionSetReturn = false;

                    for (auto& p_node : fn->parameters) {
                        if (auto* param = dynamic_cast<ParameterNode*>(p_node.get())) {
                            const std::string normType = resolveTypeName(lc(param->type));
                            const bool isArrayParam = (arrayInfo.find(normType) != arrayInfo.end());

                            for (const auto& id : param->identifiers) {
                                VarType paramType = getTypeFromString(param->type);
                                std::string incomingReg;

                                if (paramType == VarType::STRING || paramType == VarType::RECORD || isArrayParam) {
                                    if (static_cast<size_t>(ptrParamIndex) < ptrRegisters.size())
                                        incomingReg = ptrRegisters[ptrParamIndex++];
                                    paramType = VarType::PTR;  
                                } else if (paramType == VarType::DOUBLE) {
                                    if (static_cast<size_t>(floatParamIndex) < floatRegisters.size())
                                        incomingReg = floatRegisters[floatParamIndex++];
                                } else {
                                    if (static_cast<size_t>(intParamIndex) < registers.size())
                                        incomingReg = registers[intParamIndex++];
                                }

                                if (!incomingReg.empty()) {
                                    std::string mangledId = mangleVariableName(id);
                                    int localSlot = newSlotFor(mangledId);
                                    setSlotType(localSlot, paramType);
                                    setVarType(id, paramType);
                                    emit2("mov", slotVar(localSlot), incomingReg);

                                    if (isArrayParam) {
                                        arrayInfo[mangledId] = arrayInfo.at(normType);
                                    }
                                }
                                currentParamTypes[id] = normType;
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

                auto temp1 = tempPtrByScope[scopeName];
                for (const auto& tp : temp1) {
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

       
        void visit(ProgramNode& node) override;
        void visit(BlockNode& node) override;
        void visit(VarDeclNode& node) override;
        void visit(ProcCallNode& node) override;
        void visit(FuncCallNode& node) override;
        void visit(FuncDeclNode& node) override;
        void visit(ProcDeclNode& node) override;
        void visit(TypeAliasNode& node) override;
        void visit(ParameterNode& node) override;
        void visit(CompoundStmtNode& node) override;
        void visit(IfStmtNode& node) override;
        void visit(WhileStmtNode& node) override;
        void visit(ForStmtNode& node) override;
        void visit(BinaryOpNode& node) override;
        void visit(UnaryOpNode& node) override;
        void visit(VariableNode& node) override;
        void visit(NumberNode& node) override;
        void visit(StringNode& node) override;
        void visit(BooleanNode& node) override;
        void visit(EmptyStmtNode& node) override;
        void visit(ConstDeclNode& node) override;
        void visit(RepeatStmtNode& node) override;
        void visit(CaseStmtNode& node) override;
        void visit(ArrayTypeNode& node) override;
        void visit(ArrayDeclarationNode& node) override;
        void visit(FieldAccessNode& node) override;
        void visit(AssignmentNode& node) override;
        void visit(ArrayAssignmentNode& node) override;
        void visit(ArrayAccessNode& node) override;
        void visit(RecordTypeNode& node) override;
        void visit(RecordDeclarationNode& node) override;
        void visit(TypeDeclNode& node) override;
        void visit(SimpleTypeNode& node) override;
        void visit(ArrayTypeDeclarationNode& node) override;
        void visit(ExitNode& node) override;
        void visit(BreakNode& node) override;
        void visit(ContinueNode& node) override;


        
        std::string getTypeString(const VarDeclNode& node) {
            if (std::holds_alternative<std::string>(node.type))
                return std::get<std::string>(node.type);
            if (std::holds_alternative<std::unique_ptr<ASTNode>>(node.type)) {
                return "record";
            }
            return "";
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
        if (arrayInfo.find(base) != arrayInfo.end()) return VarType::PTR; 
        return VarType::UNKNOWN;
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
        
        void updateDataSectionInitialValue(const std::string& varName, const std::string& type, const std::string& value) { constInitialValues[varName] = {type, value}; }

      
    private:
        std::unordered_set<std::string> allocatedPtrs;   

        std::unordered_map<std::string, std::string> realConstants;

        
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


    int getRecordTypeSize(const std::string& typeName) {
        auto it = recordTypes.find(typeName);
        if (it != recordTypes.end()) {
            return it->second.size;
        }
        return 8;  
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

        
        bool isArrayTypeName(const std::string& t) const {
            auto base = resolveTypeName(lc(t));
            return arrayInfo.find(base) != arrayInfo.end();
        }

        
    private:

        std::vector<std::string> loopEndLabels;
        std::vector<std::string> loopContinueLabels;

        
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

      