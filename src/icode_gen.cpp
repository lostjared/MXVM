#include"mxvm/icode.hpp"

namespace mxvm {

    static std::string getPlatformSymbolName(const std::string &name) {
        
#ifdef __APPLE__
        
        if(name == "stdin")
            return "___stdinp";
        if(name == "stderr")
            return "___stderrp";
        if(name == "stdout")
            return "___stdoutp";
        
        if(!name.empty() && name[0] != '_')
            return "_" + name;
        else
            return name;
#else
        return name;        // ELF/Linux, no underscore
#endif
    }


    void Program::generateCode(bool obj, std::ostream &out) {
        this->add_standard();
        std::unordered_map<int, std::string> labels_;
        for(auto &l : labels) {
            labels_[l.second.first] = l.first;
            
        }
        
#ifdef __APPLE__
        out << ".section __DATA, __data\n";
#else
        out << ".section .data\n";
#endif
        std::vector<std::string> var_names;
        for(auto &v : vars) {
            var_names.push_back(v.first);
        }
        std::sort(var_names.begin(), var_names.end());
        
     
        for(auto &v : var_names) {
            const std::string var_out_name = getPlatformSymbolName(getMangledName(v));
            auto varx = getVariable(v);
            if(varx.type == VarType::VAR_INTEGER) {
                out << "\t" << var_out_name << ": .quad " << vars[v].var_value.int_value << "\n";
                continue;
            }
            if(varx.type == VarType::VAR_FLOAT) {
                out << "\t" << var_out_name << ": .double " << vars[v].var_value.float_value << "\n";
                continue;
            }
            if(varx.type == VarType::VAR_BYTE) {
                out << "\t" << var_out_name << ": .byte " << vars[v].var_value.int_value << "\n";
            }
        }    
        for(auto &v : var_names) {
            auto varx = getVariable(v);
            //if(varx.type == VarType::VAR_EXTERN) {
                //out << "\t.extern " << getPlatformSymbolName(getMangledName(v)) << "\n";
           // }
        }
        
#ifndef __APPLE__
        out << "\t.extern " << getPlatformSymbolName("stderr") << "\n";
        out << "\t.extern " << getPlatformSymbolName("stdin") << "\n";
        out << "\t.extern " << getPlatformSymbolName("stdout") << "\n";
#endif
        
        
        for(auto &v : var_names) {
            auto varx = getVariable(v);
            if(varx.type == VarType::VAR_STRING && varx.var_value.buffer_size == 0) {
                const std::string var_out_name = getPlatformSymbolName(getMangledName(v));
                out << "\t" << var_out_name << ": .asciz " << "\"" << escapeNewLines(vars[v].var_value.str_value) << "\"\n";
                continue;
            } 
        }

        
        if(base != nullptr) {
            
            for(auto &v : var_names) {
                auto varx = getVariable(v);
                auto t = varx.type;
                if(varx.is_global) {
                    switch(t) {
                        case VarType::VAR_BYTE:
                        case VarType::VAR_FLOAT:
                        case VarType::VAR_INTEGER:
                            out << "\t.global " << getPlatformSymbolName(getMangledName(v)) << "\n";
                        break;
                        default:
                        break;
                    }
                }
            }
        }

#ifdef __APPLE__
        out << ".section __DATA,__bss\n";
#else
        out << ".section .bss\n";
#endif
        for(auto &v : var_names) {
            const std::string var_out_name = getMangledName(v);
            auto varx = getVariable(v);
            if(varx.type == VarType::VAR_POINTER) {
                out << "\t.comm " << getPlatformSymbolName(var_out_name) << ", 8\n";
            } else if(varx.type == VarType::VAR_STRING && varx.var_value.buffer_size > 0) {
                out << "\t.comm " << getPlatformSymbolName(var_out_name) << ", " << varx.var_value.buffer_size << "\n";
            }
        }
            if(object) {
                for(auto &v : var_names) {
                    auto varx = getVariable(v);
                    if(varx.is_global && (varx.type == VarType::VAR_STRING || varx.type == VarType::VAR_POINTER)) {
                        out << "\t.global " << getPlatformSymbolName(getMangledName(v)) << "\n";
                    }
                }
            }
#ifdef __APPLE__
        out << ".section __TEXT, __text\n";
#else
        out << ".section .text\n";
#endif

        if(this->object)
            out << "\t.global " << getPlatformSymbolName(name) << "\n";
        //else
        //    out << "\t.global main\n";

        for(auto &lbl : labels) {
            if(lbl.second.second == true) {
                out << "\t.global " << getPlatformSymbolName(name + "_" + lbl.first) << "\n";
            }
        }
        std::sort(external.begin(), external.end(), [](const ExternalFunction &a, const ExternalFunction &b) {
            if (a.mod == b.mod)
            return a.name < b.name;
            return a.mod < b.mod;
        });

        for(auto &e : external) {
            if(e.module == true)
                out << "\t.extern " << getPlatformSymbolName(e.name) << "\n";
            else
                out << "\t.extern " << getPlatformSymbolName(e.mod + "_" + e.name) << "\n";
        }
    
        if(this->object) {
            out << "\t.p2align 4, 0x90\n";
            out << getPlatformSymbolName(name) << ":\n";
        }
        else {
            out << "\t.p2align 4, 0x90\n";
            out << ".global " << getPlatformSymbolName("main") << "\n";
            
#ifndef __APPLE__
            out << ".type main, @function\n";
#endif
            out << getPlatformSymbolName("main") << ":\n";
        }

        out << "\tpush %rbp\n";
        out << "\tmov %rsp, %rbp\n";
    
        bool done_found = false;

        for(size_t i = 0; i < inc.size(); ++i) {
            const Instruction &instr = inc[i];
            for (auto l : labels) {
                if(l.second.first == i && l.second.second) {
                    out << "\t.p2align 4, 0x90\n";
                    out << getPlatformSymbolName(name + "_" + l.first) << ":\n";
                    out << "\tpush %rbp\n";
                    out << "\tmov %rsp, %rbp\n";
                    break;
                }
            }            
            for (auto l : labels) {
                if(l.second.first == i && !l.second.second) {
                    out << "." << l.first << ":\n";
                }
            }
            if(instr.instruction == DONE)
                done_found = true;
            generateInstruction(out, instr);
        }
        
        #ifndef __EMSCRIPTEN__
            if(this->object == false && done_found == false)
                throw mx::Exception("Program missing done to signal completion.\n");
        #endif
#ifndef __APPLE__
        out << "\n\n\n.section .note.GNU-stack,\"\",@progbits\n\n";
#endif
   
        std::string mainFunc = " Object";
        if(root_name == name)
                mainFunc = " Program";
        std::cout << Col("MXVM: Compiled: ", mx::Color::BRIGHT_BLUE) << name << ".s"  << mainFunc << "\n";
    }


