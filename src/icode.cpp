#include"mxvm/icode.hpp"
#include<iomanip>
#include<iostream>
#include<sstream>

namespace mxvm {

    Program::Program() : pc(0), running(false) {

    }
    
    void Program::add_instruction(const Instruction &i) {
        inc.push_back(i);
    }
    
    void Program::add_label(const std::string &name, uint64_t address) {
        labels[name] = address;
    }

    void Program::add_variable(const std::string &name, const Variable &v) {
        vars[name] = v;
    }

    void Program::stop() {
        running = false;
    }

    void Program::exec() {
        if (inc.empty()) {
            std::cerr << "No instructions to execute\n";
            return;
        }
        
        pc = 0;
        running = true;
        
        while (running && pc < inc.size()) {
            const Instruction& instr = inc[pc];
            
            switch (instr.instruction) {
                case MOV:
                    exec_mov(instr);
                    break;
                case ADD:
                    exec_add(instr);
                    break;
                case SUB:
                    exec_sub(instr);
                    break;
                case MUL:
                    exec_mul(instr);
                    break;
                case DIV:
                    exec_div(instr);
                    break;
                case CMP:
                    exec_cmp(instr);
                    break;
                case JMP:
                    exec_jmp(instr);
                    continue; 
                case JE:
                    if (zero_flag) {
                        exec_jmp(instr);
                        continue;
                    }
                    break;
                case JNE:
                    if (!zero_flag) {
                        exec_jmp(instr);
                        continue;
                    }
                    break;
                case JL:
                    if (less_flag) {
                        exec_jmp(instr);
                        continue;
                    }
                    break;
                case JG:
                    if (greater_flag) {
                        exec_jmp(instr);
                        continue;
                    }
                    break;
                case PRINT:
                    exec_print(instr);
                    break;
                case EXIT:
                    exec_exit(instr);
                    return;
                default:
                    std::cerr << "Unknown instruction: " << instr.instruction << std::endl;
                    break;
            }
            
            pc++;
        }
    }

    void Program::exec_mov(const Instruction& instr) {
        if (!isVariable(instr.op1.op)) {
            std::cerr << "Error: MOV destination must be a variable, not a constant\n";
            return;
        }
        
        Variable& dest = getVariable(instr.op1.op);
        
        if (isVariable(instr.op2.op)) {
            Variable& src = getVariable(instr.op2.op);
            dest.var_value = src.var_value;
        } else {
            setVariableFromConstant(dest, instr.op2.op);
        }
    }

    void Program::exec_add(const Instruction& instr) {
        if (!isVariable(instr.op1.op)) {
            std::cerr << "Error: ADD destination must be a variable, not a constant\n";
            return;
        }
        
        Variable& dest = getVariable(instr.op1.op);
        
        if (instr.op3.op.empty()) {
            if (isVariable(instr.op2.op)) {
                Variable& src = getVariable(instr.op2.op);
                addVariables(dest, dest, src);
            } else {
                Variable temp = createTempVariable(dest.type, instr.op2.op);
                addVariables(dest, dest, temp);
            }
        } else {
            Variable* src1 = nullptr;
            Variable* src2 = nullptr;
            Variable temp1, temp2;
            
            if (isVariable(instr.op2.op)) {
                src1 = &getVariable(instr.op2.op);
            } else {
                temp1 = createTempVariable(dest.type, instr.op2.op);
                src1 = &temp1;
            }
            
            if (isVariable(instr.op3.op)) {
                src2 = &getVariable(instr.op3.op);
            } else {
                temp2 = createTempVariable(dest.type, instr.op3.op);
                src2 = &temp2;
            }
            
            addVariables(dest, *src1, *src2);
        }
    }

    void Program::exec_sub(const Instruction& instr) {
        if (!isVariable(instr.op1.op)) {
            std::cerr << "Error: SUB destination must be a variable, not a constant\n";
            return;
        }
        
        Variable& dest = getVariable(instr.op1.op);
        Variable* src1 = nullptr;
        Variable* src2 = nullptr;
        Variable temp1, temp2;
        
        if (isVariable(instr.op2.op)) {
            src1 = &getVariable(instr.op2.op);
        } else {
            temp1 = createTempVariable(dest.type, instr.op2.op);
            src1 = &temp1;
        }
        
        if (isVariable(instr.op3.op)) {
            src2 = &getVariable(instr.op3.op);
        } else {
            temp2 = createTempVariable(dest.type, instr.op3.op);
            src2 = &temp2;
        }
        
        if (dest.type == VarType::VAR_INTEGER) {
            dest.var_value.int_value = src1->var_value.int_value - src2->var_value.int_value;
            dest.var_value.type = VarType::VAR_INTEGER;
        } else if (dest.type == VarType::VAR_FLOAT) {
            dest.var_value.float_value = src1->var_value.float_value - src2->var_value.float_value;
            dest.var_value.type = VarType::VAR_FLOAT;
        }
    }

