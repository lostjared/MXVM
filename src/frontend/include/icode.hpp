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

namespace pascal {

    class CodeGenVisitor;  

    class BuiltinFunctionHandler {
        public:
            virtual ~BuiltinFunctionHandler() = default;
            virtual bool canHandle(const std::string& funcName) const = 0;
            virtual void generate(CodeGenVisitor& visitor, const std::string& funcName, 
                                const std::vector<std::unique_ptr<ASTNode>>& arguments) = 0;
            virtual bool generateWithResult(CodeGenVisitor& visitor, const std::string& funcName,
                                  const std::vector<std::unique_ptr<ASTNode>>& arguments);
        };

    class BuiltinFunctionRegistry {
        private:
            std::vector<std::unique_ptr<BuiltinFunctionHandler>> handlers;  

        public:
            void registerHandler(std::unique_ptr<BuiltinFunctionHandler> handler) {  
                handlers.push_back(std::move(handler));
            }
            
            BuiltinFunctionHandler* findHandler(const std::string& funcName) {
                for(auto& handler : handlers) {
                    if(handler->canHandle(funcName)) {  
                        return handler.get();
                    }
                }
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
        void generate(CodeGenVisitor& visitor, const std::string& funcName, 
                    const std::vector<std::unique_ptr<ASTNode>>& arguments) override;
        bool generateWithResult(CodeGenVisitor& visitor, const std::string& funcName,
                            const std::vector<std::unique_ptr<ASTNode>>& arguments) override;
    };

    class StringFunctionHandler : public BuiltinFunctionHandler {
    
    };

    class SDLFunctionHandler : public BuiltinFunctionHandler {
    public:
        bool canHandle(const std::string& funcName) const override;
        void generate(CodeGenVisitor& visitor, const std::string& funcName, 
                    const std::vector<std::unique_ptr<ASTNode>>& arguments) override;
        bool generateWithResult(CodeGenVisitor& visitor, const std::string& funcName,
                            const std::vector<std::unique_ptr<ASTNode>>& arguments) override;
    };


    class CodeGenVisitor : public ASTVisitor {
    private:
        std::unordered_set<std::string> usedRealConstants;  
        int realConstantCounter = 0;
        std::set<std::string> usedModules;
    
        std::string generateRealConstantName() {
            return "real_const_" + std::to_string(realConstantCounter++);
        }
        
    
        bool isRealNumber(const std::string& str) const {
            return str.find('.') != std::string::npos || 
                   str.find('e') != std::string::npos || 
                   str.find('E') != std::string::npos;
        }
        
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
        
        std::map<std::string, std::pair<std::string, std::string>> constInitialValues;
        const std::vector<std::string> registers = {"rax", "rbx", "rcx", "rdx", "rsi", "rdi", "r8", "r9", "r10", "r11"};
        const std::vector<std::string> ptrRegisters = {"arg0", "arg1", "arg2", "arg3", "arg4", "arg5", "arg6"};
        const std::vector<std::string> floatRegisters = {"xmm0", "xmm1", "xmm2", "xmm3", "xmm4", "xmm5", "xmm6", "xmm7", "xmm8", "xmm9"};
        
        std::vector<bool> regInUse;
        std::vector<bool> floatRegInUse;  
        
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