    void Program::generateInstruction(std::ostream &out, const Instruction  &i) {
        switch(i.instruction) {
            case ADD:
                gen_arth(out, "add", i);
                break;
            case SUB:
                gen_arth(out, "sub", i);
                break;
            case MUL:
                gen_arth(out, "mul", i);
                break;
            case PRINT:
                gen_print(out, i);
                break;
            case EXIT:
                gen_exit(out, i);
                break;
            case MOV:
                gen_mov(out, i);
                break;
            case JMP:
            case JE:
            case JNE:
            case JL:
            case JLE:
            case JG:
            case JGE:
            case JZ:
            case JNZ:
            case JA:
            case JB:
                gen_jmp(out, i);
                break;
            case CMP:
                gen_cmp(out, i);
                break;
            case ALLOC:
                gen_alloc(out, i);
                break;
            case FREE:
                gen_free(out, i);
                break;
            case LOAD:
                gen_load(out, i);
                break;
            case STORE:
                gen_store(out, i);
                break;
            case RET:
                gen_ret(out, i);
                break;
            case CALL:
                gen_call(out, i);
                break;
            case DONE:
                gen_done(out, i);
                break;
            case AND:
                gen_bitop(out, "and", i);
                break;
            case OR:
                gen_bitop(out, "or", i);
                break;
            case XOR:
                gen_bitop(out, "xor", i);
                break;
            case NOT:
                gen_not(out, i);
                break;
            case DIV:
                gen_div(out, i);
                break;
            case MOD:
                gen_mod(out, i);
                break;
            case PUSH:
                gen_push(out, i);
                break;
            case POP:
                gen_pop(out, i);
                break;
            case STACK_LOAD:
                gen_stack_load(out, i);
                break;
            case STACK_STORE:
                gen_stack_store(out, i);
                break;
            case STACK_SUB:
                gen_stack_sub(out, i);
                break;
            case GETLINE:
                gen_getline(out, i);
                break;
            case TO_INT:
                gen_to_int(out, i);
                break;
            case TO_FLOAT:
                gen_to_float(out, i);
                break;
            case INVOKE:
                gen_invoke(out, i);
                break;
            case RETURN:
                gen_return(out, i);
                break;
            case NEG:
                gen_neg(out, i);
                break;
        default:
            throw mx::Exception("Invalid or unsupported instruction: " + std::to_string(static_cast<unsigned int>(i.instruction)));
        }
    }
    void Program::gen_done(std::ostream &out, const Instruction &i) {
        out << "\txor %eax, %eax\n";
        out << "\tleave\n";
        out << "\tret\n";
    }

    void Program::gen_ret(std::ostream &out, const Instruction &i) {
        out << "\tleave\n";
        out << "\tret\n";
    }

    void Program::gen_return(std::ostream &out, const Instruction &i) {
        if(isVariable(i.op1.op)) {
            out << "\tmovq %rax, " << getMangledName(i.op1) << "(%rip)\n";
        } else {
            throw mx::Exception("return: " + i.op1.op + " requires variable\n");
        }
    }
    
    void Program::gen_neg(std::ostream &out, const Instruction &i) {
        if(isVariable(i.op1.op)) {
            Variable &v = getVariable(i.op1.op);
            if(v.type == VarType::VAR_INTEGER) {
                generateLoadVar(out, v.type, "%rcx", i.op1);
                out << "\tnegq %rcx\n";
                out << "\tmovq %rcx, " << getMangledName(i.op1) << "(%rip)\n";
            } else if(v.type == VarType::VAR_FLOAT) {
                generateLoadVar(out, VarType::VAR_FLOAT, "%xmm0", i.op1);
                out << "\txorpd %xmm1, %xmm1\n";
                out << "\tsubsd %xmm0, %xmm1\n";
                out << "\tmovsd %xmm1, " << getMangledName(i.op1) << "(%rip)\n";
            } else {
                throw mx::Exception("neg requires float or integer.");
            }
        } else {
            throw mx::Exception("neg requires variable");
        }
    }

    void Program::gen_invoke(std::ostream &out, const Instruction &i) {
            if (i.op1.op.empty()) {
                throw mx::Exception("invoke instruction requires operand of instruction name");
            }
            std::vector<Operand> op;
            op.push_back(i.op1);                 
            if (!i.op2.op.empty()) op.push_back(i.op2);
            if (!i.op3.op.empty()) op.push_back(i.op3);
            for (const auto &vx : i.vop) {       
                if (!vx.op.empty()) op.push_back(vx);
            }
            generateInvokeCall(out, op);
      }

        void Program::generateInvokeCall(std::ostream &out, std::vector<Operand> &op) {
            if (op.empty() || op[0].op.empty()) {
                throw mx::Exception("invoke instruction requires operand of instruction name");
            }
            std::string name = op[0].op;
            std::vector<Operand> args;
            for (size_t i = 1; i < op.size(); ++i) {
                if (!op[i].op.empty()) args.push_back(op[i]);
            }
            generateFunctionCall(out, name, args);
        }

    void Program::generateFunctionCall(std::ostream &out, const std::string &name, std::vector<Operand> &op) {
        const std::vector<Operand> &args = op;
       xmm_offset = 0;
        size_t stack_args = (args.size() > 6) ? (args.size() - 6) : 0;
        bool needs_dummy = (stack_args % 2) != 0; 

        if (needs_dummy) out << "\tsub $8, %rsp\n";
        if (stack_args) {
            for (size_t idx = args.size(); idx-- > 6; ) {
                generateLoadVar(out, VarType::VAR_INTEGER, "%rax", args[idx]);
                out << "\tpushq %rax\n";
            }
        }

        int reg_count = 0;
        for (size_t z = 0; z < args.size() && reg_count < 6; ++z, ++reg_count) {
            generateLoadVar(out, reg_count, args[z]);
        }

        out << "\txor %eax, %eax\n";
        out << "\tcall " << getPlatformSymbolName(name) << "\n";

        if (stack_args || needs_dummy) {
            out << "\tadd $" << ((stack_args + (needs_dummy ? 1 : 0)) * 8) << ", %rsp\n";
        }
    }
    
