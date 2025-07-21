#include"mxvm/icode.hpp"
#include"mxvm/parser.hpp"
#include"scanner/exception.hpp"
#include<iomanip>
#include<iostream>
#include<sstream>
#include <regex>

namespace mxvm {

    Program::Program() : pc(0), running(false) {

    }

    Program::~Program() {
        for(auto &i : vars) {
            if(i.second.var_value.ptr_value != nullptr) {
                free(i.second.var_value.ptr_value);
                i.second.var_value.ptr_value = nullptr;
                if(debug_mode) {
                    std::cout << "Released Pointer: " << i.first << "\n";
                }
            }
        }
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
                    exec_je(instr);
                    continue;
                case JNE:
                    exec_jne(instr);
                    continue;
                case JL:
                    exec_jl(instr);
                    continue;
                case JLE:
                    exec_jle(instr);
                    continue;
                case JG:
                    exec_jg(instr);
                    continue;
                case JGE:
                    exec_jge(instr);
                    continue;
                case JZ:
                    exec_jz(instr);
                    continue;
                case JNZ:
                    exec_jnz(instr);
                    continue;
                case JA:
                    exec_ja(instr);
                    continue;
                case JB:
                    exec_jb(instr);
                    continue;
                case LOAD:
                    exec_load(instr);
                    break;
                case STORE:
                    exec_store(instr);
                    break;
                case OR:
                    exec_or(instr);
                    break;
                case AND:
                    exec_and(instr);
                    break;
                case XOR:
                    exec_xor(instr);
                    break;
                case NOT:
                    exec_not(instr);
                    break;
                case MOD:
                    exec_mod(instr);
                case PRINT:
                    exec_print(instr);
                    break;
                case ALLOC:
                    exec_alloc(instr);
                    break;
                case FREE:
                    exec_free(instr);
                    break;
                case EXIT:
                    exec_exit(instr);
                    return;
                case GETLINE:
                    exec_getline(instr);
                    break;
                case PUSH:
                    exec_push(instr);
                    break;
                case POP:
                    exec_pop(instr);
                    break;
                case STACK_LOAD:
                    exec_stack_load(instr);
                    break;
                case STACK_STORE:
                    exec_stack_store(instr);
                    break;
                case CALL:
                    exec_call(instr);
                    continue;
                case RET:
                    exec_ret(instr);
                    break;
                default:
                    throw mx::Exception("Unknown instruction: " + instr.instruction);
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
        Variable *src1 = nullptr, *src2 = nullptr;
        Variable temp1, temp2;

        if (instr.op3.op.empty()) {
            src1 = &dest;
            if (isVariable(instr.op2.op)) {
                src2 = &getVariable(instr.op2.op);
            } else {
                temp2 = createTempVariable(dest.type, instr.op2.op);
                src2 = &temp2;
            }
        } else {
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
        }

        addVariables(dest, *src1, *src2);
    }

    void Program::exec_sub(const Instruction& instr) {
        if (!isVariable(instr.op1.op)) {
            std::cerr << "Error: SUB destination must be a variable, not a constant\n";
            return;
        }
        Variable& dest = getVariable(instr.op1.op);

        Variable *src1 = nullptr, *src2 = nullptr;
        Variable temp1, temp2;

        if (instr.op3.op.empty()) {
            src1 = &dest;
            if (isVariable(instr.op2.op)) {
                src2 = &getVariable(instr.op2.op);
            } else {
                temp2 = createTempVariable(dest.type, instr.op2.op);
                src2 = &temp2;
            }
        } else {
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
        }

        subVariables(dest, *src1, *src2);
    }

    void Program::exec_mul(const Instruction& instr) {
        if (!isVariable(instr.op1.op)) {
            std::cerr << "Error: MUL destination must be a variable, not a constant\n";
            return;
        }
        Variable& dest = getVariable(instr.op1.op);

        Variable *src1 = nullptr, *src2 = nullptr;
        Variable temp1, temp2;

        if (instr.op3.op.empty()) {
            src1 = &dest;
            if (isVariable(instr.op2.op)) {
                src2 = &getVariable(instr.op2.op);
            } else {
                temp2 = createTempVariable(dest.type, instr.op2.op);
                src2 = &temp2;
            }
        } else {
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
        }

        mulVariables(dest, *src1, *src2);
    }

