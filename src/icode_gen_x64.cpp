#include "mxvm/icode.hpp"

#include <algorithm>
#include <iostream>
#include <unordered_map>
#include <utility>

namespace mxvm {

    
    static unsigned x64_sp_mod16 = 0;
    extern size_t xmm_offset; 
    static int error_label_count = 0;   

    std::string Program::x64_getRegisterByIndex(int index, VarType type) {
        if (type == VarType::VAR_FLOAT) {
            if (index < 4) return "%xmm" + std::to_string(index);
            return "[stack]";
        }
        static const char* ireg[] = {"%rcx", "%rdx", "%r8", "%r9"};
        if (index < 4) return ireg[index];
        return "[stack]";
    }

    static inline bool is_stdio_name(const std::string &s) {
        return s == "stdin" || s == "stdout" || s == "stderr";
    }
    static inline int stdio_index(const std::string &s) {
        return s == "stdin" ? 0 : (s == "stdout" ? 1 : 2);
    }

    size_t Program::x64_reserve_call_area(std::ostream &out, size_t spill_bytes) {
        const size_t need = 32 + spill_bytes;
        const size_t aligned_need = (need + 15) & ~15;
        out << "\tsub $" << aligned_need << ", %rsp\n";
        x64_sp_mod16 = (x64_sp_mod16 + aligned_need) % 16;
        return aligned_need;
    }

    void Program::x64_release_call_area(std::ostream &out, size_t total) {
        out << "\tadd $" << total << ", %rsp\n";
        x64_sp_mod16 = (x64_sp_mod16 + (16 - (total % 16))) % 16;
    }
     void Program::x64_emit_iob_func(std::ostream &out, int index, const std::string &dstReg) {
        out << "\tmov $" << index << ", %ecx\n";
        size_t total = x64_reserve_call_area(out, 0);
        out << "\tcall __acrt_iob_func\n";
        x64_release_call_area(out, total);
        if (dstReg != "%rax") out << "\tmov %rax, " << dstReg << "\n";
    }

    int Program::x64_generateLoadVar(std::ostream &out, VarType type, std::string reg, const Operand &op) {
        int count = 0;
        if (isVariable(op.op)) {
            Variable &v = getVariable(op.op);

            if ((v.type == VarType::VAR_EXTERN || v.type == VarType::VAR_POINTER) && is_stdio_name(op.op)) {
                x64_emit_iob_func(out, stdio_index(op.op), reg);
                return 0;
            }

            switch (v.type) {
                case VarType::VAR_INTEGER:
                case VarType::VAR_POINTER:
                case VarType::VAR_EXTERN:
                    out << "\tmovq " << getMangledName(op) << "(%rip), " << reg << "\n";
                    break;
                case VarType::VAR_BYTE:
                    out << "\tmovzbq " << getMangledName(op) << "(%rip), " << reg << "\n";
                    break;
                case VarType::VAR_FLOAT:
                    out << "\tmovsd " << getMangledName(op) << "(%rip), " << reg << "\n";
                    count = 1;
                    break;
                case VarType::VAR_STRING:
                    out << "\tleaq " << getMangledName(op) << "(%rip), " << reg << "\n";
                    break;
                default: break;
            }
        } else {
            if (op.type == OperandType::OP_CONSTANT && type == VarType::VAR_INTEGER)
                out << "\tmovq $" << op.op << ", " << reg << "\n";
            else if (op.type == OperandType::OP_CONSTANT && type == VarType::VAR_FLOAT)
                throw mx::Exception("Constant to float not supported");
        }
        return count;
    }

    int Program::x64_generateLoadVar(std::ostream &out, int r, const Operand &op) {
        int count = 0;
        if (isVariable(op.op)) {
            Variable &v = getVariable(op.op);
            std::string reg = x64_getRegisterByIndex(r, v.type);

            if ((v.type == VarType::VAR_EXTERN || v.type == VarType::VAR_POINTER) && is_stdio_name(op.op)) {
                x64_emit_iob_func(out, stdio_index(op.op), reg);
                return 0;
            }

            switch (v.type) {
                case VarType::VAR_INTEGER:
                case VarType::VAR_POINTER:
                case VarType::VAR_EXTERN:
                    out << "\tmovq " << getMangledName(op) << "(%rip), " << reg << "\n";
                    break;
                case VarType::VAR_FLOAT:
                    out << "\tmovsd " << getMangledName(op) << "(%rip), " << reg << "\n";
                    count = 1;
                    break;
                case VarType::VAR_STRING:
                    out << "\tleaq " << getMangledName(op) << "(%rip), " << reg << "\n";
                    break;
                case VarType::VAR_BYTE:
                    out << "\tmovzbq " << getMangledName(op) << "(%rip), " << reg << "\n";
                    break;
                default: break;
            }
        } else {
            if (op.type == OperandType::OP_CONSTANT)
                out << "\tmovq $" << op.op << ", " << x64_getRegisterByIndex(r, VarType::VAR_INTEGER) << "\n";
        }
        return count;
    }

    
     bool isFunctionReturningOwnedPtr(const std::string& funcName) {
        static const std::unordered_set<std::string> funcs = {
            "inttostr",
            "floattostr",
        };
        return funcs.count(funcName) > 0;
    }
    
    void Program::x64_generateFunctionCall(std::ostream &out,
                                        const std::string &name,
                                        std::vector<Operand> &args) {
        xmm_offset = 0;
        const size_t stack_args = (args.size() > 4) ? (args.size() - 4) : 0;
        const size_t spill_bytes = stack_args * 8;
        size_t frame = x64_reserve_call_area(out, spill_bytes);

        static const char* GPR[4] = {"%rcx", "%rdx", "%r8", "%r9"};
        unsigned xmm_count = 0;
        unsigned int_count = 0;

        for (size_t i = 0; i < args.size(); ++i) {
            VarType t = VarType::VAR_INTEGER;
            if (isVariable(args[i].op)) t = getVariable(args[i].op).type;

            if (t == VarType::VAR_FLOAT) {
                if (xmm_count < 4) {
                    std::string xr = "%xmm" + std::to_string(xmm_count);
                    x64_generateLoadVar(out, VarType::VAR_FLOAT, xr, args[i]);
                    ++xmm_count;
                } else {
                    const size_t off = 32 + 8 * (int_count - 4);
                    x64_generateLoadVar(out, VarType::VAR_FLOAT, "%xmm7", args[i]);
                    out << "\tmovsd %xmm7, " << off << "(%rsp)\n";
                    int_count++; 
                }
            } else {
                if (int_count < 4) {
                    x64_generateLoadVar(out, VarType::VAR_INTEGER, GPR[int_count], args[i]);
                    ++int_count;
                } else {
                    const size_t off = 32 + 8 * (int_count - 4);
                    x64_generateLoadVar(out, VarType::VAR_INTEGER, "%rax", args[i]);
                    out << "\tmovq %rax, " << off << "(%rsp)\n";
                    int_count++;
                }
            }
        }

        if (name == "printf") {
            out << "\tmov $" << std::min<unsigned>(xmm_count, 4) << ", %al\n";
        } else {
            out << "\txor %eax, %eax\n";
        }

        out << "\tcall " << name << "\n";
        x64_release_call_area(out, frame);
    }