    void Program::gen_call(std::ostream &out, const Instruction &i) {
        if(isFunctionValid(i.op1.op)) {
            out << "\tcall " << getPlatformSymbolName(getMangledName(i.op1)) << "\n";
        } else {
            throw mx::Exception("Function " + i.op1.op + " not found!");
        }
    }

    void Program::gen_alloc(std::ostream &out, const Instruction &i) {

        if (!isVariable(i.op1.op)) {
            throw mx::Exception("ALLOC destination must be a variable");
        }

        Variable &v = getVariable(i.op1.op);
        if(v.type != VarType::VAR_POINTER) {
            throw mx::Exception("ALLOC destination must be a pointer\n");
        }
        
        if (!i.op2.op.empty()) {
            if (isVariable(i.op2.op)) {
                Variable &sizeVar = getVariable(i.op2.op);
                if (sizeVar.type != VarType::VAR_INTEGER) {
                    throw mx::Exception("ALLOC size must be integer");
                }
                out << "\tmovq " << getMangledName(i.op2) << "(%rip), %rsi\n";
            } else {
                out << "\tmovq $" << i.op2.op << ", %rsi\n";
            }
        } else {
            out << "\tmovq $8, %rsi\n";
        }
        if (!i.op3.op.empty()) {
            if (isVariable(i.op3.op)) {
                Variable &countVar = getVariable(i.op3.op);
                if (countVar.type != VarType::VAR_INTEGER) {
                    throw mx::Exception("ALLOC count must be integer");
                }
                out << "\tmovq " << getMangledName(i.op3) << "(%rip), %rdi\n";
            } else {
                out << "\tmovq $" << i.op3.op << ", %rdi\n";
            }
        } else {
            out << "\tmovq $1, %rdi\n";
        }
        out << "\tcall " << getPlatformSymbolName("calloc") << "\n";
        out << "\tmovq %rax, " << getMangledName(i.op1) << "(%rip)\n";
        Variable &var = getVariable(i.op1.op);
        var.var_value.owns = true;
    }

    void Program::gen_free(std::ostream &out, const Instruction &i) {
        if (!isVariable(i.op1.op)) {
            throw mx::Exception("FREE argument must be a variable");
        }
        Variable &v = getVariable(i.op1.op);
        if (v.type != VarType::VAR_POINTER) {
            throw mx::Exception("FREE argument must be a pointer");
        }      
        out << "\tmovq " << getMangledName(i.op1) << "(%rip), %rdi\n";
        out << "\tcall " << getPlatformSymbolName("free") << "\n";
    }
    void Program::gen_load(std::ostream &out, const Instruction &i) {
        if (!isVariable(i.op1.op)) {
            throw mx::Exception("LOAD destination: " + i.op1.op + " must be a variable");
        }
        Variable &dest = getVariable(i.op1.op);
        if (!isVariable(i.op2.op)) {
            throw mx::Exception("LOAD source: " + i.op2.op + " must be a pointer variable");
        }
        Variable &ptrVar = getVariable(i.op2.op);
        size_t size = (dest.type == VarType::VAR_FLOAT) ? sizeof(double) : sizeof(int64_t);
        if (!i.vop.empty() && !i.vop[0].op.empty()) {
            if (isVariable(i.vop[0].op)) {
                size = static_cast<size_t>(getVariable(i.vop[0].op).var_value.int_value);
            } else {
                size = static_cast<size_t>(std::stoll(i.vop[0].op, nullptr, 0));
            }
        }
        std::string idx_reg = "%rcx";
        if (!i.op3.op.empty()) {
            generateLoadVar(out, VarType::VAR_INTEGER, idx_reg, i.op3);
        } else {
            out << "\txor %rcx, %rcx\n";
        }

        if(ptrVar.type == VarType::VAR_POINTER)
            out << "\tmovq " << getMangledName(i.op2) << "(%rip), %rax\n";
        else if(ptrVar.type == VarType::VAR_STRING)
            out << "\tleaq " << getMangledName(i.op2) << "(%rip), %rax\n";
        else throw mx::Exception("Load invalid type: " + i.op2.op);

        auto emit_load = [&](const std::string& mem) {
            if (dest.type == VarType::VAR_INTEGER) {
                out << "\tmovq " << mem << ", %rdx\n";
                out << "\tmovq %rdx, " << getMangledName(i.op1) << "(%rip)\n";
            } else if (dest.type == VarType::VAR_FLOAT) {
                out << "\tmovsd " << mem << ", %xmm0\n";
                out << "\tmovsd %xmm0, " << getMangledName(i.op1) << "(%rip)\n";
            } else if (dest.type == VarType::VAR_BYTE) {
                out << "\tmovzbq " << mem << ", %r8\n";
                out << "\tmovb %r8b, " << getMangledName(i.op1) << "(%rip)\n";
            } else {
                throw mx::Exception("LOAD: unsupported destination type ");
            }
        };
        if (size == 1 || size == 2 || size == 4 || size == 8) {
            int scale = (int)size;
            std::string mem = "(" + std::string("%rax") + ",%rcx," + std::to_string(scale) + ")";
            emit_load(mem);
        } else {
            out << "\timul $" << size << ", %rcx, %rcx\n";
            out << "\tadd %rcx, %rax\n";
            emit_load("(%rax)");
        }
    }

