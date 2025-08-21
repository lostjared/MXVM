#include "icode.hpp"

namespace pascal {

        bool BuiltinFunctionHandler::generateWithResult(CodeGenVisitor& visitor, const std::string& funcName, const std::vector<std::unique_ptr<ASTNode>>& arguments) { return false; }

        bool IOFunctionHandler::canHandle(const std::string& funcName) const {
            return funcName == "writeln" || funcName == "write" || funcName == "readln";
        }

        void IOFunctionHandler::generate(CodeGenVisitor& visitor, const std::string& funcName, 
                        const std::vector<std::unique_ptr<ASTNode>>& arguments) {
            if (funcName == "writeln" || funcName == "write") {
                for(auto &arg : arguments) {
                    ASTNode *a = arg.get();
                    
                    if (auto numNode = dynamic_cast<NumberNode*>(a)) {
                        if (numNode->isReal || visitor.isRealNumber(numNode->value)) {
                            visitor.usedStrings.insert("fmt_float");
                            std::string v = visitor.eval(a);
                            visitor.emit2("print", "fmt_float", v);
                            if (visitor.isReg(v)) visitor.freeReg(v);
                        } else {
                            visitor.usedStrings.insert("fmt_int");
                            std::string v = visitor.eval(a);
                            visitor.emit2("print", "fmt_int", v);
                            if (visitor.isReg(v)) visitor.freeReg(v);
                        }
                    }
                    else if (auto strNode = dynamic_cast<StringNode*>(a)) {
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
                        auto varType = visitor.getVarType(varNode->name);
                        if (varType == CodeGenVisitor::VarType::CHAR) {
                            visitor.usedStrings.insert("fmt_chr");
                            std::string v = visitor.eval(a);
                            visitor.emit2("print", "fmt_chr", v);
                            if (visitor.isReg(v)) visitor.freeReg(v);
                        } 
                        else if (varType == CodeGenVisitor::VarType::STRING || visitor.isStringVar(varNode->name)) {
                            visitor.usedStrings.insert("fmt_str");
                            std::string v = visitor.eval(a);
                            visitor.emit2("print", "fmt_str", v);
                            if (visitor.isReg(v)) visitor.freeReg(v);
                        }
                        else if (varType == CodeGenVisitor::VarType::DOUBLE) {
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
                        std::string v = visitor.eval(a);
                        if (visitor.isFloatReg(v) || v.find("real_const_") != std::string::npos) {
                            visitor.usedStrings.insert("fmt_float");
                            visitor.emit2("print", "fmt_float", v);
                        } else {
                            CodeGenVisitor::VarType exprType = visitor.getExpressionType(a);
                            if (exprType == CodeGenVisitor::VarType::DOUBLE) {
                                visitor.usedStrings.insert("fmt_float");
                                visitor.emit2("print", "fmt_float", v);
                            } else {
                                visitor.usedStrings.insert("fmt_int");
                                visitor.emit2("print", "fmt_int", v);
                            }
                        }
                        
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
                        
                        if (varType == CodeGenVisitor::VarType::DOUBLE) {
                            visitor.emit2("to_float", memLoc, "input_buffer");
                        }
                        else if (varType == CodeGenVisitor::VarType::INT) {
                            visitor.emit2("to_int", memLoc, "input_buffer");
                        }
                        
                        visitor.recordLocation(varName, {CodeGenVisitor::ValueLocation::MEMORY, memLoc});
                    } 
                    else {
                        throw std::runtime_error("readln argument must be a variable");
                    }
                }
            }
        }

    bool MathFunctionHandler::canHandle(const std::string& funcName) const {
    return funcName == "abs" || funcName == "sqrt" || 
           funcName == "sin" || funcName == "cos" || 
           funcName == "random" || funcName == "round";
}

    void MathFunctionHandler::generate(CodeGenVisitor& visitor, const std::string& funcName,
                const std::vector<std::unique_ptr<ASTNode>>& arguments) {

        throw std::runtime_error("Math functions must return a value");
    }

        bool MathFunctionHandler::generateWithResult(CodeGenVisitor& visitor, const std::string& funcName,
                    const std::vector<std::unique_ptr<ASTNode>>& arguments) {
    if (arguments.empty() && funcName != "random") {
        throw std::runtime_error("Math function requires at least one argument");
    }
    
    if (funcName == "abs") {
        if (arguments.size() != 1) {
            throw std::runtime_error("abs function requires exactly 1 argument");
        }
        std::string arg = visitor.eval(arguments[0].get());
        visitor.emit_invoke("abs", {arg});
        if (visitor.isReg(arg)) visitor.freeReg(arg);
    }
    else if (funcName == "sqrt") {
        if (arguments.size() != 1) {
            throw std::runtime_error("sqrt function requires exactly 1 argument");
        }
        std::string arg = visitor.eval(arguments[0].get());
        visitor.emit_invoke("sqrt", {arg});
        if (visitor.isReg(arg)) visitor.freeReg(arg);
    }
    else if (funcName == "sin") {
        if (arguments.size() != 1) {
            throw std::runtime_error("sin function requires exactly 1 argument");
        }
        std::string arg = visitor.eval(arguments[0].get());
        visitor.emit_invoke("sin", {arg});
        if (visitor.isReg(arg)) visitor.freeReg(arg);
    }
    else if (funcName == "cos") {
        if (arguments.size() != 1) {
            throw std::runtime_error("cos function requires exactly 1 argument");
        }
        std::string arg = visitor.eval(arguments[0].get());
        visitor.emit_invoke("cos", {arg});
        if (visitor.isReg(arg)) visitor.freeReg(arg);
    }
    else if (funcName == "random") {
        if (arguments.empty()) {
            visitor.emit_invoke("random", {});
        } else if (arguments.size() == 1) {
            std::string maxVal = visitor.eval(arguments[0].get());
            visitor.emit_invoke("random_max", {maxVal});
            if (visitor.isReg(maxVal)) visitor.freeReg(maxVal);
        } else if (arguments.size() == 2) {
            std::string minVal = visitor.eval(arguments[0].get());
            std::string maxVal = visitor.eval(arguments[1].get());
            visitor.emit_invoke("random_range", {minVal, maxVal});
            if (visitor.isReg(minVal)) visitor.freeReg(minVal);
            if (visitor.isReg(maxVal)) visitor.freeReg(maxVal);
        } else {
            throw std::runtime_error("random function accepts 0, 1, or 2 arguments");
        }
    }
    else if (funcName == "round") {
        if (arguments.size() != 1) {
            throw std::runtime_error("round function requires exactly 1 argument");
        }
        std::string arg = visitor.eval(arguments[0].get());
        visitor.emit_invoke("round", {arg});
        if (visitor.isReg(arg)) visitor.freeReg(arg);
    }
    else if (funcName == "power" || funcName == "pow") {
        if (arguments.size() != 2) {
            throw std::runtime_error("power function requires exactly 2 arguments");
        }
        std::string base = visitor.eval(arguments[0].get());
        std::string exponent = visitor.eval(arguments[1].get());
        visitor.emit_invoke("power", {base, exponent});
        if (visitor.isReg(base)) visitor.freeReg(base);
        if (visitor.isReg(exponent)) visitor.freeReg(exponent);
    }
    else if (funcName == "min") {
        if (arguments.size() != 2) {
            throw std::runtime_error("min function requires exactly 2 arguments");
        }
        std::string arg1 = visitor.eval(arguments[0].get());
        std::string arg2 = visitor.eval(arguments[1].get());
        visitor.emit_invoke("min", {arg1, arg2});
        if (visitor.isReg(arg1)) visitor.freeReg(arg1);
        if (visitor.isReg(arg2)) visitor.freeReg(arg2);
    }
    else if (funcName == "max") {
        if (arguments.size() != 2) {
            throw std::runtime_error("max function requires exactly 2 arguments");
        }
        std::string arg1 = visitor.eval(arguments[0].get());
        std::string arg2 = visitor.eval(arguments[1].get());
        visitor.emit_invoke("max", {arg1, arg2});
        if (visitor.isReg(arg1)) visitor.freeReg(arg1);
        if (visitor.isReg(arg2)) visitor.freeReg(arg2);
    }
    
    
    std::string result = visitor.allocFloatReg();  
    visitor.emit1("return", result);
    visitor.pushValue(result);
    
    return true;
}

}