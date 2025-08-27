#include"mxvm/icode.hpp"
#include<iostream>
#include<iomanip>

namespace mxvm {

    void Program::stop() {
        running = false;
    }

    void Program::flatten_inc(Program *root, Instruction &i) {
        if (root != this) {
            auto qualifyVar = [&](Operand &op) {
                if (op.op.empty()) return;
                if (op.type != OperandType::OP_VARIABLE) return;
                if (!op.object.empty()) return; 
                if (op.op.find('.') != std::string::npos) return; 
                
                std::string qualified = this->name + "." + op.op;
                if (vars.find(qualified) != vars.end()) {
                    op.object = this->name;
                    op.label = op.op;
                    op.op = qualified;
                }
            };
            auto qualifyLabel = [&](Operand &op) {
                if (op.op.empty()) return;
                if (op.op.find('.') != std::string::npos) return; 
                if (labels.find(op.op) != labels.end()) {
                    op.object = this->name;
                    op.label = op.op;
                    op.op = this->name + "." + op.op;
                }
            };
            Instruction ci = i; 
            qualifyVar(ci.op1);
            qualifyVar(ci.op2);
            qualifyVar(ci.op3);
            for (auto &vo : ci.vop) qualifyVar(vo);
            switch (ci.instruction) {
                case CALL: case JMP: case JE: case JNE: case JL: case JLE:
                case JG: case JGE: case JZ: case JNZ: case JA: case JB:
                    qualifyLabel(ci.op1);
                    break;
                default:
                    break;
            }
            root->add_instruction(ci);
            return;
        }
        root->add_instruction(i);
    }

    void Program::flatten_label(Program *root, int64_t offset, const std::string &label, bool func) {

        if (root != this) {
            root->add_label(this->name + "." + label, offset, func);
            return;
        }
        root->add_label(label, offset, func);
    }

    void Program::flatten_external(Program *root, const std::string &e, RuntimeFunction &r) {
        if(root->external_functions.find(e) == root->external_functions.end()) {
            root->external_functions[e] = r;
        }
    }

    void Program::flatten(Program *program) {   
        if(program != this) {
            int64_t size = program->inc.size();
            for(auto &i : inc) {
                flatten_inc(program, i);
            }
            for(auto &lbl : labels) {
                flatten_label(program, lbl.second.first+size, lbl.first, lbl.second.second);
            }
            for(auto &e : external_functions) {
                flatten_external(program, e.first, e.second);
            }
        }

        for (auto &obj : objects) {
            if(obj.get() != program)
                obj->flatten(program);
        }
    }