    void Program::x64_generateInvokeCall(std::ostream &out, std::vector<Operand> &op) {
        if (op.empty() || op[0].op.empty()) throw mx::Exception("invoke requires instruction name");
        std::string n = op[0].op;
        std::vector<Operand> a;
        for (size_t i = 1; i < op.size(); ++i) if (!op[i].op.empty()) a.push_back(op[i]);

        if (isFunctionReturningOwnedPtr(n)) {
            this->last_call_returns_owned_ptr = true;
        } else {
            this->last_call_returns_owned_ptr = false;
        }

        x64_generateFunctionCall(out, n, a);
    }

    void Program::x64_generateCode(const Platform&, bool, std::ostream &out) {
        this->add_standard();
        std::unordered_map<int,std::string> labels_;
        for (auto &l : labels) labels_[l.second.first] = l.first;

        
        bool uses_std_module = false;
        for(const auto &e : external) {
            if(e.mod == "std") {
                uses_std_module = true;
                break;
            }
        }

        out << ".section .data\n";

        std::vector<std::string> var_names;
        for (auto &v : vars) var_names.push_back(v.first);
        std::sort(var_names.begin(), var_names.end());

        for (auto &v : var_names) {
            auto varx = getVariable(v);
            const std::string nm = getMangledName(v);
            if (varx.type == VarType::VAR_INTEGER) { out << "\t" << nm << ": .quad " << vars[v].var_value.int_value << "\n"; continue; }
            if (varx.type == VarType::VAR_FLOAT)   { out << "\t" << nm << ": .double " << vars[v].var_value.float_value << "\n"; continue; }
            if (varx.type == VarType::VAR_BYTE)    { out << "\t" << nm << ": .byte " << vars[v].var_value.int_value << "\n"; }
        }

        for (auto &v : var_names) {
            auto varx = getVariable(v);
            if (varx.type == VarType::VAR_STRING && varx.var_value.buffer_size == 0) {
                const std::string nm = getMangledName(v);
                out << "\t" << nm << ": .asciz \"" << escapeNewLines(vars[v].var_value.str_value) << "\"\n";
            }
        }

        if (base != nullptr) {
            for (auto &v : var_names) {
                auto varx = getVariable(v);
                if (varx.is_global) {
                    switch (varx.type) {
                        case VarType::VAR_BYTE:
                        case VarType::VAR_FLOAT:
                        case VarType::VAR_INTEGER:
                            out << "\t.globl " << getMangledName(v) << "\n";
                        break;
                        default: break;
                    }
                }
            }
        }

        out << ".section .bss\n";
        for (auto &v : var_names) {
            const std::string nm = getMangledName(v);
            auto varx = getVariable(v);
            if (varx.type == VarType::VAR_POINTER) out << "\t.comm " << nm << ", 8\n";
            else if (varx.type == VarType::VAR_STRING && varx.var_value.buffer_size > 0) out << "\t.comm " << nm << ", " << varx.var_value.buffer_size << "\n";
        }
        if (object) {
            for (auto &v : var_names) {
                auto varx = getVariable(v);
                if (varx.is_global && (varx.type == VarType::VAR_STRING || varx.type == VarType::VAR_POINTER))
                    out << "\t.globl " << getMangledName(v) << "\n";
            }
        }

        out << ".section .text\n";

        out << "\t.extern __acrt_iob_func\n";

        
        for (auto &lbl : labels)
            if (lbl.second.second == true) out << "\t.globl " << name + "_" + lbl.first << "\n";

        std::sort(external.begin(), external.end(), [](const ExternalFunction &a, const ExternalFunction &b){
            if (a.mod == b.mod) return a.name < b.name;
            return a.mod < b.mod;
        });
        for (auto &e : external)
            out << "\t.extern " << (e.module ? e.name : e.mod + "_" + e.name) << "\n";

         if(!this->object) {
            out << "\t.p2align 4, 0x90\n";
            out << ".globl main\n";
            out << "main:\n";
            out << "\tpush %rbp\n";
            out << "\tmov %rsp, %rbp\n";
            x64_sp_mod16 = 8;
            if(uses_std_module) {
                out << "\t# Set up program arguments for std module\n";
                out << "\tmov %rcx, %r12\n";
                out << "\tmov %rdx, %r13\n";
                size_t total = x64_reserve_call_area(out, 0);
                out << "\tcall set_program_args\n";
                x64_release_call_area(out, total);
            }
        }

        bool done_found = false;

        for (size_t i = 0; i < inc.size(); ++i) {
            const Instruction &instr = inc[i];
            for (auto l : labels) {
                if (l.second.first == i && l.second.second) {
                    out << "\t.p2align 4, 0x90\n";
                    out << name + "_" + l.first << ":\n";
                    out << "\tpush %rbp\n";
                    out << "\tmov %rsp, %rbp\n";
                     x64_sp_mod16 = 8 ; 
                    break;
                }
            }
            for (auto l : labels) {
                if (l.second.first == i && !l.second.second) out << "." << l.first << ":\n";
            }
            if (instr.instruction == DONE) done_found = true;
            x64_generateInstruction(out, instr);
        }

    #ifndef __EMSCRIPTEN__
        if (this->object == false && done_found == false) throw mx::Exception("Program missing done to signal completion.\n");
    #endif

        out << "\n\n";

        std::string mainFunc = " Object";
        if (root_name == name) mainFunc = " Program";
        std::cout << Col("MXVM: Compiled: ", mx::Color::BRIGHT_BLUE) << name << ".s" << mainFunc << Col(" platform: ", mx::Color::BRIGHT_CYAN) << "Windows" << "\n";
    }


    void Program::x64_gen_done(std::ostream &out, const Instruction&) {
        bool uses_std_module = false;
        for(const auto &e : external) {
            if(e.mod == "std") {
                uses_std_module = true;
                break;
            }
        }

        if(uses_std_module && !this->object) {
            size_t total = x64_reserve_call_area(out, 0);
            out << "\tcall free_program_args\n";
            x64_release_call_area(out, total);
        }

        out << "\txor %eax, %eax\n";
        out << "\tleave\n";
        out << "\tret\n";
    }

