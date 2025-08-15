#include "mxvm/icode.hpp"

namespace mxvm {

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

   // --- BEGIN FIX: Corrected stack management ---
   std::pair<size_t, unsigned> Program::x64_reserve_call_area(std::ostream &out, size_t spill_bytes) {
        const unsigned original_mod = x64_sp_mod16;
        const size_t needed_for_call = 32 + spill_bytes;
        const size_t current_offset = original_mod + needed_for_call;
        const size_t padding = (16 - (current_offset % 16)) % 16;
        const size_t total_sub = needed_for_call + padding;
        out << "\tsub $" << total_sub << ", %rsp\n";
        x64_sp_mod16 = 0;
        return {total_sub, original_mod};
    }

    void Program::x64_direct_stack_adjust(std::ostream &out, int64_t bytes) {
        if (bytes > 0) {
            out << "\tsub $" << bytes << ", %rsp\n";
            x64_sp_mod16 = (unsigned)((x64_sp_mod16 + bytes) & 15);
        } else if (bytes < 0) {
            out << "\tadd $" << -bytes << ", %rsp\n";
            x64_sp_mod16 = (unsigned)((x64_sp_mod16 + bytes) & 15); // bytes is negative
        }
    }
   void Program::x64_release_call_area(std::ostream &out, std::pair<size_t, unsigned> frame_info) {
        out << "\tadd $" << frame_info.first << ", %rsp\n";
        x64_sp_mod16 = frame_info.second;
    }
    // --- END FIX ---