            if (slot < -1 && slot >= -10) { 
                int regIndex = -2 - slot;
                if (regIndex >= 0 && regIndex < static_cast<int>(registers.size())) {
                    return registers[regIndex];
                }
            }
            if (slot <= -100) { 
                int regIndex = -100 - slot;
                if (regIndex >= 0 && regIndex < static_cast<int>(floatRegisters.size())) {
                    return floatRegisters[regIndex];
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
        
        std::string allocFloatReg() {
            for (size_t i = 0; i < floatRegInUse.size(); ++i) {
                if (!floatRegInUse[i]) {
                    floatRegInUse[i] = true;
                    return floatRegisters[i];
                }
            }
            throw std::runtime_error("No available floating-point registers");
        }
        
        void freeFloatReg(const std::string& reg) {
            for (size_t i = 0; i < floatRegisters.size(); ++i) {
                if (floatRegisters[i] == reg) {
                    floatRegInUse[i] = false;
                    return;
                }
            }
        }
        
        bool isFloatReg(const std::string& name) const {
            return std::find(floatRegisters.begin(), floatRegisters.end(), name) != floatRegisters.end();
        }
        
        bool isPtrReg(const std::string& name) const {
            return std::find(ptrRegisters.begin(), ptrRegisters.end(), name) != ptrRegisters.end();
        }
        
        bool isReg(const std::string& name) const {
            return std::find(registers.begin(), registers.end(), name) != registers.end() ||
                   std::find(floatRegisters.begin(), floatRegisters.end(), name) != floatRegisters.end();
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
            bool isFloatOp = isFloatReg(a) || isFloatReg(b) || 
                             isRealNumber(a) || isRealNumber(b) ||
                             a.find("real_const_") != std::string::npos ||
                             b.find("real_const_") != std::string::npos;
            
            std::string leftOp = a;
            std::string rightOp = b;
            
            if (isReg(leftOp)) {
                emit3(op, leftOp, rightOp);
                pushValue(leftOp); 
                if (isReg(b)) freeReg(b); 
            } else {
                std::string result = isFloatOp ? allocFloatReg() : allocReg();
                emit3("mov", result, leftOp);
                emit3(op, result, rightOp);
                pushValue(result);
                if (isReg(b)) freeReg(b);
            }
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
        
    public:
        friend class IOFunctionHandler;
        friend class MathFunctionHandler;
        friend class StdFunctionHandler;
        friend class SDLFunctionHandler;
        BuiltinFunctionRegistry builtinRegistry;

        CodeGenVisitor()
            : regInUse(registers.size(), false),
              floatRegInUse(floatRegisters.size(), false), 
              currentFunctionName(),
              functionSetReturn(false),
              nextSlot(0), nextTemp(0), labelCounter(0) 
        {
            initializeBuiltins(); 
        }

        virtual ~CodeGenVisitor() = default;

        void initializeBuiltins() {
            builtinRegistry.registerHandler(std::make_unique<IOFunctionHandler>());
            builtinRegistry.registerHandler(std::make_unique<StdFunctionHandler>());
            builtinRegistry.registerHandler(std::make_unique<SDLFunctionHandler>());
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
                for (size_t i = 0; i < floatRegInUse.size(); i++) {
                    floatRegInUse[i] = false;
                }
                
                int paramIndex = 0;
                for (auto &p : pn->parameters) {
                    if (auto param = dynamic_cast<ParameterNode*>(p.get())) {
                        for (auto &id : param->identifiers) {
                            if (paramIndex < 6) {
                                if (!param->type.empty() && param->type == "string") {
                                    int slot = newSlotFor(id);
                                    emit3("mov", slotVar(slot), ptrRegisters[paramIndex]);
                                    setVarType(id, VarType::STRING);
                                } 
                                else if (!param->type.empty() && param->type == "real") {
                                    int slot = newSlotFor(id);
                                    emit3("mov", slotVar(slot), floatRegisters[paramIndex]);
                                    setVarType(id, VarType::DOUBLE);
                                } 
                                else {
                                    // Integer parameter handling
                                    int slot = newSlotFor(id); // Add this line to get explicit slot
                                    varSlot[id] = slot;        // Track the slot
                                    
                                    emit3("mov", slotVar(slot), registers[paramIndex + 1]); // Add this
                                    regInUse[paramIndex + 1] = true;
                                    
                                    if (!param->type.empty()) {
                                        if (param->type == "integer" || param->type == "boolean") {
                                            setVarType(id, VarType::INT);
                                            setSlotType(slot, VarType::INT); // Add this line
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
                for (size_t i = 0; i < floatRegInUse.size(); i++) {
                    floatRegInUse[i] = false;
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
                                if (!param->type.empty() && param->type == "real") {
                                    setVarType(id, VarType::DOUBLE);
                                    recordLocation(id, {ValueLocation::REGISTER, floatRegisters[paramIndex]});
                                } 
                                else {
                                    int slot = newSlotFor(id); 
                                    varSlot[id] = slot;        
                                    
                                    emit3("mov", slotVar(slot), registers[paramIndex + 1]);
                                    regInUse[paramIndex + 1] = true;
                                    
                                    if (!param->type.empty()) {
                                        if (param->type == "integer" || param->type == "boolean") {
                                            setVarType(id, VarType::INT);
                                            setSlotType(slot, VarType::INT); 
                                        }
                                    }
                                }
                                paramIndex++;
                            }
                        }
                    }
                }

                if (fn->block) fn->block->accept(*this);

                if (!functionSetReturn) {
                    VarType funcReturnType = getVarType(fn->name);
                    if (funcReturnType == VarType::STRING) {
                        emit3("mov", "arg0", emptyString());  
                    } else if (funcReturnType == VarType::DOUBLE) {
                        
                        std::string zeroConstName = generateRealConstantName();
                        realConstants[zeroConstName] = "0.0";
                        usedRealConstants.insert(zeroConstName);
                        emit3("mov", "xmm0", zeroConstName);
                    } else {
                        emit3("mov", "rax", "0");  
                    }
                }

                emit("ret");
                currentFunctionName.clear();
                functionSetReturn = false;
            }
           
        }

        void emit_invoke(const std::string& funcName, const std::vector<std::string>& params) {
            std::string instruction = "invoke " + funcName;
            for (const auto& param : params) {
                instruction += ", " + param;
            }
            emit(instruction);
        }

        void writeTo(std::ostream& out) const {
            out << "program " << name << " {\n";
            out << "\tsection module {\n ";
            bool first = true;
            for (const auto& mod : usedModules) {
                if (!first) out << ",\n";
                out << "\t\t" << mod;
                first = false;
            }
            out << "\n\t}\n";
            out << "\tsection data {\n";
            for (const auto& reg : registers) {
                out << "\t\tint " << reg << " = 0\n";
            }
            
            
            for (const auto& floatReg : floatRegisters) {
                out << "\t\tfloat " << floatReg << " = 0.0\n";
            }
            
            for (const auto& p : ptrRegisters) {
                 out << "\t\tptr " << p << " = null\n";
            }
            
            for (const auto& constant : realConstants) {
                out << "\t\tfloat " << constant.first << " = " << constant.second << "\n";
            }
            
            if (needsEmptyString) {
                out << "\t\tstring empty_str = \"\"\n";
            }
            
            for (auto &s : stringLiterals) {
                if (s.first != "empty_str") { 
                    out << "\t\tstring " << s.first << " = " << escapeStringForMxvm(s.second) << "\n";
                }
            }
            
            if (usedStrings.count("fmt_int")) {
                out << "\t\tstring fmt_int = \"%lld \"\n";
            }
            if (usedStrings.count("fmt_str")) {
                out << "\t\tstring fmt_str = \"%s \"\n";
            }
            if (usedStrings.count("fmt_chr")) {
                out << "\t\tstring fmt_chr = \"%c \"\n";
            }
            if (usedStrings.count("fmt_float")) {
                out << "\t\tstring fmt_float = \"%.6f \"\n";
            }
            if (usedStrings.count("newline")) {
                out << "\t\tstring newline = \"\\n\"\n";
            }
            out << "\t\tstring input_buffer, 256\n";

            for (int i = 0; i < nextSlot; ++i) {
                if (!isRegisterSlot(i) && !isTempVar(slotVar(i)) && !isPtrReg(slotVar(i))) {
                    auto it = slotToType.find(i);
                    if (it != slotToType.end()) {
                        std::string varName = slotVar(i);
                        auto constIt = constInitialValues.find(varName);
                        if (constIt != constInitialValues.end()) {
                            std::string typeStr = constIt->second.first;
                            std::string value = constIt->second.second;
                            out << "\t\t" << typeStr << " " << varName << " = " << value << "\n";
                        } else {
                            if (it->second == VarType::STRING) {
                                out << "\t\tstring " << slotVar(i) << " = \"\"\n";
                            } else if (it->second == VarType::PTR) {
                                out << "\t\tptr " << slotVar(i) << " = null\n";
                            } else if (it->second == VarType::CHAR) {
                                out << "\t\tint " << slotVar(i) << " = 0\n"; 
                            } else if (it->second == VarType::DOUBLE) {
                                out << "\t\tfloat " << slotVar(i) << " = 0.0\n";
                            } else {
                                out << "\t\tint " << slotVar(i) << " = 0\n";
                            }
                        }
                    } else {
                        out << "\t\tint " << slotVar(i) << " = 0\n";
                    }
                }
            }
            out << "\t}\n";
            out << "\tsection code {\n";
            out << "\tstart:\n";
            
            for (auto &s : prolog) out << "\t\t" << s << "\n";
            for (auto &s : instructions) {
                if (endsWithColon(s)) out << "\t" << s << "\n";
                else out << "\t\t" << s << "\n";
            }
            
            out << "\t}\n";
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
                            auto init = node.initializers[i].get();
                            if (auto numNode = dynamic_cast<NumberNode*>(init)) {
                                if (numNode->isReal || isRealNumber(numNode->value)) {
                                    std::string constName = generateRealConstantName();
                                    realConstants[constName] = numNode->value;
                                    usedRealConstants.insert(constName);
                                    emit3("mov", slotVar(slot), constName);
                                } else {
                                    std::string constName = generateRealConstantName();
                                    realConstants[constName] = numNode->value + ".0";
                                    usedRealConstants.insert(constName);
                                    emit3("mov", slotVar(slot), constName);
                                }
                            } else {
                                std::string init = eval(node.initializers[i].get());
                                emit3("mov", slotVar(slot), init);
                                if (isReg(init)) freeReg(init);
                            }
                        } else {
                            std::string zeroConstName = generateRealConstantName();
                            realConstants[zeroConstName] = "0.0";
                            usedRealConstants.insert(zeroConstName);
                            emit3("mov", slotVar(slot), zeroConstName);
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
            
            FuncInfo funcInfo;
            funcInfo.returnType = getTypeFromString(node.returnType);
            for (auto& param : node.parameters) {
                if (auto paramNode = dynamic_cast<ParameterNode*>(param.get())) {
                    for (size_t i = 0; i < paramNode->identifiers.size(); i++) {
                        funcInfo.paramTypes.push_back(getTypeFromString(paramNode->type));
                    }
                }
            }
            
            funcSignatures[node.name] = funcInfo;
            deferredFuncs.push_back(&node);
            setVarType(node.name, getTypeFromString(node.returnType));
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
            auto varPtr = dynamic_cast<VariableNode*>(node.variable.get());
            if (!varPtr) return;
            
            std::string varName = varPtr->name;
            
            std::string rhs;
            if (auto numNode = dynamic_cast<NumberNode*>(node.expression.get())) {
                if (numNode->isReal || isRealNumber(numNode->value)) {
                    std::string constName = generateRealConstantName();
                    realConstants[constName] = numNode->value;
                    usedRealConstants.insert(constName);
                    rhs = constName;
                } else {
                    rhs = numNode->value;
                }
            } else {
                rhs = eval(node.expression.get());
            }
            
            if (!currentFunctionName.empty() && varName == currentFunctionName) {
                VarType funcReturnType = getVarType(currentFunctionName);
                if (funcReturnType == VarType::STRING) {
                    emit3("mov", "arg0", rhs);  
                } else if (funcReturnType == VarType::DOUBLE) {
                    emit3("mov", "xmm0", rhs);  
                } else {
                    emit3("mov", "rax", rhs);   
                }
                functionSetReturn = true;
            } else {
                int slot = newSlotFor(varName);
                std::string memLoc = slotVar(slot);
                VarType varType = getVarType(varName);
                
                
                if (varType == VarType::DOUBLE && std::all_of(rhs.begin(), rhs.end(), 
                    [](char c) { return std::isdigit(c) || c == '-'; })) {
                
                    std::string constName = generateRealConstantName();
                    realConstants[constName] = rhs + ".0";
                    usedRealConstants.insert(constName);
                    rhs = constName;
                }
                
                auto it = valueLocations.find(varName);
                if (it != valueLocations.end() && it->second.type == ValueLocation::REGISTER) {
                    freeReg(it->second.location);
                }
                
                emit3("mov", memLoc, rhs);
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

         VarType getTypeFromString(const std::string& typeName) const {
            if (typeName.empty()) {
                return VarType::UNKNOWN;
            }
            
            if (typeName == "integer" || typeName == "boolean") {
                return VarType::INT;
            }
            else if (typeName == "real") {
                return VarType::DOUBLE;
            }
            else if (typeName == "string") {
                return VarType::STRING;
            }
            else if (typeName == "char") {
                return VarType::CHAR;
            }
            else if (typeName == "record") {
                return VarType::RECORD;
            }
            else if (typeName == "pointer" || typeName == "^") {
                return VarType::PTR;
            }
            
            return VarType::UNKNOWN;
        }

        void visit(ProcCallNode& node) override {
            std::string name = node.name;
            
            auto handler = builtinRegistry.findHandler(name);

            if (handler) {
                handler->generate(*this, name, node.arguments);
                return;
            }
            
            
            for (size_t i = 1; i <= 6; i++) {
                regInUse[i] = false;
            }
            for (size_t i = 0; i < floatRegInUse.size(); i++) {
                floatRegInUse[i] = false;
            }
            
            std::vector<std::string> argValues;
            for (auto &arg : node.arguments) {
                argValues.push_back(eval(arg.get()));
            }
            
            
            auto sig_it = funcSignatures.find(name);
            
            for (size_t i = 0; i < argValues.size(); i++) {
                VarType paramType = VarType::INT; 
                
                
                if (sig_it != funcSignatures.end() && i < sig_it->second.paramTypes.size()) {
                    paramType = sig_it->second.paramTypes[i];
                } else if (i < node.arguments.size()) {
                
                    if (auto varNode = dynamic_cast<VariableNode*>(node.arguments[i].get())) {
                        paramType = getVarType(varNode->name);
                    } else if (dynamic_cast<StringNode*>(node.arguments[i].get())) {
                        paramType = VarType::STRING;
                    } else if (auto numNode = dynamic_cast<NumberNode*>(node.arguments[i].get())) {
                        paramType = (numNode->isReal || isRealNumber(numNode->value)) ? VarType::DOUBLE : VarType::INT;
                    }
                }

                if (i < 6) {
                    if (paramType == VarType::STRING) {
                        if (i < ptrRegisters.size()) {
                            emit3("mov", ptrRegisters[i], argValues[i]);
                        }
                    } else if (paramType == VarType::DOUBLE) {
                
                        std::string floatReg = floatRegisters[i];
                        emit3("mov", floatReg, argValues[i]);
                        floatRegInUse[i] = true;
                    } else {
                
                        const std::string dest = registers[i+1];
                        emit3("mov", dest, argValues[i]);
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

            auto handler = builtinRegistry.findHandler(node.name);
            if (handler) {
                bool handled = handler->generateWithResult(*this, node.name, node.arguments);
                if (handled) {
                    return; 
                }
            }

            for (size_t i = 1; i <= 6; i++) {
                regInUse[i] = false;
            }
            for (size_t i = 0; i < floatRegInUse.size(); i++) {
                floatRegInUse[i] = false;
            }
            
            std::vector<std::string> argValues;
            for (auto &arg : node.arguments) {
                argValues.push_back(eval(arg.get()));
            }
            
            auto sig_it = funcSignatures.find(node.name);
            
            for (size_t i = 0; i < argValues.size(); i++) {
                VarType paramType = VarType::INT; // Default
                if (sig_it != funcSignatures.end() && i < sig_it->second.paramTypes.size()) {
                    paramType = sig_it->second.paramTypes[i];
                } else if (i < node.arguments.size()) {
                    if (auto varNode = dynamic_cast<VariableNode*>(node.arguments[i].get())) {
                        paramType = getVarType(varNode->name);
                    } else if (dynamic_cast<StringNode*>(node.arguments[i].get())) {
                        paramType = VarType::STRING;
                    } else if (auto numNode = dynamic_cast<NumberNode*>(node.arguments[i].get())) {
                        paramType = (numNode->isReal || isRealNumber(numNode->value)) ? VarType::DOUBLE : VarType::INT;
                    } else {
                        paramType = getExpressionType(node.arguments[i].get());
                    }
                }

                if (i < 6) {
                    if (paramType == VarType::STRING) {
                        if (i < ptrRegisters.size()) {
                            emit3("mov", ptrRegisters[i], argValues[i]);
                        }
                    } else if (paramType == VarType::DOUBLE) {
                        emit3("mov", floatRegisters[i], argValues[i]);
                        floatRegInUse[i] = true;
                    } else {
                        const std::string dest = registers[i+1];
                        emit3("mov", dest, argValues[i]);
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
            
            emit1("call", funcLabel("FUNC_", node.name));

            
            VarType returnType = VarType::INT;
            if (sig_it != funcSignatures.end()) {
                returnType = sig_it->second.returnType;
            }
            
            std::string result;
            if (returnType == VarType::STRING) {
                result = allocReg();
                emit3("mov", result, "arg0");  
            } else if (returnType == VarType::DOUBLE) {
                result = allocFloatReg();
                emit3("mov", result, "xmm0");
            } else {
                result = allocReg();
                emit3("mov", result, "rax");
            }
            
            pushValue(result);
        }

        void visit(BinaryOpNode& node) override {
            std::string left = eval(node.left.get());
            std::string right = eval(node.right.get());
            VarType leftType = getExpressionType(node.left.get());
            VarType rightType = getExpressionType(node.right.get());
            
            VarType resultType = VarType::INT;
            
            bool needsFloatOp = (leftType == VarType::DOUBLE || rightType == VarType::DOUBLE ||
                                node.operator_ == BinaryOpNode::DIVIDE);
            
            if (needsFloatOp) {
                resultType = VarType::DOUBLE;
                if (isRealNumber(left) && left.find("real_const_") == std::string::npos) {
                    std::string constName = generateRealConstantName();
                    realConstants[constName] = left;
                    usedRealConstants.insert(constName);
                    left = constName;
                }
                
                if (isRealNumber(right) && right.find("real_const_") == std::string::npos) {
                    std::string constName = generateRealConstantName();
                    realConstants[constName] = right;
                    usedRealConstants.insert(constName);
                    right = constName;
                }
                
                
                if (leftType == VarType::INT) {
                    if (std::all_of(left.begin(), left.end(), [](char c) { 
                        return std::isdigit(c) || c == '-'; })) {
                        std::string constName = generateRealConstantName();
                        realConstants[constName] = left + ".0";
                        usedRealConstants.insert(constName);
                        left = constName;
                    } else {
                        std::string newLeft = allocFloatReg();
                        emit2("to_float", newLeft, left);
                        if (isReg(left)) freeReg(left);
                        left = newLeft;
                    }
                } else if (leftType == VarType::DOUBLE && !isFloatReg(left) && 
                           left.find("real_const_") == std::string::npos) {
                    std::string newLeft = allocFloatReg();
                    emit3("mov", newLeft, left);
                    if (isReg(left)) freeReg(left);
                    left = newLeft;
                }
                
                if (rightType == VarType::INT) {
                    if (std::all_of(right.begin(), right.end(), [](char c) { 
                        return std::isdigit(c) || c == '-'; })) {
                        std::string constName = generateRealConstantName();
                        realConstants[constName] = right + ".0";
                        usedRealConstants.insert(constName);
                        right = constName;
                    } else {
                        std::string newRight = allocFloatReg();
                        emit2("to_float", newRight, right);
                        if (isReg(right)) freeReg(right);
                        right = newRight;
                    }
                } else if (rightType == VarType::DOUBLE && !isFloatReg(right) && 
                           right.find("real_const_") == std::string::npos) {
                    std::string newRight = allocFloatReg();
                    emit3("mov", newRight, right);
                    if (isReg(right)) freeReg(right);
                    right = newRight;
                }
            }
            
            switch (node.operator_) {
                case BinaryOpNode::PLUS:
                    pushTri("add", left, right);
                    break;
                    
                case BinaryOpNode::MINUS:
                    pushTri("sub", left, right);
                    break;
                    
                case BinaryOpNode::MULTIPLY:
                    pushTri("mul", left, right);
                    break;
                    
                case BinaryOpNode::DIVIDE:
                    pushTri("div", left, right);
                    break;
                    
                case BinaryOpNode::MOD:
                    if (resultType == VarType::DOUBLE) {
                        std::string result = allocFloatReg(); 
                        emit_invoke("fmod", {left, right});
                        emit1("return", result);
                        pushValue(result);
                        if (isReg(left)) freeReg(left);
                        if (isReg(right)) freeReg(right);
                        return; 
                    } else {
                        pushTri("mod", left, right);
                    }
                    break;
                    
                
                case BinaryOpNode::EQUAL:
                    pushCmpResult(left, right, "je");
                    break;
                    
                case BinaryOpNode::NOT_EQUAL:
                    pushCmpResult(left, right, "jne");
                    break;
                    
                case BinaryOpNode::LESS:
                    pushCmpResult(left, right, "jl");
                    break;
                    
                case BinaryOpNode::LESS_EQUAL:
                    pushCmpResult(left, right, "jle");
                    break;
                    
                case BinaryOpNode::GREATER:
                    pushCmpResult(left, right, "jg");
                    break;
                    
                case BinaryOpNode::GREATER_EQUAL:
                    pushCmpResult(left, right, "jge");
                    break;
                    
                
                case BinaryOpNode::AND:
                    pushLogicalAnd(left, right);
                    break;
                    
                case BinaryOpNode::OR:
                    pushLogicalOr(left, right);
                    break;
                    
                default:
                    throw std::runtime_error("Unknown binary operator");
            }
            
        }

        VarType getExpressionType(ASTNode* node) {
            if (auto numNode = dynamic_cast<NumberNode*>(node)) {
                return numNode->isReal ? VarType::DOUBLE : VarType::INT;
            }
            if (auto varNode = dynamic_cast<VariableNode*>(node)) {
                return getVarType(varNode->name);
            }
            return VarType::UNKNOWN;
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
            if (it != valueLocations.end() && it->second.type == ValueLocation::REGISTER) {
                pushValue(it->second.location);
                return;
            }            
            int slot = newSlotFor(node.name);
            std::string memVar = slotVar(slot);
            std::string reg;
            if (getVarType(node.name) == VarType::DOUBLE) {
                reg = allocFloatReg();
            } else {
                reg = allocReg();
            }
            emit3("mov", reg, memVar);
            pushValue(reg);
        }

        void visit(NumberNode& node) override {
            if (node.isReal || isRealNumber(node.value)) {
                std::string constName = generateRealConstantName();
                usedRealConstants.insert(constName);
                realConstants[constName] = node.value;
                pushValue(constName);
            } else {
                pushValue(node.value);
            }
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
        void visit(ConstDeclNode& node) override {
            for (const auto& assignment : node.assignments) {
                std::string varType;
                std::string literalValue;
                
                if (auto numNode = dynamic_cast<NumberNode*>(assignment->value.get())) {
                    if (numNode->isReal || numNode->value.find('.') != std::string::npos) {
                        varType = "float";
                        literalValue = numNode->value;
                    } else {
                        varType = "int";
                        literalValue = numNode->value;
                    }
                } else if (auto stringNode = dynamic_cast<StringNode*>(assignment->value.get())) {
                    varType = "string";
                    literalValue = "\"" + stringNode->value + "\"";
                } else if (auto boolNode = dynamic_cast<BooleanNode*>(assignment->value.get())) {
                    varType = "int";
                    literalValue = boolNode->value ? "1" : "0";
                } else {
                    std::string valueReg = eval(assignment->value.get());
                    
                    varType = "int"; 
                    int slot = newSlotFor(assignment->identifier);
                    setVarType(assignment->identifier, VarType::INT);
                    setSlotType(slot, VarType::INT);
                    
                    std::string varLocation = slotVar(slot);
                    emit3("mov", varLocation, valueReg);
                    
                    if (isReg(valueReg)) {
                        freeReg(valueReg);
                    }
                    continue;
                }
                
                int slot = newSlotFor(assignment->identifier);
                VarType vType = VarType::INT;
                if (varType == "float") {
                    vType = VarType::DOUBLE;
                } else if (varType == "string") {
                    vType = VarType::STRING;
                }
                
                setVarType(assignment->identifier, vType);
                setSlotType(slot, vType);
                std::string varLocation = slotVar(slot);
                updateDataSectionInitialValue(varLocation, varType, literalValue);
            }
        }

        void updateDataSectionInitialValue(const std::string& varName, const std::string& type, const std::string& value) {
            constInitialValues[varName] = {type, value};
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

        void visit(CaseStmtNode& node) override {
            std::string switchExpr = eval(node.expression.get());
            std::string endLabel = newLabel("CASE_END");
            std::vector<std::string> branchLabels;
            
            
            for (size_t i = 0; i < node.branches.size(); i++) {
                branchLabels.push_back(newLabel("CASE_" + std::to_string(i)));
            }
            
            std::string elseLabel = newLabel("CASE_ELSE");
            
            
            for (size_t i = 0; i < node.branches.size(); i++) {
                auto& branch = node.branches[i];
                
                for (auto& value : branch->values) {
                    std::string caseValue = eval(value.get());
                    emit2("cmp", switchExpr, caseValue);
                    emit1("je", branchLabels[i]);
                    if (isReg(caseValue)) freeReg(caseValue);
                }
            }
            
            
            if (node.elseStatement) {
                emit1("jmp", elseLabel);
            } else {
                emit1("jmp", endLabel);
            }
            
            
            for (size_t i = 0; i < node.branches.size(); i++) {
                emitLabel(branchLabels[i]);
                if (node.branches[i]->statement) {
                    node.branches[i]->statement->accept(*this);
                }
                emit1("jmp", endLabel);
            }
            
            
            if (node.elseStatement) {
                emitLabel(elseLabel);
                node.elseStatement->accept(*this);
            }
            
            emitLabel(endLabel);
            
            if (isReg(switchExpr)) freeReg(switchExpr);
        }

    private:
        std::unordered_map<std::string, std::string> realConstants;
    };

    std::string mxvmOpt(const std::string &text);

} 
#endif