    void Program::x64_gen_ret(std::ostream &out, const Instruction&) {
        out << "\tleave\n";
        out << "\tret\n";
    }
    void Program::x64_gen_return(std::ostream &out, const Instruction &i) {
            if (isVariable(i.op1.op)) {
                if (this->last_call_returns_owned_ptr) {
                    Variable &v = getVariable(i.op1.op);
                    v.var_value.owns = true;
                    this->last_call_returns_owned_ptr = false; 
                }
                out << "\tmovq %rax, " << getMangledName(i.op1) << "(%rip)\n";
            }
            else throw mx::Exception("return requires variable");
        }

    void Program::x64_gen_neg(std::ostream &out, const Instruction &i) {
        if (isVariable(i.op1.op)) {
            Variable &v = getVariable(i.op1.op);
            if (v.type == VarType::VAR_INTEGER) {
                x64_generateLoadVar(out, VarType::VAR_INTEGER, "%rcx", i.op1);
                out << "\tnegq %rcx\n";
                out << "\tmovq %rcx, " << getMangledName(i.op1) << "(%rip)\n";
            } else if (v.type == VarType::VAR_FLOAT) {
                x64_generateLoadVar(out, VarType::VAR_FLOAT, "%xmm0", i.op1);
                out << "\txorpd %xmm1, %xmm1\n";
                out << "\tsubsd %xmm0, %xmm1\n";
                out << "\tmovsd %xmm1, " << getMangledName(i.op1) << "(%rip)\n";
            } else throw mx::Exception("neg requires float or integer");
        } else throw mx::Exception("neg requires variable");
    }

    void Program::x64_generateInstruction(std::ostream &out, const Instruction &i) {
        
        if (i.instruction < JMP || i.instruction > JNS) {
            last_cmp_type = CMP_NONE;
        }
        switch (i.instruction) {
            case ADD: x64_gen_arth(out, "add", i); break;
            case SUB: x64_gen_arth(out, "sub", i); break;
            case MUL: x64_gen_arth(out, "mul", i); break;
            case PRINT: x64_gen_print(out, i); break;
            case EXIT: x64_gen_exit(out, i); break;
            case MOV: x64_gen_mov(out, i); break;
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
            case JB: x64_gen_jmp(out, i); break;
            case CMP: x64_gen_cmp(out, i); break;
            case ALLOC: x64_gen_alloc(out, i); break;
            case FREE: x64_gen_free(out, i); break;
            case LOAD: x64_gen_load(out, i); break;
            case STORE: x64_gen_store(out, i); break;
            case RET: x64_gen_ret(out, i); break;
            case CALL: x64_gen_call(out, i); break;
            case DONE: x64_gen_done(out, i); break;
            case AND: x64_gen_bitop(out, "and", i); break;
            case OR:  x64_gen_bitop(out, "or",  i); break;
            case XOR: x64_gen_bitop(out, "xor", i); break;
            case NOT: x64_gen_not(out, i); break;
            case DIV: x64_gen_div(out, i); break;
            case MOD: x64_gen_mod(out, i); break;
            case PUSH: x64_gen_push(out, i); break;
            case POP:  x64_gen_pop(out, i); break;
            case STACK_LOAD: x64_gen_stack_load(out, i); break;
            case STACK_STORE: x64_gen_stack_store(out, i); break;
            case STACK_SUB: x64_gen_stack_sub(out, i); break;
            case GETLINE: x64_gen_getline(out, i); break;
            case TO_INT: x64_gen_to_int(out, i); break;
            case TO_FLOAT: x64_gen_to_float(out, i); break;
            case INVOKE: x64_gen_invoke(out, i); break;
            case RETURN: x64_gen_return(out, i); break;
            case NEG: x64_gen_neg(out, i); break;
        case FCMP:
            x64_gen_fcmp(out, i);
            break;
        case JAE:
            x64_gen_jae(out, i);
            break;
        case JBE:
            x64_gen_jbe(out, i);
            break;
        case JC:
            x64_gen_jc(out, i);
            break;
        case JNC:
            x64_gen_jnc(out, i);
            break;
        case JP:
            x64_gen_jp(out, i);
            break;
        case JNP:
            x64_gen_jnp(out, i);
            break;
        case JO:
            x64_gen_jo(out, i);
            break;
        case JNO:
            x64_gen_jno(out, i);
            break;
        case JS:
            x64_gen_js(out, i);
            break;
        case JNS:
            x64_gen_jns(out, i);
            break;
            default: throw mx::Exception("Invalid or unsupported instruction");
        }
    }

    void Program::x64_gen_invoke(std::ostream &out, const Instruction &i) {
        if (i.op1.op.empty()) throw mx::Exception("invoke requires instruction name");
        std::vector<Operand> op;
        op.push_back(i.op1);
        if (!i.op2.op.empty()) op.push_back(i.op2);
        if (!i.op3.op.empty()) op.push_back(i.op3);
        for (const auto &vx : i.vop) if (!vx.op.empty()) op.push_back(vx);
        x64_generateInvokeCall(out, op);
    }

    void Program::x64_gen_call(std::ostream &out, const Instruction &i) {
            if (!isFunctionValid(i.op1.op)) throw mx::Exception("Function not found");
            size_t total = x64_reserve_call_area(out, 0);
            out << "\tcall " << getMangledName(i.op1) << "\n";
            x64_release_call_area(out, total);
        }
        void Program::x64_gen_alloc(std::ostream &out, const Instruction &i) {
        if (!isVariable(i.op1.op)) throw mx::Exception("ALLOC destination must be a variable");
        Variable &v = getVariable(i.op1.op);
        if (v.type != VarType::VAR_POINTER) throw mx::Exception("ALLOC destination must be a pointer");

        if (!i.op3.op.empty()) {
            if (isVariable(i.op3.op)) {
                out << "\tmovq " << getMangledName(i.op3) << "(%rip), %rcx\n";
            } else {
                out << "\tmovq $" << i.op3.op << ", %rcx\n";
            }
        } else {
            out << "\tmovq $1, %rcx\n";
        }
        
        if (!i.op2.op.empty()) {
            if (isVariable(i.op2.op)) {
                out << "\tmovq " << getMangledName(i.op2) << "(%rip), %rdx\n";
            } else {
                out << "\tmovq $" << i.op2.op << ", %rdx\n";
            }
        } else {
            out << "\tmovq $8, %rdx\n";
        }

        size_t total = x64_reserve_call_area(out, 0);
        out << "\tcall calloc\n";
        x64_release_call_area(out, total);

        out << "\ttest %rax, %rax\n";
        out << "\tjz .alloc_failed_" << error_label_count << "\n";
        
        out << "\tmovq %rax, " << getMangledName(i.op1) << "(%rip)\n";
        getVariable(i.op1.op).var_value.owns = true;
        
        out << "\tjmp .alloc_done_" << error_label_count << "\n";
        out << ".alloc_failed_" << error_label_count << ":\n";
        out << "\t# Handle allocation failure\n";
        out << "\tmovq $0, " << getMangledName(i.op1) << "(%rip)\n";
        out << ".alloc_done_" << error_label_count << ":\n";
        
        error_label_count++;
    }