    void Program::exec_mul(const Instruction& instr) {
        if (!isVariable(instr.op1.op)) {
            std::cerr << "Error: MUL destination must be a variable, not a constant\n";
            return;
        }
        
        Variable& dest = getVariable(instr.op1.op);
        Variable* src1 = nullptr;
        Variable* src2 = nullptr;
        Variable temp1, temp2;
        
        if (isVariable(instr.op2.op)) {
            src1 = &getVariable(instr.op2.op);
        } else {
            temp1 = createTempVariable(dest.type, instr.op2.op);
            src1 = &temp1;
        }
        
        if (isVariable(instr.op3.op)) {
            src2 = &getVariable(instr.op3.op);
        } else {
            temp2 = createTempVariable(dest.type, instr.op3.op);
            src2 = &temp2;
        }
        
        if (dest.type == VarType::VAR_INTEGER) {
            dest.var_value.int_value = src1->var_value.int_value * src2->var_value.int_value;
            dest.var_value.type = VarType::VAR_INTEGER;
        } else if (dest.type == VarType::VAR_FLOAT) {
            dest.var_value.float_value = src1->var_value.float_value * src2->var_value.float_value;
            dest.var_value.type = VarType::VAR_FLOAT;
        }
    }

    void Program::exec_div(const Instruction& instr) {
        if (!isVariable(instr.op1.op)) {
            std::cerr << "Error: DIV destination must be a variable, not a constant\n";
            return;
        }
        
        Variable& dest = getVariable(instr.op1.op);
        Variable* src1 = nullptr;
        Variable* src2 = nullptr;
        Variable temp1, temp2;
        
        if (isVariable(instr.op2.op)) {
            src1 = &getVariable(instr.op2.op);
        } else {
            temp1 = createTempVariable(dest.type, instr.op2.op);
            src1 = &temp1;
        }
        
        if (isVariable(instr.op3.op)) {
            src2 = &getVariable(instr.op3.op);
        } else {
            temp2 = createTempVariable(dest.type, instr.op3.op);
            src2 = &temp2;
        }
        
        if (dest.type == VarType::VAR_INTEGER) {
            if (src2->var_value.int_value != 0) {
                dest.var_value.int_value = src1->var_value.int_value / src2->var_value.int_value;
                dest.var_value.type = VarType::VAR_INTEGER;
            } else {
                std::cerr << "Division by zero error\n";
                stop();
            }
        } else if (dest.type == VarType::VAR_FLOAT) {
            if (src2->var_value.float_value != 0.0) {
                dest.var_value.float_value = src1->var_value.float_value / src2->var_value.float_value;
                dest.var_value.type = VarType::VAR_FLOAT;
            } else {
                std::cerr << "Division by zero error\n";
                stop();
            }
        }
    }

    void Program::exec_cmp(const Instruction& instr) {
        Variable* var1 = nullptr;
        Variable* var2 = nullptr;
        Variable temp1, temp2;
        
        if (isVariable(instr.op1.op)) {
            var1 = &getVariable(instr.op1.op);
        } else {
            temp1 = createTempVariable(VarType::VAR_INTEGER, instr.op1.op);
            var1 = &temp1;
        }
        
        if (isVariable(instr.op2.op)) {
            var2 = &getVariable(instr.op2.op);
        } else {
            temp2 = createTempVariable(var1->type, instr.op2.op);
            var2 = &temp2;
        }
        
        // Reset flags
        zero_flag = false;
        less_flag = false;
        greater_flag = false;
        
        if (var1->var_value.type == VarType::VAR_INTEGER && var2->var_value.type == VarType::VAR_INTEGER) {
            int64_t val1 = var1->var_value.int_value;
            int64_t val2 = var2->var_value.int_value;
            
            if (val1 == val2) zero_flag = true;
            else if (val1 < val2) less_flag = true;
            else greater_flag = true;
        } else if (var1->var_value.type == VarType::VAR_FLOAT && var2->var_value.type == VarType::VAR_FLOAT) {
            double val1 = var1->var_value.float_value;
            double val2 = var2->var_value.float_value;
            
            if (val1 == val2) zero_flag = true;
            else if (val1 < val2) less_flag = true;
            else greater_flag = true;
        }
    }