    void Program::gen_store(std::ostream &out, const Instruction &i) {
        uint64_t value = 0;
        VarType  type;
        bool is_const = false;
      
        if (!isVariable(i.op1.op) && i.op1.type == OperandType::OP_CONSTANT) {
            value = std::stoll(i.op1.op, nullptr, 0);            
            type =  VarType::VAR_INTEGER;
            is_const = true;
        } else {
            Variable &src = getVariable(i.op1.op);
            value = src.var_value.int_value;
            type = src.type;
            is_const = false;
        }

        if (!isVariable(i.op2.op)) {
            throw mx::Exception("STORE destination must be a pointer variable: " + i.op2.op);
        }
        Variable &ptrVar = getVariable(i.op2.op);
        size_t elemSize = 8;
        bool elemSizeKnown = true;
        if (!i.vop.empty() && !i.vop[0].op.empty()) {
            if (isVariable(i.vop[0].op)) {
                elemSizeKnown = false; 
                generateLoadVar(out, VarType::VAR_INTEGER, "%rdx", i.vop[0]);
            } else {
                elemSize = static_cast<size_t>(std::stoll(i.vop[0].op, nullptr, 0));
            }
        } else {
            elemSize = 8;
        }

        std::string idx_reg = "%rcx";
        if (!i.op3.op.empty()) {
            generateLoadVar(out, VarType::VAR_INTEGER, idx_reg, i.op3);
        } else {
           throw mx::Exception("STORE resize third argument of size count");
        }

        if(ptrVar.type == VarType::VAR_POINTER)
             out << "\tmovq " << getMangledName(i.op2) << "(%rip), %rax\n";
        else if(ptrVar.type == VarType::VAR_STRING && ptrVar.var_value.buffer_size > 0)
            out << "\tleaq " << getMangledName(i.op2) << "(%rip), %rax\n";
        else
            throw mx::Exception ("STORE must be pointer or string buffer");

        auto emit_store = [&](const std::string& mem) {
            if(is_const == true) {
                if (type == VarType::VAR_BYTE || elemSize == 1) {
                    out << "\tmovb $" << (value & 0xFF) << ", " << mem << "\n";
                } else if (elemSize == 4) {
                    out << "\tmovl $" << (uint32_t)(value & 0xFFFFFFFFu) << ", " << mem << "\n";
                } else {
                    out << "\tmovq $" << value << ", " << mem << "\n";
                }
                return;
            }
            if (type == VarType::VAR_INTEGER || type == VarType::VAR_POINTER || type == VarType::VAR_EXTERN) {
                out << "\tmovq " << getMangledName(i.op1) << "(%rip), %rdx\n";
                out << "\tmovq %rdx, " << mem << "\n";
            } else if(type == VarType::VAR_BYTE) {
                out << "\tmovzbq " << getMangledName(i.op1) << "(%rip), %r8\n";
                out << "\tmovb %r8b, " << mem << "\n";
            } else if (type == VarType::VAR_FLOAT) {
                out << "\tmovsd " << getMangledName(i.op1) << "(%rip), %xmm0\n";
                out << "\tmovsd %xmm0, " << mem << "\n";
            } else {
                throw mx::Exception("STORE: unsupported source type");
            }
        };

        if (elemSizeKnown && (elemSize == 1 || elemSize == 2 || elemSize == 4 || elemSize == 8)) {
            int scale = (int)elemSize;
            std::string mem = "(" + std::string("%rax") + ",%rcx," + std::to_string(scale) + ")";
            emit_store(mem);
        } else {
            out << "\timulq %rdx, %rcx\n";
            out << "\taddq %rcx, %rax\n";
           emit_store("(%rax)");
        }
    }

    void Program::gen_to_int(std::ostream &out, const Instruction &i) {
        if (!isVariable(i.op1.op) || !isVariable(i.op2.op)) {
            throw mx::Exception("to_int requires two variables (dest int, src string)");
        }
        Variable &dest = getVariable(i.op1.op);
        Variable &src = getVariable(i.op2.op);
        if (dest.type != VarType::VAR_INTEGER) {
            throw mx::Exception("to_int: first argument must be an integer variable");
        }
        if (src.type != VarType::VAR_STRING) {
            throw mx::Exception("to_int: second argument must be a string variable");
        }
        out << "\tleaq " << getMangledName(i.op2) << "(%rip), %rdi\n";
        out << "\tcall atol\n";
        out << "\tmovq %rax, " << getMangledName(i.op1) << "(%rip)\n";
    }

    void Program::gen_to_float(std::ostream &out, const Instruction &i) {
        if (!isVariable(i.op1.op) || !isVariable(i.op2.op)) {
            throw mx::Exception("to_float requires two variables (dest float, src string)");
        }
        Variable &dest = getVariable(i.op1.op);
        Variable &src = getVariable(i.op2.op);
        if (dest.type != VarType::VAR_FLOAT) {
            throw mx::Exception("to_float: first argument must be a float variable");
        }
        if (src.type != VarType::VAR_STRING) {
            throw mx::Exception("to_float: second argument must be a string variable");
        }
        out << "\tleaq " << getMangledName(i.op2) << "(%rip), %rdi\n";
        out << "\tcall " << getPlatformSymbolName("atof") << "\n";
        out << "\tmovsd %xmm0, " << getMangledName(i.op1) << "(%rip)\n";
    }
        