    void Program::x64_gen_free(std::ostream &out, const Instruction &i) {
        if (!isVariable(i.op1.op)) throw mx::Exception("FREE argument must be a variable");
        Variable &v = getVariable(i.op1.op);
        if (v.type != VarType::VAR_POINTER) throw mx::Exception("FREE argument must be a pointer");
        if (!v.var_value.owns) throw mx::Exception("FREE on non-owned pointer: " + v.var_name);

        out << "\tmovq " << getMangledName(i.op1) << "(%rip), %rcx\n";
        out << "\ttest %rcx, %rcx\n";
        out << "\tjz 1f\n";
        size_t total = x64_reserve_call_area(out, 0);
        out << "\tcall free\n";
        x64_release_call_area(out, total);
        out << "\tmovq $0, " << getMangledName(i.op1) << "(%rip)\n";
        out << "1:\n";
        v.var_value.owns = false;
    }

    void Program::x64_gen_mov(std::ostream &out, const Instruction &i) {
            if (!isVariable(i.op1.op)) {
                throw mx::Exception("MOV first operand must be a variable");
            }
            Variable &dest = getVariable(i.op1.op);

            if (isVariable(i.op2.op)) {
                Variable &src = getVariable(i.op2.op);

                if (dest.type == src.type) {
                    
                    if (dest.type == VarType::VAR_FLOAT) {
                        x64_generateLoadVar(out, VarType::VAR_FLOAT, "%xmm0", i.op2);
                        out << "\tmovsd %xmm0, " << getMangledName(i.op1) << "(%rip)\n";
                    } else if (dest.type == VarType::VAR_BYTE) {
                        x64_generateLoadVar(out, VarType::VAR_BYTE, "%rax", i.op2);
                        out << "\tmovb %al, " << getMangledName(i.op1) << "(%rip)\n";
                    } else { 
                        x64_generateLoadVar(out, dest.type, "%rax", i.op2);
                        out << "\tmovq %rax, " << getMangledName(i.op1) << "(%rip)\n";
                    }
                } else {
                
                    if (dest.type == VarType::VAR_FLOAT) { 
                        if (src.type == VarType::VAR_INTEGER || src.type == VarType::VAR_BYTE) {
                            x64_generateLoadVar(out, src.type, "%rax", i.op2);
                            out << "\tcvtsi2sdq %rax, %xmm0\n";
                            out << "\tmovsd %xmm0, " << getMangledName(i.op1) << "(%rip)\n";
                        } else {
                            throw mx::Exception("MOV: unsupported conversion to float");
                        }
                    } else if (src.type == VarType::VAR_FLOAT) { 
                        if (dest.type == VarType::VAR_INTEGER || dest.type == VarType::VAR_BYTE) {
                            x64_generateLoadVar(out, VarType::VAR_FLOAT, "%xmm0", i.op2);
                            out << "\tcvttsd2siq %xmm0, %rax\n"; 
                            if (dest.type == VarType::VAR_BYTE) {
                                out << "\tmovb %al, " << getMangledName(i.op1) << "(%rip)\n";
                            } else {
                                out << "\tmovq %rax, " << getMangledName(i.op1) << "(%rip)\n";
                            }
                        } else {
                            throw mx::Exception("MOV: unsupported conversion from float");
                        }
                    } else if (dest.type == VarType::VAR_POINTER && src.type == VarType::VAR_STRING) {
                        out << "\tleaq " << getMangledName(i.op2.op) << "(%rip), %rax\n";
                        out << "\tmovq %rax, " << getMangledName(i.op1.op) << "(%rip)\n";
                        getVariable(i.op1.op).var_value.owns = false;
                    } else { 
                        x64_generateLoadVar(out, src.type, "%rax", i.op2); 
                        if (dest.type == VarType::VAR_BYTE) {
                            out << "\tmovb %al, " << getMangledName(i.op1) << "(%rip)\n";
                        } else {
                            out << "\tmovq %rax, " << getMangledName(i.op1) << "(%rip)\n";
                        }
                    }
                }
            } else {
                if (dest.type == VarType::VAR_FLOAT) {
                    double val = std::stod(i.op2.op);
                    uint64_t bits;
                    std::memcpy(&bits, &val, sizeof(bits));
                    out << "\tmovq $" << bits << ", %rax\n";
                    out << "\tmovq %rax, " << getMangledName(i.op1) << "(%rip)\n";
                } else if (dest.type == VarType::VAR_BYTE) {
                    out << "\tmovb $" << i.op2.op << ", " << getMangledName(i.op1) << "(%rip)\n";
                } else { 
                    out << "\tmovq $" << i.op2.op << ", " << getMangledName(i.op1) << "(%rip)\n";
                }
            }
        }

    
    void Program::x64_gen_load(std::ostream &out, const Instruction &i) {
        if (!isVariable(i.op1.op)) throw mx::Exception("LOAD dest must be variable");
        Variable &dest = getVariable(i.op1.op);
        if (!isVariable(i.op2.op)) throw mx::Exception("LOAD source must be pointer variable");
        Variable &ptrVar = getVariable(i.op2.op);

        if (ptrVar.type != VarType::VAR_POINTER && 
            !(ptrVar.type == VarType::VAR_STRING && ptrVar.var_value.buffer_size > 0)) {
            throw mx::Exception("LOAD: invalid base type");
        }

        
        if (ptrVar.type == VarType::VAR_POINTER) {
            out << "\tmovq " << getMangledName(i.op2) << "(%rip), %rax\n";
        } else {
            out << "\tleaq " << getMangledName(i.op2) << "(%rip), %rax\n";
        }

        out << "\ttest %rax, %rax\n";
        out << "\tjz .null_ptr_error_" << error_label_count << "\n";

        if (!i.op3.op.empty()) {
            if (isVariable(i.op3.op)) {
                out << "\tmovq " << getMangledName(i.op3) << "(%rip), %rcx\n";
            } else {
                out << "\tmovq $" << i.op3.op << ", %rcx\n";
            }
        } else {
            out << "\txorq %rcx, %rcx\n";
        }

        if (!i.vop.empty() && !i.vop[0].op.empty()) {
            const std::string &sv = i.vop[0].op;
            if (isVariable(sv)) {
                out << "\tmovq " << getMangledName(sv) << "(%rip), %r8\n";
                out << "\timulq %r8, %rcx\n";
            } else {
                long long val = std::stoll(sv);
                if (val == 1) {
                } else if (val == 2) {
                    out << "\tshlq $1, %rcx\n";
                } else if (val == 4) {
                    out << "\tshlq $2, %rcx\n";
                } else if (val == 8) {
                    out << "\tshlq $3, %rcx\n";
                } else {
                    out << "\timulq $" << sv << ", %rcx\n";
                }
            }
        }

        out << "\taddq %rcx, %rax\n";

        switch (dest.type) {
            case VarType::VAR_INTEGER:
            case VarType::VAR_POINTER:
            case VarType::VAR_EXTERN:
                out << "\tmovq (%rax), %rdx\n";
                out << "\tmovq %rdx, " << getMangledName(i.op1) << "(%rip)\n";
                break;
            case VarType::VAR_FLOAT:
                out << "\tmovsd (%rax), %xmm0\n";
                out << "\tmovsd %xmm0, " << getMangledName(i.op1) << "(%rip)\n";
                break;
            case VarType::VAR_BYTE:
                out << "\tmovzbq (%rax), %rdx\n";
                out << "\tmovq %rdx, " << getMangledName(i.op1) << "(%rip)\n";
                break;
            default:
                throw mx::Exception("LOAD: unsupported destination type");
        }

        out << "\tjmp .load_done_" << error_label_count << "\n";
        out << ".null_ptr_error_" << error_label_count << ":\n";
        out << "\t# Handle null pointer error\n";
        out << ".load_done_" << error_label_count << ":\n";

        error_label_count++;
    }
    