    void Program::exec_jmp(const Instruction& instr) {
        auto it = labels.find(instr.op1.op);
        if (it != labels.end()) {
            pc = it->second;
        } else {
            std::cerr << "Label not found: " << instr.op1.op << std::endl;
        }
    }

    void Program::exec_print(const Instruction& instr) {
        if (isVariable(instr.op1.op)) {
            Variable& fmt = getVariable(instr.op1.op);
            if (fmt.type == VarType::VAR_STRING) {
                std::string format = fmt.var_value.str_value;
                std::vector<Variable*> args;
                if (!instr.op2.op.empty() && isVariable(instr.op2.op)) {
                    args.push_back(&getVariable(instr.op2.op));
                }
                if (!instr.op3.op.empty() && isVariable(instr.op3.op)) {
                    args.push_back(&getVariable(instr.op3.op));
                }
                for (const auto& vop : instr.vop) {
                    if (isVariable(vop.op)) {
                        args.push_back(&getVariable(vop.op));
                    }
                }
                printFormatted(format, args);
            }
        }
    }

    void Program::exec_exit(const Instruction& instr) {
        int exit_code = 0;
        if (!instr.op1.op.empty()) {
            exit_code = std::stoi(instr.op1.op);
        }
        std::cout << "Program exited with code: " << exit_code << std::endl;
        stop();
    }

    Variable& Program::getVariable(const std::string& name) {
        auto it = vars.find(name);
        if (it != vars.end()) {
            return it->second;
        }
        throw std::runtime_error("Variable not found: " + name);
    }

    bool Program::isVariable(const std::string& name) {
        return vars.find(name) != vars.end();
    }

    void Program::setVariableFromString(Variable& var, const std::string& value) {
        if (var.type == VarType::VAR_INTEGER) {
            if (value.starts_with("0x") || value.starts_with("0X")) {
                var.var_value.int_value = std::stoll(value, nullptr, 16);
            } else {
                var.var_value.int_value = std::stoll(value);
            }
            var.var_value.type = VarType::VAR_INTEGER;
        } else if (var.type == VarType::VAR_FLOAT) {
            var.var_value.float_value = std::stod(value);
            var.var_value.type = VarType::VAR_FLOAT;
        } else if (var.type == VarType::VAR_STRING) {
            var.var_value.str_value = value;
            var.var_value.type = VarType::VAR_STRING;
        }
    }

    void Program::addVariables(Variable& dest, Variable& src1, Variable& src2) {
        if (dest.type == VarType::VAR_INTEGER) {
            dest.var_value.int_value = src1.var_value.int_value + src2.var_value.int_value;
            dest.var_value.type = VarType::VAR_INTEGER;
        } else if (dest.type == VarType::VAR_FLOAT) {
            dest.var_value.float_value = src1.var_value.float_value + src2.var_value.float_value;
            dest.var_value.type = VarType::VAR_FLOAT;
        }
    }

    void Program::printFormatted(const std::string& format, const std::vector<Variable*>& args) {
        size_t argIndex = 0;
        std::string result;
        
        for (size_t i = 0; i < format.length(); ++i) {
            if (format[i] == '%' && i + 1 < format.length()) {
                char specifier = format[i + 1];
                if (argIndex < args.size()) {
                    Variable* arg = args[argIndex++];
                    switch (specifier) {
                        case 'd':
                            result += std::to_string(arg->var_value.int_value);
                            break;
                        case 'f':
                            result += std::to_string(arg->var_value.float_value);
                            break;
                        case 's':
                            result += arg->var_value.str_value;
                            break;
                        default:
                            result += format[i];
                            result += format[i + 1];
                            break;
                    }
                    i++; 
                } else {
                    result += format[i];
                }
            } else {
                result += format[i];
            }
        }
        
        std::cout << result << std::endl;
    }