    void Program::exec_div(const Instruction& instr) {
        if (!isVariable(instr.op1.op)) {
            std::cerr << "Error: DIV destination must be a variable, not a constant\n";
            return;
        }
        Variable& dest = getVariable(instr.op1.op);

        Variable *src1 = nullptr, *src2 = nullptr;
        Variable temp1, temp2;

        if (instr.op3.op.empty()) {
            src1 = &dest;
            if (isVariable(instr.op2.op)) {
                src2 = &getVariable(instr.op2.op);
            } else {
                temp2 = createTempVariable(dest.type, instr.op2.op);
                src2 = &temp2;
            }
        } else {
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
        }

        divVariables(dest, *src1, *src2);
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
            throw mx::Exception("Label not found: " + instr.op1.op);
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
            if (isVariable(instr.op1.op)) {
                exit_code = getVariable(instr.op1.op).var_value.int_value;
            } else {
                exit_code = std::stoll(instr.op1.op, nullptr, 0);
            }
        }
        if(mxvm::debug_mode)
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
            int64_t v1 = (src1.type == VarType::VAR_FLOAT) ? static_cast<int64_t>(src1.var_value.float_value) : src1.var_value.int_value;
            int64_t v2 = (src2.type == VarType::VAR_FLOAT) ? static_cast<int64_t>(src2.var_value.float_value) : src2.var_value.int_value;
            dest.var_value.int_value = v1 + v2;
            dest.var_value.type = VarType::VAR_INTEGER;
        } else if (dest.type == VarType::VAR_FLOAT) {
            double v1 = (src1.type == VarType::VAR_INTEGER) ? static_cast<double>(src1.var_value.int_value) : src1.var_value.float_value;
            double v2 = (src2.type == VarType::VAR_INTEGER) ? static_cast<double>(src2.var_value.int_value) : src2.var_value.float_value;
            dest.var_value.float_value = v1 + v2;
            dest.var_value.type = VarType::VAR_FLOAT;
        }
    }

    void Program::subVariables(Variable& dest, Variable& src1, Variable& src2) {
        if (dest.type == VarType::VAR_INTEGER) {
            int64_t v1 = (src1.type == VarType::VAR_FLOAT) ? static_cast<int64_t>(src1.var_value.float_value) : src1.var_value.int_value;
            int64_t v2 = (src2.type == VarType::VAR_FLOAT) ? static_cast<int64_t>(src2.var_value.float_value) : src2.var_value.int_value;
            dest.var_value.int_value = v1 - v2;
            dest.var_value.type = VarType::VAR_INTEGER;
        } else if (dest.type == VarType::VAR_FLOAT) {
            double v1 = (src1.type == VarType::VAR_INTEGER) ? static_cast<double>(src1.var_value.int_value) : src1.var_value.float_value;
            double v2 = (src2.type == VarType::VAR_INTEGER) ? static_cast<double>(src2.var_value.int_value) : src2.var_value.float_value;
            dest.var_value.float_value = v1 - v2;
            dest.var_value.type = VarType::VAR_FLOAT;
        }
    }

    void Program::mulVariables(Variable& dest, Variable& src1, Variable& src2) {
        if (dest.type == VarType::VAR_INTEGER) {
            int64_t v1 = (src1.type == VarType::VAR_FLOAT) ? static_cast<int64_t>(src1.var_value.float_value) : src1.var_value.int_value;
            int64_t v2 = (src2.type == VarType::VAR_FLOAT) ? static_cast<int64_t>(src2.var_value.float_value) : src2.var_value.int_value;
            dest.var_value.int_value = v1 * v2;
            dest.var_value.type = VarType::VAR_INTEGER;
        } else if (dest.type == VarType::VAR_FLOAT) {
            double v1 = (src1.type == VarType::VAR_INTEGER) ? static_cast<double>(src1.var_value.int_value) : src1.var_value.float_value;
            double v2 = (src2.type == VarType::VAR_INTEGER) ? static_cast<double>(src2.var_value.int_value) : src2.var_value.float_value;
            dest.var_value.float_value = v1 * v2;
            dest.var_value.type = VarType::VAR_FLOAT;
        }
    }

    void Program::divVariables(Variable& dest, Variable& src1, Variable& src2) {
        if (dest.type == VarType::VAR_INTEGER) {
            int64_t v1 = (src1.type == VarType::VAR_FLOAT) ? static_cast<int64_t>(src1.var_value.float_value) : src1.var_value.int_value;
            int64_t v2 = (src2.type == VarType::VAR_FLOAT) ? static_cast<int64_t>(src2.var_value.float_value) : src2.var_value.int_value;
            if (v2 != 0) {
                dest.var_value.int_value = v1 / v2;
                dest.var_value.type = VarType::VAR_INTEGER;
            } else {
                std::cerr << "Division by zero error\n";
                stop();
            }
        } else if (dest.type == VarType::VAR_FLOAT) {
            double v1 = (src1.type == VarType::VAR_INTEGER) ? static_cast<double>(src1.var_value.int_value) : src1.var_value.float_value;
            double v2 = (src2.type == VarType::VAR_INTEGER) ? static_cast<double>(src2.var_value.int_value) : src2.var_value.float_value;
            if (v2 != 0.0) {
                dest.var_value.float_value = v1 / v2;
                dest.var_value.type = VarType::VAR_FLOAT;
            } else {
                std::cerr << "Division by zero error\n";
                stop();
            }
        }
    }

    void Program::exec_load(const Instruction& instr) {
        if (!isVariable(instr.op1.op)) {
            throw mx::Exception("LOAD destination must be a variable\n");
            return;
        }
        Variable& dest = getVariable(instr.op1.op);
        void* ptr = nullptr;
        size_t index = 0;
        size_t size = 8;
        if (isVariable(instr.op2.op)) {
            Variable& ptrVar = getVariable(instr.op2.op);
            if (ptrVar.type != VarType::VAR_POINTER || ptrVar.var_value.ptr_value == nullptr) {
                throw mx::Exception("LOAD source must be a valid pointer");
            }
            ptr = ptrVar.var_value.ptr_value;
        } else {
            throw mx::Exception("LOAD source must be a pointer variable");
        }

        if (!instr.op3.op.empty()) {
            if (isVariable(instr.op3.op)) {
                index = static_cast<size_t>(getVariable(instr.op3.op).var_value.int_value);
            } else {
                index = static_cast<size_t>(std::stoll(instr.op3.op, nullptr, 0));
            }
        }

        if (!instr.vop.empty()) {
            const auto& sizeOp = instr.vop[0];
            if (isVariable(sizeOp.op)) {
                size = static_cast<size_t>(getVariable(sizeOp.op).var_value.int_value);
            } else {
                size = static_cast<size_t>(std::stoll(sizeOp.op, nullptr, 0));
            }
        } 

        Variable& ptrVar = getVariable(instr.op2.op);
        if (index >= ptrVar.var_value.ptr_count) {
            throw mx::Exception("LOAD: index out of bounds for " + ptrVar.var_name);
        }
        if (size > ptrVar.var_value.ptr_size) {
            throw mx::Exception("LOAD: size out of bounds for " + ptrVar.var_name);
        }

        char* base = static_cast<char*>(ptr) + index * size;
        switch (dest.type) {
            case VarType::VAR_INTEGER:
                if (size == sizeof(int64_t)) {
                    dest.var_value.int_value = *reinterpret_cast<int64_t*>(base);
                    dest.var_value.type = VarType::VAR_INTEGER;
                } else {
                    throw mx::Exception("LOAD: size mismatch for integer");
                }
                break;
            case VarType::VAR_FLOAT:
                if (size == sizeof(double)) {
                    dest.var_value.float_value = *reinterpret_cast<double*>(base);
                    dest.var_value.type = VarType::VAR_FLOAT;
                } else {
                    throw mx::Exception("LOAD: size mismatch for float");
                }
                break;
            case VarType::VAR_STRING:
                //dest.var_value.str_value = std::string(base, size);
                //dest.var_value.type = VarType::VAR_STRING;
                break;
            default:
                throw mx::Exception("LOAD: unsupported destination type");
        }
    }

    void Program::exec_store(const Instruction& instr) {
        if (!isVariable(instr.op1.op)) {
            throw mx::Exception("STORE destination must be a variable");
            return;
        }
        Variable& dest = getVariable(instr.op1.op);
        void* ptr = nullptr;
        size_t index = 0;
        size_t size = 8;
        if (isVariable(instr.op2.op)) {
            Variable& ptrVar = getVariable(instr.op2.op);
            if (ptrVar.type != VarType::VAR_POINTER || ptrVar.var_value.ptr_value == nullptr) {
                throw mx::Exception("STORE destination must be a valid pointer");
            }
            ptr = ptrVar.var_value.ptr_value;
        } else {
            throw mx::Exception("STORE destination must be a pointer variable");
        }

        if (!instr.op3.op.empty()) {
            if (isVariable(instr.op3.op)) {
                index = static_cast<size_t>(getVariable(instr.op3.op).var_value.int_value);
            } else {
                index = static_cast<size_t>(std::stoll(instr.op3.op, nullptr, 0));
            }
        }

        if (!instr.vop.empty()) {
            const auto& sizeOp = instr.vop[0];
            if (isVariable(sizeOp.op)) {
                size = static_cast<size_t>(getVariable(sizeOp.op).var_value.int_value);
            } else {
                size = static_cast<size_t>(std::stoll(sizeOp.op, nullptr, 0));
            }
        } 
        Variable& ptrVar = getVariable(instr.op2.op);

        if (index >= ptrVar.var_value.ptr_count) {
            throw mx::Exception("STORE: index out of bounds for " + ptrVar.var_name);
        }
        if (size > ptrVar.var_value.ptr_size) {
            throw mx::Exception("STORE: size out of bounds for " + ptrVar.var_name);
        }

        char* base = static_cast<char*>(ptr) + index * size;
        switch (dest.type) {
            case VarType::VAR_INTEGER:
                if (size == sizeof(int64_t)) {
                    *reinterpret_cast<int64_t*>(base) = dest.var_value.int_value;
                } else {
                    throw mx::Exception("STORE: size mismatch for integer");
                }
                break;
            case VarType::VAR_FLOAT:
                if (size == sizeof(double)) {
                    *reinterpret_cast<double*>(base) = dest.var_value.float_value;
                } else {
                    throw mx::Exception("STORE: size mismatch for float");
                }
                break;
            case VarType::VAR_STRING:
                // Optionally implement string storage if needed
                break;
            default:
                throw mx::Exception("STORE: unsupported source type");
        }
    }

    void Program::exec_and(const Instruction& instr) {
        if (!isVariable(instr.op1.op)) {
            throw mx::Exception("AND destination must be a variable");
            return;
        }
        Variable& dest = getVariable(instr.op1.op);
        int64_t v1, v2;
        if (instr.op3.op.empty()) {
            v1 = dest.var_value.int_value;
            v2 = isVariable(instr.op2.op) ? getVariable(instr.op2.op).var_value.int_value : std::stoll(instr.op2.op, nullptr, 0);
        } else {
            v1 = isVariable(instr.op2.op) ? getVariable(instr.op2.op).var_value.int_value : std::stoll(instr.op2.op, nullptr, 0);
            v2 = isVariable(instr.op3.op) ? getVariable(instr.op3.op).var_value.int_value : std::stoll(instr.op3.op, nullptr, 0);
        }
        dest.var_value.int_value = v1 & v2;
        dest.var_value.type = VarType::VAR_INTEGER;
    }

    void Program::exec_or(const Instruction& instr) {
        if (!isVariable(instr.op1.op)) {
            throw mx::Exception("OR destination must be a variable");
            return;
        }
        Variable& dest = getVariable(instr.op1.op);
        int64_t v1, v2;
        if (instr.op3.op.empty()) {
            v1 = dest.var_value.int_value;
            v2 = isVariable(instr.op2.op) ? getVariable(instr.op2.op).var_value.int_value : std::stoll(instr.op2.op, nullptr, 0);
        } else {
            v1 = isVariable(instr.op2.op) ? getVariable(instr.op2.op).var_value.int_value : std::stoll(instr.op2.op, nullptr, 0);
            v2 = isVariable(instr.op3.op) ? getVariable(instr.op3.op).var_value.int_value : std::stoll(instr.op3.op, nullptr, 0);
        }
        dest.var_value.int_value = v1 | v2;
        dest.var_value.type = VarType::VAR_INTEGER;
    }

    void Program::exec_xor(const Instruction& instr) {
        if (!isVariable(instr.op1.op)) {
            throw mx::Exception("XOR destination must be a variable");
            return;
        }
        Variable& dest = getVariable(instr.op1.op);
        int64_t v1, v2;
        if (instr.op3.op.empty()) {
            v1 = dest.var_value.int_value;
            v2 = isVariable(instr.op2.op) ? getVariable(instr.op2.op).var_value.int_value : std::stoll(instr.op2.op, nullptr, 0);
        } else {
            v1 = isVariable(instr.op2.op) ? getVariable(instr.op2.op).var_value.int_value : std::stoll(instr.op2.op, nullptr, 0);
            v2 = isVariable(instr.op3.op) ? getVariable(instr.op3.op).var_value.int_value : std::stoll(instr.op3.op, nullptr, 0);
        }
        dest.var_value.int_value = v1 ^ v2;
        dest.var_value.type = VarType::VAR_INTEGER;
    }

    void Program::exec_alloc(const Instruction& instr) {
        if (!isVariable(instr.op1.op)) {
            throw mx::Exception("ALLOC destination must be a variable");
            return;
        }
        Variable& dest = getVariable(instr.op1.op);
        size_t size = 0;
        size_t count = 1;

        if (!instr.op2.op.empty()) {
            if (isVariable(instr.op2.op)) {
            size = static_cast<size_t>(getVariable(instr.op2.op).var_value.int_value);
            } else {
            size = static_cast<size_t>(std::stoll(instr.op2.op, nullptr, 0));
            }
        }

        if (!instr.op3.op.empty()) {
            if (isVariable(instr.op3.op)) {
            count = static_cast<size_t>(getVariable(instr.op3.op).var_value.int_value);
            } else {
            count = static_cast<size_t>(std::stoll(instr.op3.op, nullptr, 0));
            }
        }

        dest.type = VarType::VAR_POINTER;
        dest.var_value.type = VarType::VAR_POINTER;
        dest.var_value.ptr_value = calloc(count, size);
        if (dest.var_value.ptr_value == nullptr) {
            throw mx::Exception("ALLOC failed: calloc returned nullptr");
        }
        dest.var_value.ptr_size = size;
        dest.var_value.ptr_count = count;
    }
    void Program::exec_free(const Instruction& instr) {
        if (!isVariable(instr.op1.op)) {
            throw mx::Exception("FREE argument must be a variable");
            return;
        }
        Variable& var = getVariable(instr.op1.op);
        if (var.type == VarType::VAR_POINTER && var.var_value.ptr_value != nullptr) {
            free(var.var_value.ptr_value);
            var.var_value.ptr_value = nullptr;
        }
    }

    void Program::exec_not(const Instruction& instr) {
        if (!isVariable(instr.op1.op)) {
            throw mx::Exception("NOT destination must be a variable");
            return;
        }
        Variable& dest = getVariable(instr.op1.op);
        if(dest.var_value.type != VarType::VAR_INTEGER) {
            throw mx::Exception("Error NOT bitwise operation must be on integer value");
        }
        dest.var_value.int_value = !dest.var_value.int_value;
        dest.var_value.type = VarType::VAR_INTEGER;
    }

    void Program::exec_mod(const Instruction& instr) {
        if (!isVariable(instr.op1.op)) {
            std::cerr << "Error: MOD destination must be a variable, not a constant\n";
            return;
        }
        Variable& dest = getVariable(instr.op1.op);

        Variable *src1 = nullptr, *src2 = nullptr;
        Variable temp1, temp2;

        if (instr.op3.op.empty()) {
            src1 = &dest;
            if (isVariable(instr.op2.op)) {
                src2 = &getVariable(instr.op2.op);
            } else {
                temp2 = createTempVariable(dest.type, instr.op2.op);
                src2 = &temp2;
            }
        } else {
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
        }

        if (dest.type == VarType::VAR_INTEGER) {
            int64_t v1 = (src1->type == VarType::VAR_FLOAT) ? static_cast<int64_t>(src1->var_value.float_value) : src1->var_value.int_value;
            int64_t v2 = (src2->type == VarType::VAR_FLOAT) ? static_cast<int64_t>(src2->var_value.float_value) : src2->var_value.int_value;
            if (v2 == 0) {
                std::cerr << "Division by zero error in MOD\n";
                stop();
                return;
            }
            dest.var_value.int_value = v1 % v2;
            dest.var_value.type = VarType::VAR_INTEGER;
        } else {
            std::cerr << "MOD only supports integer types\n";
        }
    }

    void Program::exec_je(const Instruction& instr) {
        if (zero_flag) exec_jmp(instr);
        else
            pc++;
    }

    void Program::exec_jne(const Instruction& instr) {
        if (!zero_flag) exec_jmp(instr);
        else
            pc++;
    }

    void Program::exec_jl(const Instruction& instr) {
        if (less_flag) exec_jmp(instr);
        else
            pc++;
    }

    void Program::exec_jle(const Instruction& instr) {
        if (less_flag || zero_flag) exec_jmp(instr);
        else
            pc++;
    }

    void Program::exec_jg(const Instruction& instr) {
        if (greater_flag) exec_jmp(instr);
        else
            pc++;
    }

    void Program::exec_jge(const Instruction& instr) {
        if (greater_flag || zero_flag) exec_jmp(instr);
        else  
            pc++;
    }

    void Program::exec_jz(const Instruction& instr) {
        if (zero_flag) exec_jmp(instr);
        else
            pc++;
    }

    void Program::exec_jnz(const Instruction& instr) {
        if (!zero_flag) exec_jmp(instr);
        else
            pc++;
    }

    void Program::exec_ja(const Instruction& instr) {
        if (greater_flag) exec_jmp(instr);
        else
            pc++;
    }

    void Program::exec_jb(const Instruction& instr) {
        if (less_flag) exec_jmp(instr);
        else
            pc++;
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
                            if(arg->var_value.type == VarType::VAR_INTEGER)
                                result += std::to_string(arg->var_value.int_value);
                            else if(arg->var_value.type == VarType::VAR_FLOAT)
                                result += std::to_string(static_cast<int64_t>(arg->var_value.float_value));
                            break;
                        case 'f':
                            if(arg->var_value.type == VarType::VAR_INTEGER)
                                result += std::to_string(static_cast<double>(arg->var_value.int_value));
                            else if(arg->var_value.type == VarType::VAR_FLOAT)
                                result += std::to_string(arg->var_value.float_value);
                            break;
                        case 's':
                            result += arg->var_value.str_value;
                            break;
                        case 'b': {
                            std::string bin;
                            uint64_t val = arg->var_value.int_value;
                            if (val == 0) {
                                bin = "0";
                            } else {
                                while (val) {
                                    bin = (val % 2 ? "1" : "0") + bin;
                                    val /= 2;
                                }
                            }
                            result += bin;
                            break;
                        }
                        case 'p':
                            if(arg->var_value.type == VarType::VAR_POINTER)  {
                                if(arg->var_value.ptr_value == nullptr)
                                    result += "null";
                                else {
                                    std::ostringstream oss;
                                    oss << "0x" << std::hex << reinterpret_cast<uintptr_t>(arg->var_value.ptr_value);
                                    result += oss.str();
                                }
                            }
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
        std::cout << result;
    }

    void Program::exec_getline(const Instruction &instr) {
        if (!isVariable(instr.op1.op)) {
            throw mx::Exception("GETLINE destination must be a variable");
            return;
        }
        Variable& dest = getVariable(instr.op1.op);
        std::string input;
        std::getline(std::cin, input);
     
        switch (dest.type) {
            case VarType::VAR_STRING:
                dest.var_value.str_value = input;
                dest.var_value.type = VarType::VAR_STRING;
                break;
            case VarType::VAR_INTEGER:
                try {
                    dest.var_value.int_value = std::stoll(input, nullptr, 0);
                    dest.var_value.type = VarType::VAR_INTEGER;
                } catch (...) {
                    dest.var_value.int_value = 0;
                    dest.var_value.type = VarType::VAR_INTEGER;
                }
                break;
            case VarType::VAR_FLOAT:
                try {
                    dest.var_value.float_value = std::stod(input);
                    dest.var_value.type = VarType::VAR_FLOAT;
                } catch (...) {
                    dest.var_value.float_value = 0.0;
                    dest.var_value.type = VarType::VAR_FLOAT;
                }
                break;
            default:
                throw mx::Exception("GETLINE: unsupported variable type");
        }
    }

    void Program::exec_push(const Instruction &instr) {
        if (!isVariable(instr.op1.op)) {
            try {
                int64_t int_val = std::stoll(instr.op1.op, nullptr, 0);
                stack.push(int_val);
                return;
            } catch (...) {
                throw mx::Exception("PUSH argument must be a variable or integer constant");
                return;
            }
        }
        Variable& var = getVariable(instr.op1.op);
        if (var.var_value.type == VarType::VAR_INTEGER) {
            stack.push(var.var_value.int_value);
        } else if (var.var_value.type == VarType::VAR_POINTER) {
            stack.push(var.var_value.ptr_value);
        } else {
            throw mx::Exception("PUSH only supports integer or pointer types");
        }
    }

    void Program::exec_pop(const Instruction &instr) {
        if (!isVariable(instr.op1.op)) {
            throw mx::Exception("POP destination must be a variable");
            return;
        }
        Variable& var = getVariable(instr.op1.op);

        if (stack.empty()) {
            throw mx::Exception("POP from empty stack");
            return;
        }

        StackValue value = stack.pop();
        if (var.type == VarType::VAR_INTEGER) {
            if (!std::holds_alternative<int64_t>(value)) {
                throw mx::Exception("POP type mismatch: expected integer, pointer found.");
            }
            var.var_value.int_value = std::get<int64_t>(value);
            var.var_value.type = VarType::VAR_INTEGER;
        } else if (var.type == VarType::VAR_POINTER) {
            if (!std::holds_alternative<void*>(value)) {
                throw mx::Exception("POP type mismatch: expected pointer, integer found.");
            }
            var.var_value.ptr_value = std::get<void*>(value);
            var.var_value.type = VarType::VAR_POINTER;
        } else {
            throw mx::Exception("POP only supports integer or pointer variables");
        }
    }

    void Program::exec_stack_load(const Instruction &instr) {
        if (!isVariable(instr.op1.op)) {
            throw mx::Exception("STACK_LOAD destination must be a variable");
            return;
        }
        Variable& dest = getVariable(instr.op1.op);

        size_t index = 0;
        if (!instr.op2.op.empty()) {
            if (isVariable(instr.op2.op)) {
                index = static_cast<size_t>(getVariable(instr.op2.op).var_value.int_value);
            } else {
                index = static_cast<size_t>(std::stoll(instr.op2.op, nullptr, 0));
            }
        }

        if (index >= stack.size()) {
            throw mx::Exception("STACK_LOAD: index out of bounds");
            return;
        }

        StackValue &value = stack[index]; 
        if (dest.type == VarType::VAR_INTEGER) {
            if (!std::holds_alternative<int64_t>(value)) {
                throw mx::Exception("STACK_LOAD type mismatch: expected integer, found pointer");
            }
            dest.var_value.int_value = std::get<int64_t>(value);
            dest.var_value.type = VarType::VAR_INTEGER;
        } else if (dest.type == VarType::VAR_POINTER) {
            if (!std::holds_alternative<void*>(value)) {
                throw mx::Exception("STACK_LOAD type mismatch: expected pointer, found integer");
            }
            dest.var_value.ptr_value = std::get<void*>(value);
            dest.var_value.type = VarType::VAR_POINTER;
        } else {
            throw mx::Exception("STACK_LOAD only supports integer or pointer variables");
        }
    }

    void Program::exec_stack_store(const Instruction &instr) {
        if (!isVariable(instr.op1.op)) {
            throw mx::Exception("STACK_STORE source must be a variable");
            return;
        }
        Variable& src = getVariable(instr.op1.op);

        size_t index = 0;
        if (!instr.op2.op.empty()) {
            if (isVariable(instr.op2.op)) {
                index = static_cast<size_t>(getVariable(instr.op2.op).var_value.int_value);
            } else {
                index = static_cast<size_t>(std::stoll(instr.op2.op, nullptr, 0));
            }
        }

        if (index >= stack.size()) {
            throw mx::Exception("STACK_STORE: index out of bounds");
            return;
        }
        StackValue& value = stack[index]; 
        if (src.type == VarType::VAR_INTEGER) {
            if (!std::holds_alternative<int64_t>(value)) {
                throw mx::Exception("STACK_STORE type mismatch: expected integer slot");
            }
            value = src.var_value.int_value;
        } else if (src.type == VarType::VAR_POINTER) {
            if (!std::holds_alternative<void*>(value)) {
                throw mx::Exception("STACK_STORE type mismatch: expected pointer slot");
            }
            value = src.var_value.ptr_value;
        } else {
            throw mx::Exception("STACK_STORE only supports integer or pointer variables");
        }
    }

    void Program::exec_call(const Instruction &instr) {
        if (instr.op1.op.empty()) {
            throw mx::Exception("CALL requires a label operand");
            return;
        }
        auto it = labels.find(instr.op1.op);
        if (it == labels.end()) {
            throw mx::Exception("CALL: label not found: " + instr.op1.op);
            return;
        }
        stack.push(static_cast<int64_t>(pc + 1));
        pc = it->second;
    }

    void Program::exec_ret(const Instruction &instr) {
        if (stack.empty()) {
            throw mx::Exception("RET: stack is empty, no return address");
            return;
        }
        StackValue value = stack.pop();
        if (!std::holds_alternative<int64_t>(value)) {
            throw mx::Exception("RET: return address on stack is not an integer");
            return;
        }
        pc = static_cast<size_t>(std::get<int64_t>(value)) - 1;
    }

    Variable Program::createTempVariable(VarType type, const std::string& value) {
        Variable temp;
        temp.type = type;
        temp.var_name = "";

        if (type == VarType::VAR_INTEGER) {
            if (value.empty()) throw mx::Exception("empty integer constant: " + value);
            temp.var_value.int_value = std::stoll(value, nullptr, 0);
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
            var.var_value.int_value = std::stoll(value, nullptr, 0);
            var.var_value.type = VarType::VAR_INTEGER;
        } else if (var.type == VarType::VAR_FLOAT) {
            var.var_value.float_value = std::stod(value);
            var.var_value.type = VarType::VAR_FLOAT;
        } else if (var.type == VarType::VAR_STRING) {
            var.var_value.str_value = value;
            var.var_value.type = VarType::VAR_STRING;
        }
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
                        if(var.second.var_value.ptr_value == nullptr)
                            out << "null";
                        else {
                            out << std::setw(20) << std::hex << "0x" 
                            << reinterpret_cast<uintptr_t>(var.second.var_value.ptr_value) << std::dec;
                        }
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

    void Program::post(std::ostream &out) {
        if(stack.empty()) {
                out << "Stack aligned.\n";
        } else {
            out << "Stack unaligned. {\n";
            for(size_t i = 0; i < stack.size(); ++i) {
                out << "\tAddress: [" << i << "] = ";
                const StackValue& value = stack[i];
                if (std::holds_alternative<int64_t>(value)) {
                    out << std::get<int64_t>(value);
                } else if (std::holds_alternative<void*>(value)) {
                    void* ptr = std::get<void*>(value);
                    if (ptr == nullptr) {
                        out << "null";
                    } else {
                        out << "0x" << std::hex << reinterpret_cast<uintptr_t>(ptr) << std::dec;
                    }
                } else {
                    out << "(unknown)";
                }
                out << "\n";
            }
            out << "}\n";
        }
    }
}