    void Program::x64_gen_store(std::ostream &out, const Instruction &i) {
        if (!isVariable(i.op2.op)) {
            throw mx::Exception("STORE destination must be a variable");
        }
        
        Variable& ptrVar = getVariable(i.op2.op);
        if (ptrVar.type != VarType::VAR_POINTER && 
            !(ptrVar.type == VarType::VAR_STRING && ptrVar.var_value.buffer_size > 0)) {
            throw mx::Exception("STORE destination must be a pointer or string buffer");
        }
        
        
        if (ptrVar.type == VarType::VAR_POINTER) {
            out << "\tmovq " << getMangledName(i.op2) << "(%rip), %rcx\n";
        } else { 
            out << "\tleaq " << getMangledName(i.op2) << "(%rip), %rcx\n";
        }
     
        out << "\ttest %rcx, %rcx\n";
        out << "\tjz .null_ptr_error_" << error_label_count << "\n";
        
        if (!i.op3.op.empty()) {
            if (isVariable(i.op3.op)) {
                out << "\tmovq " << getMangledName(i.op3) << "(%rip), %rdx\n";
            } else {
                out << "\tmovq $" << i.op3.op << ", %rdx\n";
            }
        } else {
            out << "\txorq %rdx, %rdx\n";
        }
        
        if (!i.vop.empty() && !i.vop[0].op.empty()) {
            if (isVariable(i.vop[0].op)) {
                out << "\tmovq " << getMangledName(i.vop[0]) << "(%rip), %r8\n";
                out << "\timulq %r8, %rdx\n";
            } else {
                size_t stride = static_cast<size_t>(std::stoll(i.vop[0].op, nullptr, 0));
                if (stride > 1) {
                    out << "\timulq $" << stride << ", %rdx\n";
                }
            }
        }
        
        out << "\taddq %rdx, %rcx\n";
        
        if (isVariable(i.op1.op)) {
            Variable& src = getVariable(i.op1.op);
            switch (src.type) {
                case VarType::VAR_INTEGER:
                case VarType::VAR_POINTER:
                case VarType::VAR_EXTERN:
                    out << "\tmovq " << getMangledName(i.op1) << "(%rip), %rax\n";
                    out << "\tmovq %rax, (%rcx)\n";
                    break;
                case VarType::VAR_STRING:
                    out << "\tleaq " << getMangledName(i.op1) << "(%rip), %rax\n";
                    out << "\tmovq %rax, (%rcx)\n";
                    break;
                case VarType::VAR_BYTE:
                    out << "\tmovzbq " << getMangledName(i.op1) << "(%rip), %rax\n";
                    out << "\tmovb %al, (%rcx)\n";
                    break;
                case VarType::VAR_FLOAT:
                    out << "\tmovsd " << getMangledName(i.op1) << "(%rip), %xmm0\n";
                    out << "\tmovsd %xmm0, (%rcx)\n";
                    break;
                default:
                    throw mx::Exception("STORE: unsupported source type");
            }
        } else {
            if (i.op1.op.find('.') != std::string::npos || 
                i.op1.op.find('e') != std::string::npos || 
                i.op1.op.find('E') != std::string::npos) {
                double val = std::stod(i.op1.op);
                uint64_t bits;
                memcpy(&bits, &val, sizeof(bits));
                out << "\tmovq $" << bits << ", %rax\n";
                out << "\tmovq %rax, (%rcx)\n";
            } else {
                out << "\tmovq $" << i.op1.op << ", %rax\n";
                out << "\tmovq %rax, (%rcx)\n";
            }
        }
        
        out << "\tjmp .store_done_" << error_label_count << "\n";
        out << ".null_ptr_error_" << error_label_count << ":\n";
        out << "\t# Handle null pointer error\n";
        out << ".store_done_" << error_label_count << ":\n";
        
        error_label_count++;
    }    
    void Program::x64_gen_to_int(std::ostream &out, const Instruction &i) {
        out << "\tleaq " << getMangledName(i.op2) << "(%rip), %rcx\n";
        size_t total = x64_reserve_call_area(out, 0);
        out << "\tcall atol\n";
        x64_release_call_area(out, total);
        out << "\tmovq %rax, " << getMangledName(i.op1) << "(%rip)\n";
    }

    void Program::x64_gen_to_float(std::ostream &out, const Instruction &i) {
        if (!isVariable(i.op1.op) || !isVariable(i.op2.op)) {
            throw mx::Exception("TO_FLOAT requires variable operands");
        }
        x64_generateLoadVar(out, VarType::VAR_INTEGER, "%rax", i.op2);
        out << "\tcvtsi2sdq %rax, %xmm0\n";
        out << "\tmovsd %xmm0, " << getMangledName(i.op1) << "(%rip)\n";
    }