    int Program::exec() {

        this->add_standard();

        if (inc.empty()) {
            std::cerr << "No instructions to execute\n";
            return EXIT_FAILURE;
        }
        pc = 0;
        running = true;
   
        while (running && pc < inc.size()) {
            const Instruction& instr = inc.at(pc);
            if(mxvm::instruct_mode)
                std::cout << instr << "\n";

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
                case FCMP:
                    exec_fcmp(instr);
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
                case JAE:
                    exec_jae(instr);
                    continue;
                case JBE:
                    exec_jbe(instr);
                    continue;
                case JC:
                    exec_jc(instr);
                    continue;
                case JNC:
                    exec_jnc(instr);
                    continue;
                case JP:
                    exec_jp(instr);
                    continue;
                case JNP:
                    exec_jnp(instr);
                    continue;
                case JO:
                    exec_jo(instr);
                    continue;
                case JNO:
                    exec_jno(instr);
                    continue;
                case JS:
                    exec_js(instr);
                    continue;
                case JNS:
                    exec_jns(instr);
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
                    break;
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
                    return getExitCode();
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
                case STACK_SUB:
                    exec_stack_sub(instr);
                    break;
                case CALL:
                    exec_call(instr);
                    continue;
                case RET:
                    exec_ret(instr);
                    break;
                case STRING_PRINT:
                    exec_string_print(instr);
                    break;
                case DONE:
                    exec_done(instr);
                    break;
                case TO_INT:
                    exec_to_int(instr);
                    break;
                case TO_FLOAT:
                    exec_to_float(instr);
                    break;
                case INVOKE:
                    exec_invoke(instr);
                    break;
                case RETURN:
                    exec_return(instr);
                    break;
                case NEG:
                    exec_neg(instr);
                    break;
                default:
                    throw mx::Exception("Unknown instruction: " + std::to_string(instr.instruction));
                    break;
            }
            pc++;
        }
        return getExitCode();
    }

    void Program::exec_mov(const Instruction& instr) {
        if (!isVariable(instr.op1.op)) {
            std::cerr << "Error: MOV destination: " + instr.op1.op + "  must be a variable, not a constant\n";
            return;
        }
        Variable& dest = getVariable(instr.op1.op);       
        dest.var_value.owns = false;
        if (isVariable(instr.op2.op)) {
            Variable& src = getVariable(instr.op2.op);
            dest.var_value = src.var_value; 
            dest.type = src.type;
            if (dest.type != VarType::VAR_POINTER) {
                dest.type = src.type;
            }
            if (dest.type == VarType::VAR_POINTER) {
                dest.var_value.owns = false;
            }
        } else {
            setVariableFromConstant(dest, instr.op2.op);
        }
    }

    void Program::exec_add(const Instruction& instr) {
        if (!isVariable(instr.op1.op)) {
            std::cerr << "Error: ADD destination: " + instr.op1.op + " must be a variable, not a constant\n";
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
            temp2 = createTempVariable(VarType::VAR_INTEGER, instr.op2.op);
            var2 = &temp2;
        }
        zero_flag = false;
        less_flag = false;
        greater_flag = false;
        if ((var1->type == VarType::VAR_INTEGER || var1->type == VarType::VAR_BYTE) &&
            (var2->type == VarType::VAR_INTEGER || var2->type == VarType::VAR_BYTE)) {
            int64_t val1 = var1->var_value.int_value;
            int64_t val2 = var2->var_value.int_value;
            if (val1 == val2) zero_flag = true;
            else if (val1 < val2) less_flag = true;
            else greater_flag = true;
        } else if (var1->type == VarType::VAR_FLOAT && var2->type == VarType::VAR_FLOAT) {
            double val1 = var1->var_value.float_value;
            double val2 = var2->var_value.float_value;
            if (val1 == val2) zero_flag = true;
            else if (val1 < val2) less_flag = true;
            else greater_flag = true;
        } else if (var1->type == VarType::VAR_FLOAT && (var2->type == VarType::VAR_INTEGER || var2->type == VarType::VAR_BYTE)) {
            double val1 = var1->var_value.float_value;
            double val2 = static_cast<double>(var2->var_value.int_value);
            if (val1 == val2) zero_flag = true;
            else if (val1 < val2) less_flag = true;
            else greater_flag = true;
        } else if ((var1->type == VarType::VAR_INTEGER || var1->type == VarType::VAR_BYTE) && var2->type == VarType::VAR_FLOAT) {
            double val1 = static_cast<double>(var1->var_value.int_value);
            double val2 = var2->var_value.float_value;
            if (val1 == val2) zero_flag = true;
            else if (val1 < val2) less_flag = true;
            else greater_flag = true;
        } else if (var1->type == VarType::VAR_POINTER && var2->type == VarType::VAR_POINTER) {
            uintptr_t val1 = reinterpret_cast<uintptr_t>(var1->var_value.ptr_value);
            uintptr_t val2 = reinterpret_cast<uintptr_t>(var2->var_value.ptr_value);
            if (val1 == val2) zero_flag = true;
            else if (val1 < val2) less_flag = true;
            else greater_flag = true;
        } else if (var1->type == VarType::VAR_POINTER && (var2->type == VarType::VAR_INTEGER || var2->type == VarType::VAR_BYTE)) {
            uintptr_t val1 = reinterpret_cast<uintptr_t>(var1->var_value.ptr_value);
            uint64_t val2 = var2->var_value.int_value;
            if (val1 == val2) zero_flag = true;
            else if (val1 < val2) less_flag = true;
            else greater_flag = true;
        } else if ((var1->type == VarType::VAR_INTEGER || var1->type == VarType::VAR_BYTE) && var2->type == VarType::VAR_POINTER) {
            uint64_t val1 = var1->var_value.int_value;
            uintptr_t val2 = reinterpret_cast<uintptr_t>(var2->var_value.ptr_value);
            if (val1 == val2) zero_flag = true;
            else if (val1 < val2) less_flag = true;
            else greater_flag = true;
        } else {
            throw mx::Exception("cmp: unsupported type combination: " +
               var1->toString() + " vs " + var2->toString());
        }
    }

    void Program::exec_jmp(const Instruction& instr) {
        auto it = labels.find(instr.op1.op);
        if (it != labels.end()) {
            pc = it->second.first;
        } else {
            throw mx::Exception("Label not found: " + instr.op1.op);
        }
    }

    void Program::exec_print(const Instruction& instr) {
        std::string format;
        std::vector<Variable> tempArgs;
        std::vector<Variable*> args;
       if (isVariable(instr.op1.op)) {
            Variable& fmt = getVariable(instr.op1.op);
            if (fmt.type != VarType::VAR_STRING)
                throw mx::Exception("PRINT format must be a string variable");
            format = fmt.var_value.str_value;
        } else {
            format = instr.op1.op;
        }
        auto getArgVariable = [&](const Operand& op, VarType type = VarType::VAR_INTEGER) -> Variable* {
            if (isVariable(op.op)) {
                return &getVariable(op.op);
            } else {
                if(op.type == OperandType::OP_VARIABLE) {
                    throw mx::Exception("Instruction variable not defined: " + op.op);
                }
                tempArgs.push_back(createTempVariable(type, op.op));
                return &tempArgs.back();
            }
        };
        if (!instr.op2.op.empty()) {
            args.push_back(getArgVariable(instr.op2));
        }
        if (!instr.op3.op.empty()) {
            args.push_back(getArgVariable(instr.op3));
        }
        for (const auto& vop : instr.vop) {
            if (!vop.op.empty()) {
                args.push_back(getArgVariable(vop));
            }
        }
        printFormatted(format, args);
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
        exitCode = exit_code;
        stop();
    }

    Variable& Program::getVariable(const std::string& n) {
        auto rax_pos = n.find("%");
        if(rax_pos != std::string::npos) {
            auto dot = n.find(".");
            if(dot != std::string::npos) {
                std::string n_ = n.substr(dot+1);
                auto e = vars.find(n_);
                if(e != vars.end())
                    return e->second;
            } else {
                auto e = vars.find(n);
                if(e != vars.end())
                    return e->second;
            }
        }
        auto pos = n.find(".");
        if(pos == std::string::npos) {
            auto it = vars.find(name + "." + n);
            if (it != vars.end()) {
                return it->second;
            }
        }
        else {
            auto it = vars.find(n);
            if(it != vars.end())
                return it->second;
        }

        if(Program::base != nullptr) {
            for(auto &obj : Base::base->object_map) {
                if(pos == std::string::npos) {
                    auto it = obj.second->vars.find(name + "." + n);
                    if(it != obj.second->vars.end())
                        return it->second;
                } else {
                    auto it = obj.second->vars.find(n);
                    if(it != obj.second->vars.end())
                        return it->second;                
                }
            }
        }
        throw mx::Exception("Variable not found: " + name + "." + n);
    }

    bool Program::isVariable(const std::string& n) {

        auto rax_pos = n.find("%");

        if(rax_pos != std::string::npos) {
            return true;
        }


        if(Program::base != nullptr) {
            for(auto &obj : Base::base->object_map) {
                auto it = obj.second->vars.find(n);
                if(it != obj.second->vars.end()) {
                    return true;
                }

                it = obj.second->vars.find(name + "." + n);
                if(it != obj.second->vars.end())
                    return true;
            }
        }

        if(vars.find(name + "." + n) != vars.end())
            return true;

        if(vars.find(n) != vars.end())
            return true;

        return false;
    }

    bool Program::validateNames(Validator& v) {
        for(auto &variable : v.var_names) {
            auto it = vars.find(variable.second->getTokenValue()); 
            if(it == vars.end()) {
                for(auto &o : objects) {
                    if(o->name == variable.first && !o->isVariable(variable.second->getTokenValue())) {
                        throw mx::Exception("Syntax Error: Argument variable not defined: Object: " + 
                            variable.first + " variable: " + 
                            variable.second->getTokenValue() + 
                            " at line " + std::to_string(variable.second->getLine())); 
                    }
                }
            }}
        return true;
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
            } else {
                
                dest.var_value.int_value = 0;
            }
            dest.var_value.type = VarType::VAR_INTEGER;
        } else if (dest.type == VarType::VAR_FLOAT) {
            double v1 = (src1.type == VarType::VAR_INTEGER) ? static_cast<double>(src1.var_value.int_value) : src1.var_value.float_value;
            double v2 = (src2.type == VarType::VAR_INTEGER) ? static_cast<double>(src2.var_value.int_value) : src2.var_value.float_value;
            if (v2 != 0.0) {
                dest.var_value.float_value = v1 / v2;
            } else {
                dest.var_value.float_value = 0.0;
            }
            dest.var_value.type = VarType::VAR_FLOAT;
        }
    }

    void Program::exec_load(const Instruction& instr) {
        if (!isVariable(instr.op1.op)) {
            throw mx::Exception("LOAD destination must be a variable");
        }
        Variable& dest = getVariable(instr.op1.op);
        void* ptr = nullptr;
        size_t index = 0;
        size_t stride = 8; 

        if (isVariable(instr.op2.op)) {
            Variable& ptrVar = getVariable(instr.op2.op);
            if (ptrVar.type != VarType::VAR_POINTER && ptrVar.type != VarType::VAR_STRING) {
                throw mx::Exception("LOAD source must be a valid pointer/string buffer");
            }
            if(ptrVar.type == VarType::VAR_POINTER)
                ptr = ptrVar.var_value.ptr_value;
            else
                ptr = (void*)ptrVar.var_value.str_value.data();
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

        if (!instr.vop.empty() && !instr.vop[0].op.empty()) {
            const auto& sizeOp = instr.vop[0];
            if (isVariable(sizeOp.op)) {
                stride = static_cast<size_t>(getVariable(sizeOp.op).var_value.int_value);
            } else {
                stride = static_cast<size_t>(std::stoll(sizeOp.op, nullptr, 0));
            }
        } 

        char* base = static_cast<char*>(ptr) + index * stride;
        switch (dest.type) {
            case VarType::VAR_INTEGER:
            case VarType::VAR_BYTE:
                dest.var_value.int_value = *reinterpret_cast<int64_t*>(base);
                dest.var_value.type = dest.type;
                break;
            case VarType::VAR_FLOAT:
                dest.var_value.float_value = *reinterpret_cast<double*>(base);
                dest.var_value.type = VarType::VAR_FLOAT;
                break;
            case VarType::VAR_POINTER:
                dest.var_value.ptr_value = *reinterpret_cast<void**>(base);
                dest.var_value.type = VarType::VAR_POINTER;
                break;
            case VarType::VAR_STRING: {
                void* str_ptr = *reinterpret_cast<void**>(base);
                if (str_ptr) {      
                    dest.var_value.str_value = std::string(static_cast<char*>(str_ptr));
                } else {
                    dest.var_value.str_value = "";
                }
                dest.var_value.type = VarType::VAR_STRING;
                break;
            }
            default:
                throw mx::Exception("LOAD: unsupported destination type");
        }
    }

    
    void Program::exec_store(const Instruction& instr) {
        void* ptr = nullptr;
        
        if (!isVariable(instr.op2.op)) {
            throw mx::Exception("STORE destination must be a variable or register");
        }
        
        Variable& ptrVar = getVariable(instr.op2.op);
        
        if (ptrVar.type == VarType::VAR_POINTER) {
            if (ptrVar.var_value.ptr_value == nullptr) {
                throw mx::Exception("STORE destination pointer is null: " + ptrVar.var_name);
            }
            ptr = ptrVar.var_value.ptr_value;
        } else if (ptrVar.type == VarType::VAR_INTEGER) {
            if (ptrVar.var_value.int_value == 0) {
                throw mx::Exception("STORE destination contains null pointer: " + ptrVar.var_name);
            }
            ptr = reinterpret_cast<void*>(static_cast<uintptr_t>(ptrVar.var_value.int_value));
        } else {
            throw mx::Exception("STORE destination must be a pointer or integer containing a pointer address: " + ptrVar.var_name + " (type: " + std::to_string(static_cast<int>(ptrVar.type)) + ")");
        }

        size_t index = 0;
        if (!instr.op3.op.empty()) {
            if (isVariable(instr.op3.op)) {
                index = static_cast<size_t>(getVariable(instr.op3.op).var_value.int_value);
            } else {
                index = static_cast<size_t>(std::stoll(instr.op3.op, nullptr, 0));
            }
        }

        size_t stride = 8;
        if (!instr.vop.empty() && !instr.vop[0].op.empty()) {
            const auto& sizeOp = instr.vop[0];
            if (isVariable(sizeOp.op)) {
                stride = static_cast<size_t>(getVariable(sizeOp.op).var_value.int_value);
            } else {
                stride = static_cast<size_t>(std::stoll(sizeOp.op, nullptr, 0));
            }
        }

        char* base = static_cast<char*>(ptr) + index * stride;
        
        if (instr.op1.type == OperandType::OP_CONSTANT) {
            *reinterpret_cast<int64_t*>(base) = std::stoll(instr.op1.op, nullptr, 0);
        } else {
            Variable& src = getVariable(instr.op1.op);
            switch (src.type) {
                case VarType::VAR_INTEGER:
                case VarType::VAR_BYTE:
                    *reinterpret_cast<int64_t*>(base) = src.var_value.int_value;
                    break;
                case VarType::VAR_FLOAT:
                    *reinterpret_cast<double*>(base) = src.var_value.float_value;
                    break;
                case VarType::VAR_POINTER:
                    *reinterpret_cast<void**>(base) = src.var_value.ptr_value;
                    break;
                case VarType::VAR_STRING:
                    *reinterpret_cast<const char**>(base) = src.var_value.str_value.c_str();
                    break;
                default:
                    throw mx::Exception("STORE: unsupported source type");
            }
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
        int64_t size = 0;
        int64_t count = 1;

        if (!instr.op2.op.empty()) {
            if (isVariable(instr.op2.op)) {
                size = getVariable(instr.op2.op).var_value.int_value;
            } else {
                size = std::stoll(instr.op2.op, nullptr, 0);
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
        dest.var_value.owns = true;
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
        dest.var_value.int_value = (dest.var_value.int_value == 0) ? 1 : 0;
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

        if (dest.type == VarType::VAR_INTEGER || dest.type == VarType::VAR_BYTE) {
            int64_t v1 = (src1->type == VarType::VAR_FLOAT) ? static_cast<int64_t>(src1->var_value.float_value) : src1->var_value.int_value;
            int64_t v2 = (src2->type == VarType::VAR_FLOAT) ? static_cast<int64_t>(src2->var_value.float_value) : src2->var_value.int_value;
            if (v2 == 0) {
                dest.var_value.int_value = 0; 
            } else {
                dest.var_value.int_value = v1 % v2;
            }
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

    void Program::exec_string_print(const Instruction &instr) {
        if (!isVariable(instr.op1.op)) {
            throw mx::Exception("STRING_PRINT destination must be a variable");
            return;
        }
        Variable& dest = getVariable(instr.op1.op);

        if (dest.type != VarType::VAR_STRING) {
            throw mx::Exception("STRING_PRINT destination must be a string variable");
            return;
        }

        std::string format;
        std::vector<Variable> tempArgs;      
        std::vector<Variable*> args;         
        if (isVariable(instr.op2.op)) {
            Variable& fmtVar = getVariable(instr.op2.op);
            if (fmtVar.type != VarType::VAR_STRING)
                throw mx::Exception("STRING_PRINT format must be a string variable");
            format = fmtVar.var_value.str_value;
        } else {
            format = instr.op2.op;
        }
        
        auto getArgVariable = [&](const Operand& op, VarType type = VarType::VAR_INTEGER) -> Variable* {
            if (isVariable(op.op)) {
                return &getVariable(op.op);
            } else {
                if(op.type == OperandType::OP_VARIABLE) {
                    throw mx::Exception("string_print instruction variable not defined: " + op.op);
                }
                tempArgs.push_back(createTempVariable(type, op.op));
                return &tempArgs.back();
            }
        };
        
        if (!instr.op3.op.empty()) {
            args.push_back(getArgVariable(instr.op3));
        }
        for (const auto& vop : instr.vop) {
            if (!vop.op.empty()) {
                args.push_back(getArgVariable(vop));
            }
        }

        std::ostringstream oss;
        size_t argIndex = 0;
        const char* fmt = format.c_str();
        char buffer[4096];

        for (size_t i = 0; i < format.length(); ++i) {
            if (fmt[i] == '%' && i + 1 < format.length()) {
                size_t start = i;
                size_t j = i + 1;
                while (j < format.length() && 
                    (fmt[j] == '-' || fmt[j] == '+' || fmt[j] == ' ' || fmt[j] == '#' || fmt[j] == '0' ||
                        (fmt[j] >= '0' && fmt[j] <= '9') || fmt[j] == '.' ||
                        fmt[j] == 'l' || fmt[j] == 'h' || fmt[j] == 'z' || fmt[j] == 'j' || fmt[j] == 't')) {
                    ++j;
                }
                if (j < format.length() && fmt[j] == '%') { 
                    oss << '%';
                    i = j;
                    continue;
                }
                if (j >= format.length()) break;
                if (!std::isalpha(fmt[j])) {
                    oss << fmt[i];
                    continue;
                }
                std::string spec(fmt + start, fmt + j + 1);
                if (argIndex < args.size()) {
                    Variable* arg = args[argIndex++];
                    if (arg->type == VarType::VAR_INTEGER) {
                        std::snprintf(buffer, sizeof(buffer), spec.c_str(), arg->var_value.int_value);
                    } else if (arg->type == VarType::VAR_POINTER || arg->var_value.type == VarType::VAR_EXTERN) {
                        if (spec.find("%s") != std::string::npos) {
                            std::snprintf(buffer, sizeof(buffer), spec.c_str(), static_cast<char*>(arg->var_value.ptr_value));
                        } else {
                            std::snprintf(buffer, sizeof(buffer), spec.c_str(), arg->var_value.ptr_value);
                        }
                    } else if (arg->type == VarType::VAR_FLOAT) {
                        std::snprintf(buffer, sizeof(buffer), spec.c_str(), arg->var_value.float_value);
                    } else if (arg->type == VarType::VAR_STRING) {
                        std::snprintf(buffer, sizeof(buffer), spec.c_str(), arg->var_value.str_value.c_str());
                    } else if (arg->type == VarType::VAR_BYTE) {
                        std::snprintf(buffer, sizeof(buffer), spec.c_str(), arg->var_value.int_value);
                    } else {
                        std::snprintf(buffer, sizeof(buffer), "%s", "(unsupported)");
                    }
                    oss << buffer;
                } else {
                    oss << spec; 
                }
                i = j;
            } else {
                oss << fmt[i];
            }
        }
        
        dest.var_value.str_value = oss.str();
        dest.var_value.type = VarType::VAR_STRING;
    }
    
    std::string Program::printFormatted(const std::string& format, const std::vector<Variable*>& args, bool output) {
        std::ostringstream oss;
        size_t argIndex = 0;
        const char* fmt = format.c_str();
        char buffer[4096];
        for (size_t i = 0; i < format.length(); ++i) {
            if (fmt[i] == '%' && i+1 < format.length()) {
                size_t start = i;
                size_t j = i + 1;
                while (j < format.length() && 
                    (fmt[j] == '-' || fmt[j] == '+' || fmt[j] == ' ' || fmt[j] == '#' || fmt[j] == '0' ||
                        (fmt[j] >= '0' && fmt[j] <= '9') || fmt[j] == '.' ||
                        fmt[j] == 'l' || fmt[j] == 'h' || fmt[j] == 'z' || fmt[j] == 'j' || fmt[j] == 't')) {
                    ++j;
                }
                if (j < format.length() && fmt[j] == '%') { 
                    oss << '%';
                    i = j;
                    continue;
                }
                if (j >= format.length()) break;
                if (!std::isalpha(fmt[j])) {
                    oss << fmt[i];
                    continue;
                }
                std::string spec(fmt + start, fmt + j + 1);
                if (argIndex < args.size()) {

                    Variable* arg = args[argIndex++];
                    if (arg->type == VarType::VAR_INTEGER) {
                        std::snprintf(buffer, sizeof(buffer), spec.c_str(), arg->var_value.int_value);
                    } else if (arg->type == VarType::VAR_POINTER || arg->var_value.type == VarType::VAR_EXTERN) {
                        if (spec.find("%s") != std::string::npos) {
                            std::snprintf(buffer, sizeof(buffer), spec.c_str(), static_cast<char*>(arg->var_value.ptr_value));
                        } else {
                            std::snprintf(buffer, sizeof(buffer), spec.c_str(), arg->var_value.ptr_value);
                        }
                    } else if (arg->type == VarType::VAR_FLOAT) {
                        std::snprintf(buffer, sizeof(buffer), spec.c_str(), arg->var_value.float_value);
                    } else if (arg->type == VarType::VAR_STRING) {
                        std::snprintf(buffer, sizeof(buffer), spec.c_str(), arg->var_value.str_value.c_str());
                    } else if(arg->type == VarType::VAR_BYTE) {
                        std::snprintf(buffer, sizeof(buffer), spec.c_str(), arg->var_value.int_value);
                    } 
                    else {
                        std::snprintf(buffer, sizeof(buffer), "%s", "(unsupported)");
                    }
                    oss << buffer;
                } else {
                    oss << spec; 
                }
                i = j;
            } else {
                oss << fmt[i];
            }
        }
        if(output)
            std::cout << oss.str();
        return oss.str();
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
        } else if (var.var_value.type == VarType::VAR_POINTER || var.var_value.type == VarType::VAR_EXTERN) {
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
        } else if (var.type == VarType::VAR_EXTERN) {
            if (!std::holds_alternative<void*>(value)) {
                throw mx::Exception("POP type mismatch: expected pointer, integer found.");
            }
            var.var_value.ptr_value = std::get<void*>(value);
            var.var_value.type = VarType::VAR_EXTERN;
        } 
        else {
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

    void Program::exec_stack_sub(const Instruction &instr) {
        size_t count = 1;
        if (!instr.op1.op.empty()) {
            if (isVariable(instr.op1.op)) {
                count = static_cast<size_t>(getVariable(instr.op1.op).var_value.int_value);
            } else {
                count = static_cast<size_t>(std::stoll(instr.op1.op, nullptr, 0));
            }
        }
        if (count > stack.size()) {
            throw mx::Exception("STACK_SUB: not enough values on stack to pop " + std::to_string(count));
            return;
        }
        for (size_t i = 0; i < count; ++i) {
            stack.pop();
        }
    }

    void Program::exec_call(const Instruction &instr) {
        if (instr.op1.op.empty()) {
            throw mx::Exception("CALL requires a label operand");
            return;
        }
        auto it = labels.find(instr.op1.op);
        if(it != labels.end()) {
            stack.push(static_cast<int64_t>(pc + 1));
            pc = it->second.first;
        } else {
            throw mx::Exception("CALL Label not found: " + instr.op1.op);
        }
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

    void Program::exec_done(const Instruction &i) {
        exitCode = 0;
        stop();
    }

    void Program::exec_to_int(const Instruction &instr) {
        if(!instr.op1.op.empty() && isVariable(instr.op1.op)) {
            Variable &v = getVariable(instr.op1.op);
            if(v.type == VarType::VAR_INTEGER) {
                if(isVariable(instr.op2.op)) {
                    Variable &s = getVariable(instr.op2.op);
                    if(s.type == VarType::VAR_STRING) {
                        try {
                            v.var_value.int_value = std::stoll(s.var_value.str_value, nullptr, 0); 
                        } catch(...) {
                            v.var_value.int_value = 0;
                        }
                    } else if(s.type == VarType::VAR_FLOAT) {
                        v.var_value.int_value = static_cast<int>(s.var_value.float_value);
                    }
                } else {
                    throw mx::Exception("to_int second argument must be a variable");
                }
            } else {
                throw mx::Exception("to_int first argument must be an integer variable");
            }
        } else {
            throw mx::Exception("to_int first argument must be a variable");
        }
    }

    void Program::exec_to_float(const Instruction &instr) {
        if (!instr.op1.op.empty() && isVariable(instr.op1.op)) {
            Variable &v = getVariable(instr.op1.op);
            if (v.type == VarType::VAR_FLOAT) {
                if (isVariable(instr.op2.op)) {
                    Variable &s = getVariable(instr.op2.op);
                    if (s.type == VarType::VAR_STRING) {
                        try {
                            v.var_value.float_value = std::stod(s.var_value.str_value);
                            v.var_value.type = VarType::VAR_FLOAT;
                        } catch (...) {
                            v.var_value.float_value = 0.0;
                            v.var_value.type = VarType::VAR_FLOAT;
                        }
                    } else if (s.type == VarType::VAR_INTEGER || s.type == VarType::VAR_BYTE) {
                        v.var_value.float_value = static_cast<double>(s.var_value.int_value);
                        v.var_value.type = VarType::VAR_FLOAT;
                    } else if (s.type == VarType::VAR_FLOAT) {
                        v.var_value.float_value = s.var_value.float_value;
                        v.var_value.type = VarType::VAR_FLOAT;
                    } else if (s.type == VarType::VAR_POINTER || s.type == VarType::VAR_EXTERN) {
                        v.var_value.float_value = static_cast<double>(reinterpret_cast<uintptr_t>(s.var_value.ptr_value));
                        v.var_value.type = VarType::VAR_FLOAT;
                    }
                } else {
                    throw mx::Exception("to_float second argument must be a variable");
                }
            } else {
                throw mx::Exception("to_float first argument must be a float variable");
            }
        } else {
            throw mx::Exception("to_float first argument must be a variable");
        }
    }

    void Program::exec_return(const Instruction &instr) {
        if(!instr.op1.op.empty() && isVariable(instr.op1.op)) {
            Variable &v = getVariable(instr.op1.op);
            std::string name = v.var_name;
            Variable &r = getVariable(result.op);
            if(v.type != r.type) {
                throw mx::Exception("Invalid return type: " + instr.op1.op + " != "  + result.op  + " type mismatch.\n");
            }
            v = r;
            v.var_name = name;
        }
    }
    void Program::exec_neg(const Instruction &instr) {
        if(!instr.op1.op.empty() && isVariable(instr.op1.op)) {
            Variable &v = getVariable(instr.op1.op);
            switch(v.type) {
                case VarType::VAR_INTEGER:
                    v.var_value.int_value = -v.var_value.int_value;
                break;
                case VarType::VAR_FLOAT:
                    v.var_value.float_value = -v.var_value.float_value;
                break;
            default:
                    v.var_value.int_value = -v.var_value.int_value;
            }
        }
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
        out << "Debug Information..\n";
        out << "=== INSTRUCTIONS ===\n";
        if (inc.empty()) {
            out << "  (no instructions)\n";
        } else {
            out << std::left << std::setw(10) << "Addr" 
                << std::setw(20) << "Instruction" 
                << std::setw(25) << "Operand1" 
                << std::setw(25) << "Operand2" 
                << std::setw(25) << "Operand3"
                << std::setw(30) << "Extra Operands" << "\n";
            out << std::string(130, '-') << "\n";
            
            for (size_t i = 0; i < inc.size(); ++i) {
                const auto &instr = inc[i];
                out << std::setw(10) << i;
                if (instr.instruction >= 0 && instr.instruction < static_cast<int>(IncType.size())) {
                    out << std::setw(12) << IncType[instr.instruction];
                } else {
                    out << std::setw(12) << "UNKNOWN";
                }
                out << std::setw(25) << (instr.op1.op.empty() ? "-" : instr.op1.op);
                out << std::setw(25) << (instr.op2.op.empty() ? "-" : instr.op2.op);
                out << std::setw(25) << (instr.op3.op.empty() ? "-" : instr.op3.op);   
                if (!instr.vop.empty()) {
                    std::string extraOps;
                    for (size_t j = 0; j < instr.vop.size(); ++j) {
                        if (j > 0) extraOps += ", ";
                        extraOps += instr.vop[j].op;
                    }
                    out << std::setw(30) << extraOps;
                } else {
                    out << std::setw(30) << "-";
                }
                
                out << "\n";
            }
        }
        out << "\n";
    }

    
    void Program::exec_invoke(const Instruction &instr) {
        auto it = external_functions.find(instr.op1.op);
        if (it == external_functions.end()) {
            throw mx::Exception("INVOKE: external function not found: " + instr.op1.op);
        }
        RuntimeFunction &fn = it->second;
        if(!fn.func) {
            throw mx::Exception("INVOKE: function: " + instr.op1.op + " not found");
        }
        std::vector<Operand> args;
        if (!instr.op2.op.empty()) args.push_back(instr.op2);
        if (!instr.op3.op.empty()) args.push_back(instr.op3);
        for (const auto& vop : instr.vop) {
            if (!vop.op.empty()) args.push_back(vop);
        }
        fn.call(this, args);
        result.op = "%rax";
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

    Variable Program::variableFromOperand(const Operand &op) {
        if(isVariable(op.op)) {
            return getVariable(op.op);
        } else if(op.type == OperandType::OP_CONSTANT) {
            return createTempVariable(VarType::VAR_INTEGER, op.op);
        }
        throw mx::Exception("Could not create variable from operand: " + op.op);
    }

    void Program::exec_fcmp(const Instruction& instr) {
        Variable* var1 = nullptr;
        Variable* var2 = nullptr;
        Variable temp1, temp2;
        
        if (isVariable(instr.op1.op)) {
            var1 = &getVariable(instr.op1.op);
        } else {
            temp1 = createTempVariable(VarType::VAR_FLOAT, instr.op1.op);
            var1 = &temp1;
        }
        if (isVariable(instr.op2.op)) {
            var2 = &getVariable(instr.op2.op);
        } else {
            temp2 = createTempVariable(VarType::VAR_FLOAT, instr.op2.op);
            var2 = &temp2;
        }
        
        
        zero_flag = false;
        less_flag = false;
        greater_flag = false;
        
        
        double val1, val2;
        if (var1->type == VarType::VAR_FLOAT) {
            val1 = var1->var_value.float_value;
        } else if (var1->type == VarType::VAR_INTEGER || var1->type == VarType::VAR_BYTE) {
            val1 = static_cast<double>(var1->var_value.int_value);
        } else {
            throw mx::Exception("FCMP: unsupported operand type for var1");
        }
        
        if (var2->type == VarType::VAR_FLOAT) {
            val2 = var2->var_value.float_value;
        } else if (var2->type == VarType::VAR_INTEGER || var2->type == VarType::VAR_BYTE) {
            val2 = static_cast<double>(var2->var_value.int_value);
        } else {
            throw mx::Exception("FCMP: unsupported operand type for var2");
        }
        
        
        if (val1 == val2) {
            zero_flag = true;
        } else if (val1 < val2) {
            less_flag = true;
        } else {
            greater_flag = true;
        }
    }

    void Program::exec_jae(const Instruction& instr) {
        if (greater_flag || zero_flag) exec_jmp(instr);
        else pc++;
    }

    void Program::exec_jbe(const Instruction& instr) {
        if (less_flag || zero_flag) exec_jmp(instr);
        else pc++;
    }

    void Program::exec_jc(const Instruction& instr) {
        if (carry_flag) exec_jmp(instr);
        else pc++;
    }

    void Program::exec_jnc(const Instruction& instr) {
        if (!carry_flag) exec_jmp(instr);
        else pc++;
    }

    void Program::exec_jp(const Instruction& instr) {
        pc++;
    }

    void Program::exec_jnp(const Instruction& instr) {
        exec_jmp(instr);
    }

    void Program::exec_jo(const Instruction& instr) {
        pc++;
    }

    void Program::exec_jno(const Instruction& instr) {
        exec_jmp(instr);
    }

    void Program::exec_js(const Instruction& instr) {
        pc++;
    }

    void Program::exec_jns(const Instruction& instr) {
        exec_jmp(instr);
    }

}