    void Program::gen_div(std::ostream &out, const Instruction &i) {
        if (i.op3.op.empty()) {
            if (isVariable(i.op1.op)) {
                Variable &v = getVariable(i.op1.op);
                if (v.type == VarType::VAR_INTEGER) {
                    generateLoadVar(out, VarType::VAR_INTEGER, "%rax", i.op1);
                    generateLoadVar(out, VarType::VAR_INTEGER, "%rcx", i.op2);
                    out << "\tcmpq $0, %rcx\n";
                    out << "\tje 1f\n";
                    out << "\tcqto\n";
                    out << "\tidivq %rcx\n";
                    out << "\tjmp 2f\n";
                    out << "1:\n";
                    out << "\txor %eax, %eax\n";
                    out << "2:\n";
                    out << "\tmovq %rax, " << getMangledName(i.op1) << "(%rip)\n";
                } else if (v.type == VarType::VAR_FLOAT) {
                    generateLoadVar(out, VarType::VAR_FLOAT, "%xmm0", i.op1);
                    generateLoadVar(out, VarType::VAR_FLOAT, "%xmm1", i.op2);
                    out << "\txorpd %xmm2, %xmm2\n";
                    out << "\tucomisd %xmm2, %xmm1\n";
                    out << "\tje 1f\n";
                    out << "\tdivsd %xmm1, %xmm0\n";
                    out << "\tjmp 2f\n";
                    out << "1:\n";
                    out << "\txorpd %xmm0, %xmm0\n";
                    out << "2:\n";
                    out << "\tmovsd %xmm0, " << getMangledName(i.op1) << "(%rip)\n";
                } else {
                    throw mx::Exception("DIV: unsupported variable type");
                }
            } else {
                throw mx::Exception("DIV: first argument must be a variable");
            }
        } else {
            if (isVariable(i.op1.op)) {
                Variable &v = getVariable(i.op1.op);
                if (v.type == VarType::VAR_INTEGER) {
                    generateLoadVar(out, VarType::VAR_INTEGER, "%rax", i.op2);
                    generateLoadVar(out, VarType::VAR_INTEGER, "%rcx", i.op3);
                    out << "\tcmpq $0, %rcx\n";
                    out << "\tje 1f\n";
                    out << "\tcqto\n";
                    out << "\tidivq %rcx\n";
                    out << "\tjmp 2f\n";
                    out << "1:\n";
                    out << "\txor %eax, %eax\n";
                    out << "2:\n";
                    out << "\tmovq %rax, " << getMangledName(i.op1) << "(%rip)\n";
                } else if (v.type == VarType::VAR_FLOAT) {
                    generateLoadVar(out, VarType::VAR_FLOAT, "%xmm0", i.op2);
                    generateLoadVar(out, VarType::VAR_FLOAT, "%xmm1", i.op3);
                    out << "\txorpd %xmm2, %xmm2\n";
                    out << "\tucomisd %xmm2, %xmm1\n";
                    out << "\tje 1f\n";
                    out << "\tdivsd %xmm1, %xmm0\n";
                    out << "\tjmp 2f\n";
                    out << "1:\n";
                    out << "\txorpd %xmm0, %xmm0\n";
                    out << "2:\n";
                    out << "\tmovsd %xmm0, " << getMangledName(i.op1) << "(%rip)\n";
                } else {
                    throw mx::Exception("DIV: unsupported variable type");
                }
            } else {
                throw mx::Exception("DIV: first argument must be a variable");
            }
        }
    }

    void Program::gen_mod(std::ostream &out, const Instruction &i) {
        if (i.op3.op.empty()) {
            if (isVariable(i.op1.op)) {
                Variable &v = getVariable(i.op1.op);
                if (v.type == VarType::VAR_INTEGER) {
                    generateLoadVar(out, VarType::VAR_INTEGER, "%rax", i.op1);
                    generateLoadVar(out, VarType::VAR_INTEGER, "%rcx", i.op2);
                    out << "\tcmpq $0, %rcx\n";
                    out << "\tje 1f\n";
                    out << "\tcqto\n"; 
                    out << "\tidivq %rcx\n";
                    out << "\tjmp 2f\n";
                    out << "1:\n";
                    out << "\txor %edx, %edx\n";
                    out << "2:\n";
                    out << "\tmovq %rdx, " << getMangledName(i.op1) << "(%rip)\n"; 
                } else {
                    throw mx::Exception("MOD only supports integer variables");
                }
            } else {
                throw mx::Exception("MOD: first argument must be a variable");
            }
        } else {
            if (isVariable(i.op1.op)) {
                Variable &v = getVariable(i.op1.op);
                if (v.type == VarType::VAR_INTEGER) {
                    generateLoadVar(out, VarType::VAR_INTEGER, "%rax", i.op2);
                    generateLoadVar(out, VarType::VAR_INTEGER, "%rcx", i.op3);
                    out << "\tcmpq $0, %rcx\n";
                    out << "\tje 1f\n";
                    out << "\tcqto\n";
                    out << "\tidivq %rcx\n";
                    out << "\tjmp 2f\n";
                    out << "1:\n";
                    out << "\txor %edx, %edx\n";
                    out << "2:\n";
                    out << "\tmovq %rdx, " << getMangledName(i.op1) << "(%rip)\n";
                } else {
                    throw mx::Exception("MOD only supports integer variables");
                }
            } else {
                throw mx::Exception("MOD: first argument must be a variable");
            }
        }
    }

    void Program::gen_getline(std::ostream &out, const Instruction &i) {
        if (!isVariable(i.op1.op)) {
            throw mx::Exception("GETLINE destination must be a variable");
        }
        Variable &dest = getVariable(i.op1.op);
        if (dest.type != VarType::VAR_STRING || dest.var_value.buffer_size == 0 ) {
            throw mx::Exception("GETLINE destination must be a string buffer variable");
        }
        static int over_count = 0;
        out << "\tleaq " << getMangledName(i.op1) << "(%rip), %rdi\n";
        out << "\tmovq $" << dest.var_value.buffer_size << ", %rsi\n";
        out << "\tmovq "<<  getPlatformSymbolName("stdin") << "(%rip), %rdx\n";
        out << "\tcall " << getPlatformSymbolName("fgets") << "\n";
        out << "\tleaq " << getMangledName(i.op1) << "(%rip), %rdi\n";
        out << "\tcall " << getPlatformSymbolName("strlen") << "\n";
        out << "\tmovq %rax, %rcx\n";
        out << "\tcmp $0, %rax\n";
        out << "\tje .over" << over_count << "\n";
        out << "\tsub $1, %rcx\n";
        out << "\tleaq " << getMangledName(i.op1) << "(%rip), %rdi\n";
        out << "\tmovb $0, (%rdi, %rcx, 1)\n";
        out << ".over" << over_count << ": \n";
        over_count++;
    }

    void Program::gen_bitop(std::ostream &out, const std::string &opc, const Instruction &i) {
        if (i.op3.op.empty()) {
            if (isVariable(i.op1.op)) {
                Variable &v = getVariable(i.op1.op);
                if (v.type == VarType::VAR_INTEGER) {
                    generateLoadVar(out, VarType::VAR_INTEGER, "%rax", i.op1);
                    generateLoadVar(out, VarType::VAR_INTEGER, "%rcx", i.op2);
                    out << "\t" << opc << "q %rcx, %rax\n";
                    out << "\tmovq %rax, " << getMangledName(i.op1) << "(%rip)\n";
                } else {
                    throw mx::Exception("Bitwise operations only supported for integer variables");
                }
            } else {
                throw mx::Exception("First argument of bitop instruction must be a variable.");
            }
        } else {
            if (isVariable(i.op1.op)) {
                Variable &v = getVariable(i.op1.op);
                if (v.type == VarType::VAR_INTEGER) {
                    generateLoadVar(out, VarType::VAR_INTEGER, "%rax", i.op2);
                    generateLoadVar(out, VarType::VAR_INTEGER, "%rcx", i.op3);
                    out << "\t" << opc << "q %rcx, %rax\n";
                    out << "\tmovq %rax, " << getMangledName(i.op1) << "(%rip)\n";
                } else {
                    throw mx::Exception("Bitwise operations only supported for integer variables");
                }
            } else {
                throw mx::Exception("First argument of bitop instruction must be a variable.");
            }
        }
    }