    void Program::x64_gen_div(std::ostream &out, const Instruction &i) {
        if (i.op3.op.empty()) {
            if (isVariable(i.op1.op)) {
                Variable &v = getVariable(i.op1.op);
                if (v.type == VarType::VAR_INTEGER) {
                    x64_generateLoadVar(out, VarType::VAR_INTEGER, "%rax", i.op1);
                    x64_generateLoadVar(out, VarType::VAR_INTEGER, "%rcx", i.op2);
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
                    x64_generateLoadVar(out, VarType::VAR_FLOAT, "%xmm0", i.op1);
                    x64_generateLoadVar(out, VarType::VAR_FLOAT, "%xmm1", i.op2);
                    out << "\txorpd %xmm2, %xmm2\n";
                    out << "\tucomisd %xmm2, %xmm1\n";
                    out << "\tje 1f\n";
                    out << "\tdivsd %xmm1, %xmm0\n";
                    out << "\tjmp 2f\n";
                    out << "1:\n";
                    out << "\txorpd %xmm0, %xmm0\n";
                    out << "2:\n";
                    out << "\tmovsd %xmm0, " << getMangledName(i.op1) << "(%rip)\n";
                } else throw mx::Exception("DIV unsupported type");
            } else throw mx::Exception("DIV: first must be variable");
        } else {
            if (isVariable(i.op1.op)) {
                Variable &v = getVariable(i.op1.op);
                if (v.type == VarType::VAR_INTEGER) {
                    x64_generateLoadVar(out, VarType::VAR_INTEGER, "%rax", i.op2);
                    x64_generateLoadVar(out, VarType::VAR_INTEGER, "%rcx", i.op3);
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
                    x64_generateLoadVar(out, VarType::VAR_FLOAT, "%xmm0", i.op2);
                    x64_generateLoadVar(out, VarType::VAR_FLOAT, "%xmm1", i.op3);
                    out << "\txorpd %xmm2, %xmm2\n";
                    out << "\tucomisd %xmm2, %xmm1\n";
                    out << "\tje 1f\n";
                    out << "\tdivsd %xmm1, %xmm0\n";
                    out << "\tjmp 2f\n";
                    out << "1:\n";
                    out << "\txorpd %xmm0, %xmm0\n";
                    out << "2:\n";
                    out << "\tmovsd %xmm0, " << getMangledName(i.op1) << "(%rip)\n";
                } else throw mx::Exception("DIV unsupported type");
            } else throw mx::Exception("DIV: first must be variable");
        }
    }

    void Program::x64_gen_mod(std::ostream &out, const Instruction &i) {
        if (i.op3.op.empty()) {
            if (isVariable(i.op1.op)) {
                Variable &v = getVariable(i.op1.op);
                if (v.type == VarType::VAR_INTEGER) {
                    x64_generateLoadVar(out, VarType::VAR_INTEGER, "%rax", i.op1);
                    x64_generateLoadVar(out, VarType::VAR_INTEGER, "%rcx", i.op2);
                    out << "\tcmpq $0, %rcx\n";
                    out << "\tje 1f\n";
                    out << "\tcqto\n";
                    out << "\tidivq %rcx\n";
                    out << "\tjmp 2f\n";
                    out << "1:\n";
                    out << "\txor %edx, %edx\n";
                    out << "2:\n";
                    out << "\tmovq %rdx, " << getMangledName(i.op1) << "(%rip)\n";
                } else throw mx::Exception("MOD int only");
            } else throw mx::Exception("MOD: first must be variable");
        } else {
            if (isVariable(i.op1.op)) {
                Variable &v = getVariable(i.op1.op);
                if (v.type == VarType::VAR_INTEGER) {
                    x64_generateLoadVar(out, VarType::VAR_INTEGER, "%rax", i.op2);
                    x64_generateLoadVar(out, VarType::VAR_INTEGER, "%rcx", i.op3);
                    out << "\tcmpq $0, %rcx\n";
                    out << "\tje 1f\n";
                    out << "\tcqto\n";
                    out << "\tidivq %rcx\n";
                    out << "\tjmp 2f\n";
                    out << "1:\n";
                    out << "\txor %edx, %edx\n";
                    out << "2:\n";
                    out << "\tmovq %rdx, " << getMangledName(i.op1) << "(%rip)\n";
                } else throw mx::Exception("MOD int only");
            } else throw mx::Exception("MOD: first must be variable");
        }
    }

    void Program::x64_gen_getline(std::ostream &out, const Instruction &i) {
        if (!isVariable(i.op1.op)) throw mx::Exception("GETLINE: dest must be var");
        Variable &dest = getVariable(i.op1.op);
        if (dest.type != VarType::VAR_STRING || dest.var_value.buffer_size == 0)
            throw mx::Exception("GETLINE: needs string buffer");

        
        out << "\txor %ecx, %ecx\n";
        size_t t0 = x64_reserve_call_area(out, 0);
        out << "\tcall __acrt_iob_func\n";
        x64_release_call_area(out, t0);
        out << "\tmov %rax, %r8\n";

        
        out << "\tleaq " << getMangledName(i.op1) << "(%rip), %rcx\n";
        out << "\tmovq $" << dest.var_value.buffer_size << ", %rdx\n";
        size_t total = x64_reserve_call_area(out, 0);
        out << "\tcall fgets\n";
        x64_release_call_area(out, total);

        
        static size_t over_count = 0;
        out << "\ttest %rax, %rax\n";
        out << "\tje .over" << over_count << "\n";

        
        out << "\tleaq " << getMangledName(i.op1) << "(%rip), %rcx\n";
        size_t tlen = x64_reserve_call_area(out, 0);
        out << "\tcall strlen\n";
        x64_release_call_area(out, tlen);

        out << "\tmov %rax, %rcx\n";
        out << "\tcmp $0, %rax\n";
        out << "\tje .over" << over_count << "\n";
        out << "\tsub $1, %rcx\n";
        out << "\tleaq " << getMangledName(i.op1) << "(%rip), %rdi\n";
        out << "\tmovb $0, (%rdi, %rcx, 1)\n";
        out << ".over" << over_count++ << ":\n";
    }

    void Program::x64_gen_bitop(std::ostream &out, const std::string &opc, const Instruction &i) {
        if (i.op3.op.empty()) {
            if (isVariable(i.op1.op)) {
                Variable &v = getVariable(i.op1.op);
                if (v.type == VarType::VAR_INTEGER) {
                    x64_generateLoadVar(out, VarType::VAR_INTEGER, "%rax", i.op1);
                    x64_generateLoadVar(out, VarType::VAR_INTEGER, "%rcx", i.op2);
                    out << "\t" << opc << "q %rcx, %rax\n";
                    out << "\tmovq %rax, " << getMangledName(i.op1) << "(%rip)\n";
                } else throw mx::Exception("bitop int only");
            } else throw mx::Exception("bitop first must be variable");
        } else {
            if (isVariable(i.op1.op)) {
                Variable &v = getVariable(i.op1.op);
                if (v.type == VarType::VAR_INTEGER) {
                    x64_generateLoadVar(out, VarType::VAR_INTEGER, "%rax", i.op2);
                    x64_generateLoadVar(out, VarType::VAR_INTEGER, "%rcx", i.op3);
                    out << "\t" << opc << "q %rcx, %rax\n";
                    out << "\tmovq %rax, " << getMangledName(i.op1) << "(%rip)\n";
                } else throw mx::Exception("bitop int only");
            } else throw mx::Exception("bitop first must be variable");
        }
    }