    void Program::print(std::ostream &out) {
        out << "=== VARIABLES ===\n";
        if (vars.empty()) {
            out << "  (no variables)\n";
        } else {
            out << std::left << std::setw(15) << "Name" 
                << std::setw(12) << "Type" 
                << std::setw(20) << "Value" << "\n";
            out << std::string(47, '-') << "\n";
            
            for (const auto &var : vars) {
                out << std::setw(15) << var.first;
                
                std::string typeStr;
                switch (var.second.type) {
                    case VarType::VAR_INTEGER: typeStr = "int"; break;
                    case VarType::VAR_FLOAT: typeStr = "float"; break;
                    case VarType::VAR_STRING: typeStr = "string"; break;
                    case VarType::VAR_POINTER: typeStr = "ptr"; break;
                    case VarType::VAR_LABEL: typeStr = "label"; break;
                    case VarType::VAR_ARRAY: typeStr = "array"; break;
                    default: typeStr = "unknown"; break;
                }
                out << std::setw(12) << typeStr;
                
                switch (var.second.var_value.type) {
                    case VarType::VAR_INTEGER:
                        out << std::setw(20) << var.second.var_value.int_value;
                        break;
                    case VarType::VAR_FLOAT:
                        out << std::setw(20) << std::fixed << std::setprecision(2) 
                            << var.second.var_value.float_value;
                        break;
                    case VarType::VAR_STRING:
                        out << std::setw(20) << ("\"" + var.second.var_value.str_value + "\"");
                        break;
                    case VarType::VAR_POINTER:
                        out << std::setw(20) << std::hex << "0x" 
                            << reinterpret_cast<uintptr_t>(var.second.var_value.ptr_value) << std::dec;
                        break;
                    case VarType::VAR_LABEL:
                        out << std::setw(20) << var.second.var_value.label_value;
                        break;
                    default:
                        out << std::setw(20) << "(uninitialized)";
                        break;
                }
                out << "\n";
            }
        }
        out << "\n";

        out << "=== LABELS ===\n";
        if (labels.empty()) {
            out << "  (no labels)\n";
        } else {
            out << std::left << std::setw(20) << "Label" 
                << std::setw(10) << "Address" << "\n";
            out << std::string(30, '-') << "\n";
            
            for (const auto &label : labels) {
                out << std::setw(20) << label.first 
                    << std::setw(10) << label.second << "\n";
            }
        }
        out << "\n";

        out << "=== INSTRUCTIONS ===\n";
        if (inc.empty()) {
            out << "  (no instructions)\n";
        } else {
            out << std::left << std::setw(5) << "Addr" 
                << std::setw(12) << "Instruction" 
                << std::setw(15) << "Operand1" 
                << std::setw(15) << "Operand2" 
                << std::setw(15) << "Operand3"
                << std::setw(20) << "Extra Operands" << "\n";
            out << std::string(82, '-') << "\n";
            
            for (size_t i = 0; i < inc.size(); ++i) {
                const auto &instr = inc[i];
                
                
                out << std::setw(5) << i;
                
                
                if (instr.instruction >= 0 && instr.instruction < static_cast<int>(IncType.size())) {
                    out << std::setw(12) << IncType[instr.instruction];
                } else {
                    out << std::setw(12) << "UNKNOWN";
                }
                
                out << std::setw(15) << (instr.op1.op.empty() ? "-" : instr.op1.op);
                out << std::setw(15) << (instr.op2.op.empty() ? "-" : instr.op2.op);
                out << std::setw(15) << (instr.op3.op.empty() ? "-" : instr.op3.op);
                
                
                if (!instr.vop.empty()) {
                    std::string extraOps;
                    for (size_t j = 0; j < instr.vop.size(); ++j) {
                        if (j > 0) extraOps += ", ";
                        extraOps += instr.vop[j].op;
                    }
                    out << std::setw(20) << extraOps;
                } else {
                    out << std::setw(20) << "-";
                }
                
                out << "\n";
            }
        }
        out << "\n";
    }

    Variable Program::createTempVariable(VarType type, const std::string& value) {
        Variable temp;
        temp.type = type;
        temp.var_name = ""; 

        if (type == VarType::VAR_INTEGER) {
            if (value.starts_with("0x") || value.starts_with("0X")) {
                temp.var_value.int_value = std::stoll(value, nullptr, 16);
            } else {
                temp.var_value.int_value = std::stoll(value);
            }
            temp.var_value.type = VarType::VAR_INTEGER;
        } else if (type == VarType::VAR_FLOAT) {
            temp.var_value.float_value = std::stod(value);
            temp.var_value.type = VarType::VAR_FLOAT;
        } else if (type == VarType::VAR_STRING) {
            temp.var_value.str_value = value;
            temp.var_value.type = VarType::VAR_STRING;
        }
        return temp;
    }

    void Program::setVariableFromConstant(Variable& var, const std::string& value) {
        if (var.type == VarType::VAR_INTEGER) {
            if (value.starts_with("0x") || value.starts_with("0X")) {
                var.var_value.int_value = std::stoll(value, nullptr, 16);
            } else {
                var.var_value.int_value = std::stoll(value);
            }
            var.var_value.type = VarType::VAR_INTEGER;
        } else if (var.type == VarType::VAR_FLOAT) {
            var.var_value.float_value = std::stod(value);
            var.var_value.type = VarType::VAR_FLOAT;
        } else if (var.type == VarType::VAR_STRING) {
            var.var_value.str_value = value;
            var.var_value.type = VarType::VAR_STRING;
        }
    }
}