    void Program::gen_not(std::ostream &out, const Instruction &i) {
        if(!i.op1.op.empty()) {
            if(isVariable(i.op1.op)) {
                Variable &v = getVariable(i.op1.op);
                if(v.type != VarType::VAR_INTEGER) {
                    throw mx::Exception("NOT instruction expect integer variable");
                }
                generateLoadVar(out, VarType::VAR_INTEGER, "%rax", i.op1);
                out << "\tnotq %rax\n";
                out << "\tmovq %rax, " << getMangledName(i.op1) << "(%rip)\n";
            } else {
                throw mx::Exception("NOT instruction requires variable");
            }
        } else {
            throw mx::Exception("NOT instruction requires operand");
        }
    }

    void Program::gen_push(std::ostream &out, const Instruction &i) {
        if (isVariable(i.op1.op)) {
            Variable &v = getVariable(i.op1.op);
            if (v.type == VarType::VAR_INTEGER || v.type == VarType::VAR_POINTER || v.type == VarType::VAR_EXTERN) {
                out << "\tmovq " << getMangledName(i.op1) << "(%rip), %rax\n";
                out << "\tpushq %rax\n";
            } else if (v.type == VarType::VAR_STRING) {
                out << "\tleaq " << getMangledName(i.op1) << "(%rip), %rax\n";
                out << "\tpushq %rax\n";
            } else {
                throw mx::Exception("PUSH only supports integer or pointer variables");
            }
        } else if (i.op1.type == OperandType::OP_CONSTANT) {
            out << "\tmovq $" << i.op1.op << ", %rax\n";
            out << "\tpushq %rax\n";
        } else {
            throw mx::Exception("PUSH argument must be a variable or integer constant");
        }
    }

    void Program::gen_pop(std::ostream &out, const Instruction &i) {
        if (!isVariable(i.op1.op)) {
            throw mx::Exception("POP destination must be a variable");
        }
        Variable &v = getVariable(i.op1.op);
        if (v.type == VarType::VAR_INTEGER) {
            out << "\tpopq %rax\n";
            out << "\tmovq %rax, " << getMangledName(i.op1) << "(%rip)\n";
        } else if (v.type == VarType::VAR_POINTER || v.type == VarType::VAR_EXTERN) {
            out << "\tpopq %rax\n";
            out << "\tmovq %rax, " << getMangledName(i.op1) << "(%rip)\n";
        } else {
            throw mx::Exception("POP only supports integer or pointer variables");
        }
    }

    void Program::gen_stack_load(std::ostream &out, const Instruction &i) {
        if(!isVariable(i.op1.op)) {
            throw mx::Exception("stack_load, requires first argument to be a variable.");
        }
        Variable &dest = getVariable(i.op1.op);
        if(i.op2.op.empty()) {
            throw mx::Exception("stack_load, requires second argument");
        }
        generateLoadVar(out, dest.type, "%rax", i.op2);
        out << "\tmovq (%rsp, %rax, 8), " << "%rcx\n";
        out << "\tmovq %rcx, " << getMangledName(i.op1) << "(%rip)\n";
    }

    void Program::gen_stack_store(std::ostream &out, const Instruction &i) {
         if(!isVariable(i.op1.op)) {
            throw mx::Exception("stack_store, requires first argument to be a variable.");
        }
        Variable &dest = getVariable(i.op1.op);
        if(i.op2.op.empty()) {
            throw mx::Exception("stack_store, requires second argument");
        }
        generateLoadVar(out, dest.type, "%rax", i.op2);
        out << "\tmovq " << getMangledName(i.op1) << "(%rip), %rcx\n";
        out << "\tmovq %rcx, (%rsp, %rax, 8)\n";
    }

    void Program::gen_stack_sub(std::ostream &out, const Instruction &i) {
        if (!i.op1.op.empty()) {
            if (isVariable(i.op1.op)) {
                out << "\tmovq " << getMangledName(i.op1) << "(%rip), %rcx\n";
            } else {
                out << "\tmovq $" << i.op1.op << ", %rcx\n";
            }
        } else {
            out << "\tmovq $1, %rcx\n";
        }
        out << "\tshl $3, %rcx\n"; 
        out << "\taddq %rcx, %rsp\n";
    }
        

    int Program::generateLoadVar(std::ostream &out, int r, const Operand &op) {
        int count = 0;
        
        if(isVariable(op.op)) {
            if(op.type != OperandType::OP_VARIABLE) {
                throw mx::Exception("Operand expected variable instead I found: " + op.op);
            }
            Variable &v = getVariable(op.op);
            
            std::string reg = getRegisterByIndex(r, v.type);
            switch(v.type) {
                case VarType::VAR_INTEGER:
                case VarType::VAR_POINTER:
                case VarType::VAR_EXTERN:
#ifdef ___APPLE__
                    std::cout << op.op << "###\n";
                    if(op.op == "stderr" || op.op == "stdin" || op.op == "stdout") {
                        out << "\tleaq " << getMangledName(op) << ", (%rip)" << reg  << "\n";
                        out << "\tmovq (" << reg << "), " << reg << "\n";
                        break;
                    }
#endif

                    out << "\tmovq " << getMangledName(op) << "(%rip), " << reg << "\n";
                    count = 0;
                    break;
                case VarType::VAR_FLOAT:
                    out << "\tmovsd " << getMangledName(op) << "(%rip), " << reg << "\n";
                    count = 1;
                break;
                case VarType::VAR_STRING:
                    count = 0;
                    out << "\tleaq " << getMangledName(op) << "(%rip), " << reg << "\n";
                break;
                case VarType::VAR_BYTE:
                    count = 0;
                    out << "\tmovzbq " << getMangledName(op) << "(%rip), " << reg << "\n";
                break;
                default:
                break;
            }
        } else {
            if(op.type == OperandType::OP_CONSTANT) {
                out << "\tmovq $" << op.op << ", " << getRegisterByIndex(r, VarType::VAR_INTEGER) << "\n";
            }
           
        }
        return count;
    }