    void Program::x64_gen_not(std::ostream &out, const Instruction &i) {
        if (!i.op1.op.empty()) {
            if (isVariable(i.op1.op)) {
                Variable &v = getVariable(i.op1.op);
                if (v.type != VarType::VAR_INTEGER) throw mx::Exception("NOT int only");
                x64_generateLoadVar(out, VarType::VAR_INTEGER, "%rax", i.op1);
                out << "\ttestq %rax, %rax\n";
                out << "\tsete %al\n";
                out << "\tmovzbq %al, %rax\n";
                out << "\tmovq %rax, " << getMangledName(i.op1) << "(%rip)\n";
            } else throw mx::Exception("NOT requires variable");
        } else throw mx::Exception("NOT requires operand");
    }

    void Program::x64_gen_push(std::ostream &out, const Instruction &i) {
        if (isVariable(i.op1.op)) {
            Variable &v = getVariable(i.op1.op);
            if (v.type == VarType::VAR_INTEGER || v.type == VarType::VAR_POINTER || v.type == VarType::VAR_EXTERN) {
                out << "\tmovq " << getMangledName(i.op1) << "(%rip), %rax\n";
                out << "\tpushq %rax\n";
                x64_sp_mod16 ^= 8;
            } else if (v.type == VarType::VAR_STRING) {
                out << "\tleaq " << getMangledName(i.op1) << "(%rip), %rax\n";
                out << "\tpushq %rax\n";
                x64_sp_mod16 ^= 8;
            } else {
                throw mx::Exception("PUSH supports int/pointer/string");
            }
        } else if (i.op1.type == OperandType::OP_CONSTANT) {
            out << "\tmovq $" << i.op1.op << ", %rax\n";
            out << "\tpushq %rax\n";
            x64_sp_mod16 ^= 8;
        } else {
            throw mx::Exception("PUSH requires var or const");
        }
    }

    void Program::x64_gen_pop(std::ostream &out, const Instruction &i) {
        if (!isVariable(i.op1.op)) throw mx::Exception("POP dest must be a variable");
        Variable &v = getVariable(i.op1.op);
        if (v.type == VarType::VAR_INTEGER || v.type == VarType::VAR_POINTER || v.type == VarType::VAR_EXTERN) {
            out << "\tpopq %rax\n";
            x64_sp_mod16 ^= 8;
            out << "\tmovq %rax, " << getMangledName(i.op1) << "(%rip)\n";
        } else {
            throw mx::Exception("POP supports int/pointer");
        }
    }

    void Program::x64_gen_stack_sub(std::ostream &out, const Instruction &i) {
        if (!i.op1.op.empty()) {
            if (isVariable(i.op1.op))      out << "\tmovq " << getMangledName(i.op1) << "(%rip), %rcx\n";
            else /* constant */            out << "\tmovq $" << i.op1.op << ", %rcx\n";
        } else {
            out << "\tmovq $1, %rcx\n";
        }
        out << "\tshl $3, %rcx\n";
        out << "\taddq %rcx, %rsp\n";
        
        if (!i.op1.op.empty() && !isVariable(i.op1.op)) {
            unsigned long long n = std::stoull(i.op1.op, nullptr, 0);
            if (n & 1ull) x64_sp_mod16 ^= 8;
        }
    }
    
    void Program::x64_gen_stack_load(std::ostream &out, const Instruction &i) {
        if (!isVariable(i.op1.op)) throw mx::Exception("stack_load first arg variable");
        if (i.op2.op.empty()) throw mx::Exception("stack_load requires index");
        x64_generateLoadVar(out, VarType::VAR_INTEGER, "%rax", i.op2);
        out << "\tmovq (%rsp, %rax, 8), %rcx\n";
        out << "\tmovq %rcx, " << getMangledName(i.op1) << "(%rip)\n";
    }

    void Program::x64_gen_stack_store(std::ostream &out, const Instruction &i) {
        if (!isVariable(i.op1.op)) throw mx::Exception("stack_store first arg variable");
        if (i.op2.op.empty()) throw mx::Exception("stack_store requires index");
        x64_generateLoadVar(out, VarType::VAR_INTEGER, "%rax", i.op2);
        out << "\tmovq " << getMangledName(i.op1) << "(%rip), %rcx\n";
        out << "\tmovq %rcx, (%rsp, %rax, 8)\n";
    }

    void Program::x64_gen_print(std::ostream &out, const Instruction &i) {
        xmm_offset = 0;
        std::vector<Operand> args;
        args.push_back(i.op1);
        if (!i.op2.op.empty()) args.push_back(i.op2);
        if (!i.op3.op.empty()) args.push_back(i.op3);
        for (const auto &v : i.vop) if (!v.op.empty()) args.push_back(v);
        x64_generateFunctionCall(out, "printf", args);
    }

    void Program::x64_gen_jmp(std::ostream &out, const Instruction &i) {
        if (!i.op1.op.empty()) {
            auto pos = labels.find(i.op1.op);
            if (pos == labels.end()) throw mx::Exception("Jump must have valid label: " + i.op1.op);
            const char* m = nullptr;
            switch (i.instruction) {
                case JMP: m = "jmp"; break;
                case JE:  m = "je";  break;
                case JNE: m = "jne"; break;
                case JL:  m = (last_cmp_type == CMP_FLOAT) ? "jb" : "jl"; break;
                case JLE: m = (last_cmp_type == CMP_FLOAT) ? "jbe" : "jle"; break;
                case JG:  m = (last_cmp_type == CMP_FLOAT) ? "ja" : "jg"; break;
                case JGE: m = (last_cmp_type == CMP_FLOAT) ? "jae" : "jge"; break;
                case JZ:  m = "jz";  break;
                case JNZ: m = "jnz"; break;
                case JA:  m = "ja";  break;
                case JB:  m = "jb";  break;
                default: break;
            }
            out << "\t" << m << " ." << i.op1.op << "\n";
        } else throw mx::Exception("Jump requires label");
    }