    void Program::x64_emit_iob_func(std::ostream &out, int index, const std::string &dstReg) {
        out << "\tmov $" << index << ", %ecx\n";
        auto frame = x64_reserve_call_area(out, 0);
        out << "\tcall __acrt_iob_func\n";
        x64_release_call_area(out, frame);
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

    
    static inline void x64_call_frame_enter(std::ostream &out, size_t bytes) { out << "\tsub $" << bytes << ", %rsp\n"; }
    static inline void x64_call_frame_leave(std::ostream &out, size_t bytes) { out << "\tadd $" << bytes << ", %rsp\n"; }

    void Program::x64_generateFunctionCall(std::ostream &out,
                               const std::string &name,
                               std::vector<Operand> &args) {
    xmm_offset = 0;
    size_t iregs = 0, fregs = 0, stack_args = 0;
    
    
    for (auto &a : args) {
        bool isfp = isVariable(a.op) && getVariable(a.op).type == VarType::VAR_FLOAT;
        if (isfp) { (fregs < 4) ? ++fregs : ++stack_args; }
        else      { (iregs < 4) ? ++iregs : ++stack_args; }
    }

    
    const size_t spill_bytes = stack_args * 8;
    auto frame = x64_reserve_call_area(out, spill_bytes);

    
    if (stack_args) {
        size_t sp_off = 32;
        size_t arg_idx = 4;
        
        for (size_t i = 0; i < stack_args; ++i) {
            if (arg_idx < args.size()) {
                bool isfp = isVariable(args[arg_idx].op) && getVariable(args[arg_idx].op).type == VarType::VAR_FLOAT;
                
                if (isfp) {
                    x64_generateLoadVar(out, VarType::VAR_FLOAT, "%xmm0", args[arg_idx]);
                    out << "\tmovsd %xmm0, " << sp_off << "(%rsp)\n";
                } else {
                    x64_generateLoadVar(out, VarType::VAR_INTEGER, "%rax", args[arg_idx]);
                    out << "\tmovq %rax, " << sp_off << "(%rsp)\n";
                }
                
                sp_off += 8;
                arg_idx++;
            }
        }
    }

    size_t ri = 0, rf = 0;
    for (size_t z = 0; z < args.size() && z < 4; ++z) {
        bool isfp = isVariable(args[z].op) && getVariable(args[z].op).type == VarType::VAR_FLOAT;
        
        if (isfp) {
            if (rf < 4) {
                std::string reg = x64_getRegisterByIndex((int)rf, VarType::VAR_FLOAT);
                x64_generateLoadVar(out, VarType::VAR_FLOAT, reg, args[z]);
                ++rf;
            }
        } else {
            if (ri < 4) {
                std::string reg = x64_getRegisterByIndex((int)ri, VarType::VAR_INTEGER);
                x64_generateLoadVar(out, VarType::VAR_INTEGER, reg, args[z]);
                ++ri;
            }
        }
    }

    out << "\tcall " << name << "\n";
    x64_release_call_area(out, frame);
}

    void Program::x64_generateInvokeCall(std::ostream &out, std::vector<Operand> &op) {
        if (op.empty() || op[0].op.empty()) throw mx::Exception("invoke requires instruction name");
        std::string n = op[0].op;
        std::vector<Operand> a;
        for (size_t i = 1; i < op.size(); ++i) if (!op[i].op.empty()) a.push_back(op[i]);
        x64_generateFunctionCall(out, n, a);
    }

    void Program::x64_generateCode(const Platform&, bool, std::ostream &out) {
        this->add_standard();
        std::unordered_map<int,std::string> labels_;
        for (auto &l : labels) labels_[l.second.first] = l.first;

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

        if (this->object) out << "\t.globl " << name << "\n";

        for (auto &lbl : labels)
            if (lbl.second.second == true) out << "\t.globl " << name + "_" + lbl.first << "\n";

        std::sort(external.begin(), external.end(), [](const ExternalFunction &a, const ExternalFunction &b){
            if (a.mod == b.mod) return a.name < b.name;
            return a.mod < b.mod;
        });
        for (auto &e : external)
            out << "\t.extern " << (e.module ? e.name : e.mod + "_" + e.name) << "\n";

        if (this->object) {
            out << "\t.p2align 4, 0x90\n";
            out << name << ":\n";
        } else {
            out << "\t.p2align 4, 0x90\n";
            out << ".globl main\n";
            out << "main:\n";
        }

        out << "\tpush %rbp\n";
        out << "\tmov %rsp, %rbp\n";
        x64_sp_mod16 = 8; // prologue consumed 8 bytes

        bool done_found = false;

        for (size_t i = 0; i < inc.size(); ++i) {
            const Instruction &instr = inc[i];
            for (auto l : labels) {
                if (l.second.first == i && l.second.second) {
                    out << "\t.p2align 4, 0x90\n";
                    out << name + "_" + l.first << ":\n";
                    out << "\tpush %rbp\n";
                    out << "\tmov %rsp, %rbp\n";
                    x64_sp_mod16 = 8; // new prologue
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

    void Program::x64_generateInstruction(std::ostream &out, const Instruction &i) {
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
            default: throw mx::Exception("Invalid or unsupported instruction");
        }
    }

    void Program::x64_gen_done(std::ostream &out, const Instruction&) {
        out << "\txor %eax, %eax\n";
        out << "\tleave\n";
        out << "\tret\n";
        // After 'leave', we conceptually exit; no need to update x64_sp_mod16.
    }

    void Program::x64_gen_ret(std::ostream &out, const Instruction&) {
        out << "\tleave\n";
        out << "\tret\n";
    }

    void Program::x64_gen_return(std::ostream &out, const Instruction &i) {
        if (isVariable(i.op1.op)) out << "\tmovq %rax, " << getMangledName(i.op1) << "(%rip)\n";
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
        auto frame = x64_reserve_call_area(out, 0);
        out << "\tcall " << getMangledName(i.op1) << "\n";
        x64_release_call_area(out, frame);
    }

    void Program::x64_gen_alloc(std::ostream &out, const Instruction &i) {
        if (!isVariable(i.op1.op)) throw mx::Exception("ALLOC destination must be a variable");
        Variable &v = getVariable(i.op1.op);
        if (v.type != VarType::VAR_POINTER) throw mx::Exception("ALLOC destination must be a pointer");

        if (!i.op3.op.empty()) {
            if (isVariable(i.op3.op)) out << "\tmovq " << getMangledName(i.op3) << "(%rip), %rcx\n";
            else                      out << "\tmovq $" << i.op3.op << ", %rcx\n";
        } else out << "\tmovq $1, %rcx\n";
        if (!i.op2.op.empty()) {
            if (isVariable(i.op2.op)) out << "\tmovq " << getMangledName(i.op2) << "(%rip), %rdx\n";
            else                      out << "\tmovq $" << i.op2.op << ", %rdx\n";
        } else out << "\tmovq $8, %rdx\n";

        auto frame = x64_reserve_call_area(out, 0);
        out << "\tcall calloc\n";
        x64_release_call_area(out, frame);

        out << "\tmovq %rax, " << getMangledName(i.op1) << "(%rip)\n";
        getVariable(i.op1.op).var_value.owns = true;
    }

    void Program::x64_gen_free(std::ostream &out, const Instruction &i) {
        if (!isVariable(i.op1.op)) throw mx::Exception("FREE argument must be a variable");
        Variable &v = getVariable(i.op1.op);
        if (v.type != VarType::VAR_POINTER) throw mx::Exception("FREE argument must be a pointer");

        out << "\tmovq " << getMangledName(i.op1) << "(%rip), %rcx\n";
        auto frame = x64_reserve_call_area(out, 0);
        out << "\tcall free\n";
        x64_release_call_area(out, frame);
    }

    void Program::x64_gen_load(std::ostream &out, const Instruction &i) {
        if (!isVariable(i.op1.op)) throw mx::Exception("LOAD dest must be variable");
        Variable &dest = getVariable(i.op1.op);
        if (!isVariable(i.op2.op)) throw mx::Exception("LOAD source must be pointer variable");
        Variable &ptrVar = getVariable(i.op2.op);
        size_t size = (dest.type == VarType::VAR_FLOAT) ? sizeof(double) : sizeof(int64_t);
        if (!i.vop.empty() && !i.vop[0].op.empty()) {
            if (isVariable(i.vop[0].op)) size = (size_t)getVariable(i.vop[0].op).var_value.int_value;
            else size = (size_t)std::stoll(i.vop[0].op, nullptr, 0);
        }
        std::string idx_reg = "%rcx";
        if (!i.op3.op.empty()) x64_generateLoadVar(out, VarType::VAR_INTEGER, idx_reg, i.op3);
        else out << "\txor %rcx, %rcx\n";

        if (ptrVar.type == VarType::VAR_POINTER) out << "\tmovq " << getMangledName(i.op2) << "(%rip), %rax\n";
        else if (ptrVar.type == VarType::VAR_STRING) out << "\tleaq " << getMangledName(i.op2) << "(%rip), %rax\n";
        else throw mx::Exception("Load invalid type: " + i.op2.op);

        auto emit_load = [&](const std::string &mem){
            if (dest.type == VarType::VAR_INTEGER) {
                if (size == 1) { out << "\tmovzbq " << mem << ", %rdx\n"; out << "\tmovq %rdx, " << getMangledName(i.op1) << "(%rip)\n"; }
                else { out << "\tmovq " << mem << ", %rdx\n"; out << "\tmovq %rdx, " << getMangledName(i.op1) << "(%rip)\n"; }
            } else if (dest.type == VarType::VAR_FLOAT) {
                out << "\tmovsd " << mem << ", %xmm0\n";
                out << "\tmovsd %xmm0, " << getMangledName(i.op1) << "(%rip)\n";
            } else if (dest.type == VarType::VAR_BYTE) {
                out << "\tmovzbq " << mem << ", %r8\n";
                out << "\tmovb %r8b, " << getMangledName(i.op1) << "(%rip)\n";
            } else throw mx::Exception("LOAD: unsupported dest type");
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

    void Program::x64_gen_store(std::ostream &out, const Instruction &i) {
        uint64_t value = 0;
        VarType type;
        bool is_const = false;
        if (!isVariable(i.op1.op) && i.op1.type == OperandType::OP_CONSTANT) {
            value = std::stoll(i.op1.op, nullptr, 0);
            type = VarType::VAR_INTEGER;
            is_const = true;
        } else {
            Variable &src = getVariable(i.op1.op);
            value = src.var_value.int_value;
            type = src.type;
            is_const = false;
        }
        if (!isVariable(i.op2.op)) throw mx::Exception("STORE destination must be pointer variable: " + i.op2.op);
        Variable &ptrVar = getVariable(i.op2.op);

        size_t elemSize = 8;
        bool elemSizeKnown = true;
        if (!i.vop.empty() && !i.vop[0].op.empty()) {
            if (isVariable(i.vop[0].op)) {
                elemSizeKnown = false;
                x64_generateLoadVar(out, VarType::VAR_INTEGER, "%rdx", i.vop[0]);
            } else elemSize = (size_t)std::stoll(i.vop[0].op, nullptr, 0);
        } else elemSize = 8;

        std::string idx_reg = "%rcx";
        if (!i.op3.op.empty()) x64_generateLoadVar(out, VarType::VAR_INTEGER, idx_reg, i.op3);
        else throw mx::Exception("STORE requires element index");

        if (ptrVar.type == VarType::VAR_POINTER) out << "\tmovq " << getMangledName(i.op2) << "(%rip), %rax\n";
        else if (ptrVar.type == VarType::VAR_STRING && ptrVar.var_value.buffer_size > 0) out << "\tleaq " << getMangledName(i.op2) << "(%rip), %rax\n";
        else throw mx::Exception("STORE must target pointer or string buffer");

        auto emit_imm_store = [&](const std::string &mem){
            switch (elemSize) {
                case 1: out << "\tmovb $"  << (value & 0xFFu)                 << ", " << mem << "\n"; break;
                case 2: out << "\tmovw $"  << (value & 0xFFFFu)               << ", " << mem << "\n"; break;
                case 4: out << "\tmovl $"  << (uint32_t)(value & 0xFFFFFFFFu) << ", " << mem << "\n"; break;
                case 8: out << "\tmovq $"  << value                           << ", " << mem << "\n"; break;
                default: throw mx::Exception("STORE: unsupported elemSize");
            }
        };
        auto emit_rdx_store = [&](const std::string &mem){
            switch (elemSize) {
                case 1: out << "\tmovb %dl, "  << mem << "\n"; break;
                case 2: out << "\tmovw %dx, "  << mem << "\n"; break;
                case 4: out << "\tmovl %edx, " << mem << "\n"; break;
                case 8: out << "\tmovq %rdx, " << mem << "\n"; break;
                default: throw mx::Exception("STORE: unsupported elemSize");
            }
        };
        auto emit_store = [&](const std::string &mem){
            if (is_const) { emit_imm_store(mem); return; }
            if (type == VarType::VAR_INTEGER || type == VarType::VAR_POINTER || type == VarType::VAR_EXTERN) {
                out << "\tmovq " << getMangledName(i.op1) << "(%rip), %rdx\n";
                emit_rdx_store(mem);
            } else if (type == VarType::VAR_BYTE) {
                out << "\tmovzbq " << getMangledName(i.op1) << "(%rip), %rdx\n";
                emit_rdx_store(mem);
            } else if (type == VarType::VAR_FLOAT) {
                if (elemSize == 4) {
                    out << "\tmovss " << getMangledName(i.op1) << "(%rip), %xmm0\n";
                    out << "\tmovss %xmm0, " << mem << "\n";
                } else {
                    out << "\tmovsd " << getMangledName(i.op1) << "(%rip), %xmm0\n";
                    out << "\tmovsd %xmm0, " << mem << "\n";
                }
            } else throw mx::Exception("STORE: unsupported source type");
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

    void Program::x64_gen_to_int(std::ostream &out, const Instruction &i) {
        out << "\tleaq " << getMangledName(i.op2) << "(%rip), %rcx\n";
        auto frame = x64_reserve_call_area(out, 0);
        out << "\tcall atol\n";
        x64_release_call_area(out, frame);
        out << "\tmovq %rax, " << getMangledName(i.op1) << "(%rip)\n";
    }

    void Program::x64_gen_to_float(std::ostream &out, const Instruction &i) {
        out << "\tleaq " << getMangledName(i.op2) << "(%rip), %rcx\n";
        auto frame = x64_reserve_call_area(out, 0);
        out << "\tcall atof\n";
        x64_release_call_area(out, frame);
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
        auto frame0 = x64_reserve_call_area(out, 0);
        out << "\tcall __acrt_iob_func\n";
        x64_release_call_area(out, frame0);
        out << "\tmov %rax, %r8\n";


        out << "\tleaq " << getMangledName(i.op1) << "(%rip), %rcx\n";
        out << "\tmovq $" << dest.var_value.buffer_size << ", %rdx\n";
        auto frame1 = x64_reserve_call_area(out, 0);
        out << "\tcall fgets\n";
        x64_release_call_area(out, frame1);
        static size_t over_count = 0;


        out << "\ttest %rax, %rax\n";
        out << "\tje .over" << over_count << "\n";


        out << "\tleaq " << getMangledName(i.op1) << "(%rip), %rcx\n";
        auto frame2 = x64_reserve_call_area(out, 0);
        out << "\tcall strlen\n";
        x64_release_call_area(out, frame2);

        out << "\tmov %rax, %rcx\n";
        out << "\ttest %rcx, %rcx\n";  
        out << "\tje .over" << over_count << "\n";
        out << "\tdec %rcx\n";        


        out << "\tleaq " << getMangledName(i.op1) << "(%rip), %r10\n";
        out << "\tmovb $0, (%r10, %rcx, 1)\n";
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
                out << "\tnotq %rax\n";
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
        // Update alignment tracker when the count is a known constant.
        if (!i.op1.op.empty() && !isVariable(i.op1.op)) {
            unsigned long long n = std::stoull(i.op1.op, nullptr, 0);
            if (n & 1ull) x64_sp_mod16 ^= 8;
        }
    }

    void Program::x64_gen_stack_load(std::ostream &out, const Instruction &i) {
        if (!isVariable(i.op1.op)) throw mx::Exception("stack_load first arg variable");
        Variable &dest = getVariable(i.op1.op);
        if (i.op2.op.empty()) throw mx::Exception("stack_load requires index");
        x64_generateLoadVar(out, dest.type, "%rax", i.op2);
        out << "\tmovq (%rsp, %rax, 8), %rcx\n";
        out << "\tmovq %rcx, " << getMangledName(i.op1) << "(%rip)\n";
    }

    void Program::x64_gen_stack_store(std::ostream &out, const Instruction &i) {
        if (!isVariable(i.op1.op)) throw mx::Exception("stack_store first arg variable");
        Variable &dest = getVariable(i.op1.op);
        if (i.op2.op.empty()) throw mx::Exception("stack_store requires index");
        x64_generateLoadVar(out, dest.type, "%rax", i.op2);
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
                case JMP: m = "jmp"; break; case JE: m = "je"; break; case JNE: m = "jne"; break;
                case JL: m = "jl"; break; case JLE: m = "jle"; break; case JG: m = "jg"; break;
                case JGE: m = "jge"; break; case JZ: m = "jz"; break; case JNZ: m = "jnz"; break;
                case JA: m = "ja"; break; case JB: m = "jb"; break; default: break;
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
            x64_generateLoadVar(out, VarType::VAR_FLOAT, "%xmm0", i.op1);
            x64_generateLoadVar(out, VarType::VAR_FLOAT, "%xmm1", i.op2);
            out << "\tcomisd %xmm1, %xmm0\n";
        } else if (t1 == VarType::VAR_POINTER && (t2 == VarType::VAR_INTEGER || t2 == VarType::VAR_BYTE)) {
            x64_generateLoadVar(out, t1, "%rax", i.op1);
            x64_generateLoadVar(out, t2, "%rcx", i.op2);
            out << "\tcmpq %rcx, %rax\n";
        } else {
            x64_generateLoadVar(out, VarType::VAR_INTEGER, "%rax", i.op1);
            x64_generateLoadVar(out, VarType::VAR_INTEGER, "%rcx", i.op2);
            out << "\tcmpq %rcx, %rax\n";
        }
    }

    void Program::x64_gen_mov(std::ostream &out, const Instruction &i) {
        if (!isVariable(i.op1.op)) throw mx::Exception("mov first operand must be variable");
        Variable &v = getVariable(i.op1.op);
        if (isVariable(i.op2.op)) {
            Variable &v2 = getVariable(i.op2.op);
            if (v2.type != v.type) throw mx::Exception("mov type mismatch");
            switch (v.type) {
                case VarType::VAR_INTEGER:
                    x64_generateLoadVar(out, VarType::VAR_INTEGER, "%rax", i.op2);
                    out << "\tmovq %rax, " << getMangledName(i.op1) << "(%rip)\n";
                    break;
                case VarType::VAR_POINTER:
                case VarType::VAR_EXTERN:
                    x64_generateLoadVar(out, v.type, "%rax", i.op2);
                    out << "\tmovq %rax, " << getMangledName(i.op1) << "(%rip)\n";
                    break;
                case VarType::VAR_FLOAT:
                    x64_generateLoadVar(out, VarType::VAR_FLOAT, "%xmm0", i.op2);
                    out << "\tmovsd %xmm0, " << getMangledName(i.op1) << "(%rip)\n";
                    break;
                case VarType::VAR_STRING:
                    x64_generateLoadVar(out, VarType::VAR_STRING, "%rax", i.op2);
                    out << "\tmovq %rax, " << getMangledName(i.op1) << "(%rip)\n";
                    break;
                case VarType::VAR_BYTE:
                    x64_generateLoadVar(out, VarType::VAR_BYTE, "%rax", i.op2);
                    out << "\tmovb %al, " << getMangledName(i.op1) << "(%rip)\n";
                    break;
                default: throw mx::Exception("mov unsupported type");
            }
        } else {
            if (i.op2.type == OperandType::OP_CONSTANT) {
                out << "\tmovq $" << i.op2.op << ", " << getMangledName(i.op1) << "(%rip)\n";
            }
        }
    }

    void Program::x64_gen_arth(std::ostream &out, std::string arth, const Instruction &i) {
        if (i.op3.op.empty()) {
            if (isVariable(i.op1.op)) {
                Variable &v = getVariable(i.op1.op);
                if (v.type == VarType::VAR_INTEGER) {
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
                if (v.type == VarType::VAR_INTEGER) {
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
        if (!i.op1.op.empty()) {
            std::vector<Operand> opz; opz.push_back(i.op1);
            x64_generateFunctionCall(out, "exit", opz);
        } else throw mx::Exception("exit requires argument");
    }

}