    int Program::generateLoadVar(std::ostream &out, VarType type, std::string reg, const Operand &op) {
        int count = 0;
        if(isVariable(op.op)) {
            if(op.type != OperandType::OP_VARIABLE) {
                throw mx::Exception("Operand expected variable instead I foudn: " + op.op);
            }
            Variable &v = getVariable(op.op);
            if(v.type != type && type != VarType::VAR_INTEGER) {
                std::ostringstream vartype;
                vartype << v.type << " != " << type << "\n";
                throw mx::Exception ("LoadVar Variable type mismatch: " + op.op + " " + vartype.str());
            }
            switch(v.type) {
                case VarType::VAR_INTEGER:
                case VarType::VAR_POINTER:
                case VarType::VAR_EXTERN:
                    out << "\tmovq "  << getMangledName(op) << "(%rip), " << reg << "\n";
                    count = 0;
                    break;
                case VarType::VAR_BYTE:
                    out << "\tmovzbq " << getMangledName(op) << "(%rip), " << reg << "\n";
                    break;
                case VarType::VAR_FLOAT:
                    out << "\tmovsd " << getMangledName(op) << "(%rip), " << reg << "\n";
                    count = 1;
                break;
                case VarType::VAR_STRING:
                    count = 0;
                    out << "\tleaq " << getMangledName(op) << "(%rip), " << reg << "\n";
                break;
                default:
                break;
            }
        } else {
            if(op.type == OperandType::OP_CONSTANT && type == VarType::VAR_INTEGER) {
                out << "\tmovq $" << op.op << ", " << reg << "\n";
            } else if(op.type == OperandType::OP_CONSTANT && type == VarType::VAR_FLOAT) {
                throw mx::Exception("You cannot use a constant with a floating point variable.");
            }
        }
        return count;
    }

    std::string Program::getRegisterByIndex(int index, VarType type) {
        if(index < 6 && (type == VarType::VAR_INTEGER || type == VarType::VAR_STRING || type == VarType::VAR_POINTER || type == VarType::VAR_EXTERN || type == VarType::VAR_BYTE)) {
            static const char *reg[] = {"%rdi", "%rsi", "%rdx", "%rcx", "%r8", "%r9"};
            return reg[index];
        } else if(index < 9 && type == VarType::VAR_FLOAT) {
              return "%xmm" +  std::to_string(xmm_offset++);
        } 
        return "[stack]";
    }

    // ...existing code...
    void Program::gen_print(std::ostream &out, const Instruction &i) {
        xmm_offset = 0;
        out << "\tleaq " << getMangledName(i.op1) << "(%rip), %rdi\n";
        std::vector<Operand> args;
        if (!i.op2.op.empty()) args.push_back(i.op2);
        if (!i.op3.op.empty()) args.push_back(i.op3);
        for (const auto& v : i.vop) {
            if (!v.op.empty()) args.push_back(v);
        }

        int total = 0;
        const size_t stack_args = (args.size() > 5) ? (args.size() - 5) : 0;
        const bool needs_dummy = (stack_args % 2) != 0; // keep 16B alignment

        if (needs_dummy) {
            out << "\tsub $8, %rsp\n";
        }

        if (args.size() > 5) {
            for (size_t idx = args.size(); idx-- > 5; ) {
                if (isVariable(args[idx].op)) {
                    Variable &v = getVariable(args[idx].op);
                    switch (v.type) {
                        case VarType::VAR_INTEGER:
                        case VarType::VAR_POINTER:
                        case VarType::VAR_EXTERN:  {
                            generateLoadVar(out, VarType::VAR_INTEGER, "%rax", args[idx]);
                            out << "\tpushq %rax\n";
                            break;
                        }
                        case VarType::VAR_BYTE: {
                            generateLoadVar(out, VarType::VAR_BYTE, "%rax", args[idx]);
                            out << "\tpushq %rax\n";
                            break;
                        }
                        case VarType::VAR_FLOAT: {
                            generateLoadVar(out, VarType::VAR_FLOAT, "%xmm0", args[idx]);
                            out << "\tsub $8, %rsp\n";
                            out << "\tmovsd %xmm0, (%rsp)\n";
                            break;
                        case VarType::VAR_STRING:
                            std::cout << " STRING HERE \n";
                        break;
                        }
                        default:
                            throw mx::Exception("print: unsupported stack arg type");
                    }
                } else {
                    out << "\tpushq $" << args[idx].op << "\n";
                }
            }
        }
        int reg_count = 1; 
        for (size_t z = 0; z < args.size() && reg_count < 6; ++z, ++reg_count) {
            total += generateLoadVar(out, reg_count, args[z]);
        }
        if (total == 0) {
            out << "\txor %eax, %eax\n";
        } else {
            out << "\tmovb $" << total << ", %al\n";
        }
        out << "\tcall " << getPlatformSymbolName("printf") << "\n";
        if (stack_args || needs_dummy) {
            out << "\tadd $" << ((stack_args + (needs_dummy ? 1 : 0)) * 8) << ", %rsp\n";
        }
    }


    void Program::gen_jmp(std::ostream &out, const Instruction &i) {
        if(!i.op1.op.empty()) {
            auto pos = labels.find(i.op1.op);
            if(pos == labels.end()) {
                throw mx::Exception("Jump instruction must have valid label: " + i.op1.op);
            }
            const char* mnem = nullptr;
            switch (i.instruction) {
                case JMP: mnem = "jmp"; break;
                case JE:  mnem = "je";  break;
                case JNE: mnem = "jne"; break;
                case JL:  mnem = "jl";  break;
                case JLE: mnem = "jle"; break;
                case JG:  mnem = "jg";  break;
                case JGE: mnem = "jge"; break;
                case JZ:  mnem = "jz";  break;
                case JNZ: mnem = "jnz"; break;
                case JA:  mnem = "ja";  break;
                case JB:  mnem = "jb";  break;
                default:
                    throw mx::Exception("Invalid jump opcode for gen_jmp");
            }
            out << "\t" << mnem << " ." << i.op1.op << "\n";
        } else {
            throw mx::Exception("Jump instruction must have label: " + i.op1.op);
        }
    }