    void Program::x64_gen_cmp(std::ostream &out, const Instruction &i) {
        if (i.op2.op.empty()) throw mx::Exception("CMP requires two operands");
        VarType t1 = VarType::VAR_INTEGER, t2 = VarType::VAR_INTEGER;
        if (isVariable(i.op1.op)) t1 = getVariable(i.op1.op).type;
        if (isVariable(i.op2.op)) t2 = getVariable(i.op2.op).type;

        if (t1 == VarType::VAR_FLOAT || t2 == VarType::VAR_FLOAT) {
            if (t1 == VarType::VAR_FLOAT) {
                x64_generateLoadVar(out, VarType::VAR_FLOAT, "%xmm0", i.op1);
            } else { 
                x64_generateLoadVar(out, VarType::VAR_INTEGER, "%rax", i.op1);
                out << "\tcvtsi2sdq %rax, %xmm0\n";
            }

            if (t2 == VarType::VAR_FLOAT) {
                x64_generateLoadVar(out, VarType::VAR_FLOAT, "%xmm1", i.op2);
            } else { 
                x64_generateLoadVar(out, VarType::VAR_INTEGER, "%rax", i.op2);
                out << "\tcvtsi2sdq %rax, %xmm1\n";
            }
            
            out << "\tcomisd %xmm1, %xmm0\n";
            last_cmp_type = CMP_FLOAT;
        } else if (t1 == VarType::VAR_POINTER && (t2 == VarType::VAR_INTEGER || t2 == VarType::VAR_BYTE)) {
            x64_generateLoadVar(out, t1, "%rax", i.op1);
            x64_generateLoadVar(out, t2, "%rcx", i.op2);
            out << "\tcmpq %rcx, %rax\n";
            last_cmp_type = CMP_INTEGER;
        } else {
            x64_generateLoadVar(out, VarType::VAR_INTEGER, "%rax", i.op1);
            x64_generateLoadVar(out, VarType::VAR_INTEGER, "%rcx", i.op2);
            out << "\tcmpq %rcx, %rax\n";
            last_cmp_type = CMP_INTEGER;
        }
    }
    void Program::x64_gen_arth(std::ostream &out, std::string arth, const Instruction &i) {
        if (i.op3.op.empty()) {
            if (isVariable(i.op1.op)) {
                Variable &v = getVariable(i.op1.op);
                if (v.type == VarType::VAR_INTEGER || v.type == VarType::VAR_POINTER) {
                    x64_generateLoadVar(out, VarType::VAR_INTEGER, "%rax", i.op1);
                    x64_generateLoadVar(out, VarType::VAR_INTEGER, "%rcx", i.op2);
                    if (arth == "mul") arth = "imul";
                    out << "\t" << arth << "q %rcx, %rax\n";
                    out << "\tmovq %rax, " << getMangledName(i.op1) << "(%rip)\n";
                } else if (v.type == VarType::VAR_FLOAT) {
                    x64_generateLoadVar(out, VarType::VAR_FLOAT, "%xmm0", i.op1);
                    x64_generateLoadVar(out, VarType::VAR_FLOAT, "%xmm1", i.op2);
                    out << "\t" << arth << "sd %xmm1, %xmm0\n";
                    out << "\tmovsd %xmm0, " << getMangledName(i.op1) << "(%rip)\n";
                } else throw mx::Exception("arth unsupported type");
            } else throw mx::Exception("arth first must be variable");
        } else {
            if (isVariable(i.op1.op)) {
                Variable &v = getVariable(i.op1.op);
                if (v.type == VarType::VAR_INTEGER || v.type == VarType::VAR_POINTER) {
                    x64_generateLoadVar(out, VarType::VAR_INTEGER, "%rax", i.op2);
                    x64_generateLoadVar(out, VarType::VAR_INTEGER, "%rcx", i.op3);
                    if (arth == "mul") arth = "imul";
                    out << "\t" << arth << "q %rcx, %rax\n";
                    out << "\tmovq %rax, " << getMangledName(i.op1) << "(%rip)\n";
                } else if (v.type == VarType::VAR_FLOAT) {
                    x64_generateLoadVar(out, VarType::VAR_FLOAT, "%xmm0", i.op2);
                    x64_generateLoadVar(out, VarType::VAR_FLOAT, "%xmm1", i.op3);
                    out << "\t" << arth << "sd %xmm1, %xmm0\n";
                    out << "\tmovsd %xmm0, " << getMangledName(i.op1) << "(%rip)\n";
                } else throw mx::Exception("arth unsupported type");
            } else throw mx::Exception("arth first must be variable");
        }
    }

    void Program::x64_gen_exit(std::ostream &out, const Instruction &i) {
        
        bool uses_std_module = false;
        for(const auto &e : external) {
            if(e.mod == "std") {
                uses_std_module = true;
                break;
            }
        }

        
        if(uses_std_module) {
            out << "\t# Clean up program arguments before exit\n";
            size_t total = x64_reserve_call_area(out, 0);
            out << "\tcall free_program_args\n";
            x64_release_call_area(out, total);
        }

        if (!i.op1.op.empty()) {
            std::vector<Operand> opz; opz.push_back(i.op1);
            x64_generateFunctionCall(out, "exit", opz);
        } else throw mx::Exception("exit requires argument");
    }

    void Program::x64_gen_fcmp(std::ostream &out, const Instruction &i) {
        if (i.op2.op.empty()) {
            throw mx::Exception("FCMP requires two operands");
        }
        
        x64_generateLoadVar(out, VarType::VAR_FLOAT, "%xmm0", i.op1);
        x64_generateLoadVar(out, VarType::VAR_FLOAT, "%xmm1", i.op2);
        out << "\tcomisd %xmm1, %xmm0\n";
        last_cmp_type = CMP_FLOAT;
    }

       void Program::x64_gen_jae(std::ostream &out, const Instruction &i) {
        out << "\tjae ." << i.op1.op << "\n";
    }

    void Program::x64_gen_jbe(std::ostream &out, const Instruction &i) {
        out << "\tjbe ." << i.op1.op << "\n";
    }

    void Program::x64_gen_jc(std::ostream &out, const Instruction &i) {
        out << "\tjc ." << i.op1.op << "\n";
    }

    void Program::x64_gen_jnc(std::ostream &out, const Instruction &i) {
        out << "\tjnc ." << i.op1.op << "\n";
    }

    void Program::x64_gen_jp(std::ostream &out, const Instruction &i) {
        out << "\tjp ." << i.op1.op << "\n";
    }

    void Program::x64_gen_jnp(std::ostream &out, const Instruction &i) {
        out << "\tjnp ." << i.op1.op << "\n";
    }

    void Program::x64_gen_jo(std::ostream &out, const Instruction &i) {
        out << "\tjo ." << i.op1.op << "\n";
    }

    void Program::x64_gen_jno(std::ostream &out, const Instruction &i) {
        out << "\tjno ." << i.op1.op << "\n";
    }

    void Program::x64_gen_js(std::ostream &out, const Instruction &i) {
        out << "\tjs ." << i.op1.op << "\n";
    }

    void Program::x64_gen_jns(std::ostream &out, const Instruction &i) {
        out << "\tjns ." << i.op1.op << "\n";
    }
}