    void Program::gen_cmp(std::ostream &out, const Instruction &i) {
        if (i.op2.op.empty()) {
            throw mx::Exception("CMP requires two operands");
        }

        VarType type1 = VarType::VAR_INTEGER;
        VarType type2 = VarType::VAR_INTEGER;
        if (isVariable(i.op1.op)) {
            type1 = getVariable(i.op1.op).type;
        }
        if (isVariable(i.op2.op)) {
            type2 = getVariable(i.op2.op).type;
        }

        if (type1 == VarType::VAR_FLOAT || type2 == VarType::VAR_FLOAT) {
            generateLoadVar(out, VarType::VAR_FLOAT, "%xmm0", i.op1);
            generateLoadVar(out, VarType::VAR_FLOAT, "%xmm1", i.op2);
            out << "\tcomisd %xmm1, %xmm0\n";
        }  else if(type1 == VarType::VAR_POINTER && (type2 == VarType::VAR_INTEGER || type2 == VarType::VAR_BYTE)) {
            generateLoadVar(out, type1, "%rax", i.op1);
            generateLoadVar(out, type2, "%rcx", i.op2);
            out << "\tcmpq %rcx, %rax\n";
        }
        else {
            generateLoadVar(out, VarType::VAR_INTEGER, "%rax", i.op1);
            generateLoadVar(out, VarType::VAR_INTEGER, "%rcx", i.op2);
            out << "\tcmpq %rcx, %rax\n";
        }
    }

    void Program::gen_mov(std::ostream &out, const Instruction &i) {
        if(isVariable(i.op1.op)) {
            Variable &v = getVariable(i.op1.op);
            if(isVariable(i.op2.op)) {
                Variable &v2 = getVariable(i.op2.op);
                if(v2.type != v.type) {
                    throw mx::Exception ("mov operand type mismatch. ");
                }
                switch(v.type) {
                    case VarType::VAR_INTEGER:
                        generateLoadVar(out, VarType::VAR_INTEGER, "%rax", i.op2);
                        out << "\tmovq %rax, " << getMangledName(i.op1) << "(%rip)\n";
                    break;
                    case VarType::VAR_POINTER:
                    case VarType::VAR_EXTERN:
                        generateLoadVar(out, v.type, "%rax", i.op2);
                        out << "\tmovq %rax, " << getMangledName(i.op1) << "(%rip)\n";
                    break;
                    case VarType::VAR_FLOAT:
                        generateLoadVar(out, VarType::VAR_FLOAT, "%xmm0", i.op2);
                        out << "\tmovsd %xmm0, " << getMangledName(i.op1) << "(%rip)\n";
                    break;
                    case VarType::VAR_STRING:
                        generateLoadVar(out, VarType::VAR_STRING, "%rax", i.op2);
                        out << "\tmovq %rax, " << getMangledName(i.op1) << "(%rip)\n";
                    break;
                    case VarType::VAR_BYTE:
                        generateLoadVar(out, VarType::VAR_BYTE, "%rax", i.op2);
                        out << "\tmovb %al, " << getMangledName(i.op1) << "(%rip)\n";
                    break;
                default:
                    throw mx::Exception("type not supported for mov instruction.");
                }
            } else {
                if(i.op2.type == OperandType::OP_CONSTANT) {
                    out << "\tmovq $" << i.op2.op << ", " << getMangledName(i.op1) <<  "(%rip)\n";
                }
            } 
        } else {
            throw mx::Exception("For mov command first operand: " + i.op1.op + " should be a Variable");
        }
    }

    void Program::gen_arth(std::ostream &out, std::string arth, const Instruction &i) {
         if(i.op3.op.empty()) {
            if(isVariable(i.op1.op)) {
                Variable &v = getVariable(i.op1.op);
                if(v.type == VarType::VAR_INTEGER) {
                    generateLoadVar(out, VarType::VAR_INTEGER, "%rax", i.op1);
                    generateLoadVar(out, VarType::VAR_INTEGER, "%rcx", i.op2);
                    if(arth == "mul")
                        arth = "imul";
                    out << "\t" << arth << "q %rcx, %rax\n";
                    out << "\tmovq %rax, " << getMangledName(i.op1) << "(%rip)\n";
                } else if(v.type == VarType::VAR_FLOAT) {
                    generateLoadVar(out, VarType::VAR_FLOAT, "%xmm0", i.op1);
                    generateLoadVar(out, VarType::VAR_FLOAT, "%xmm1", i.op2);
                    out << "\t" << arth << "sd %xmm1, %xmm0\n";
                    out << "\tmovsd %xmm0, " << getMangledName(i.op1) << "(%rip)\n";
                } else {
                    
                    throw mx::Exception("Variable type not impelmented for add function");
                }
            } else {
                throw mx::Exception("First argument: " + i.op1.op + " of add instruction must ba a variable.");
            }
         } else {
            if(isVariable(i.op1.op)) {
                Variable &v = getVariable(i.op1.op);
                if(v.type == VarType::VAR_INTEGER) {
                    generateLoadVar(out, VarType::VAR_INTEGER, "%rax", i.op2);
                    generateLoadVar(out, VarType::VAR_INTEGER, "%rcx", i.op3);
                    if(arth == "mul")
                        arth = "imul";
                    out << "\t" << arth << "q %rcx, %rax\n";
                    out << "\tmovq %rax, " << getMangledName(i.op1) << "(%rip)\n";
                } else if(v.type == VarType::VAR_FLOAT) {
                    generateLoadVar(out,VarType::VAR_FLOAT,"%xmm0", i.op2);
                    generateLoadVar(out,VarType::VAR_FLOAT, "%xmm1", i.op3);
                    out << "\t" << arth << "sd %xmm1, %xmm0\n";
                    out << "\tmovsd %xmm0, " << getMangledName(i.op1) << "(%rip)\n";
                } else {
                    throw mx::Exception("Variable type not impelmented for add function");
                }
            } else{
                throw mx::Exception("First argument of add instruction must be a variable.\n");
            }
         }
    }

    void Program::gen_exit(std::ostream &out, const Instruction &i) {
        if(!i.op1.op.empty()) {
            std::vector<Operand> opz;
            opz.push_back(i.op1);
            generateFunctionCall(out, "exit", opz);
        } else {
            throw mx::Exception("exit requires argument.\n");
        }
    }
}
