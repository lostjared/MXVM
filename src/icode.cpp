#include"mxvm/icode.hpp"
#include"mxvm/parser.hpp"
#include"scanner/exception.hpp"
#include"mxvm/function.hpp"
#include<iomanip>
#include<iostream>
#include<sstream>
#include<regex>
#include<filesystem>

namespace mxvm {

    std::string Program::getMangledName(const std::string& var) {
        if (!isVariable(var)) return var;
        Variable& v = getVariable(var);
        if (!v.obj_name.empty())
            return v.obj_name + "_" + var;
        else
            return var;
    }

        
    Base::Base(Base&& other) noexcept
        : name(std::move(other.name))
        , inc(std::move(other.inc))
        , vars(std::move(other.vars))
        , labels(std::move(other.labels))
        , external(std::move(other.external))
        , external_functions(std::move(other.external_functions)) {
    }

    Base& Base::operator=(Base&& other) noexcept {
        if (this != &other) {
            name = std::move(other.name);
            inc = std::move(other.inc);
            vars = std::move(other.vars);
            labels = std::move(other.labels);
            external = std::move(other.external);
            external_functions = std::move(other.external_functions);
        }
        return *this;
    }

    Program::Program() : pc(0), running(false) {
        Variable vstdout, vstdin, vstderr;
        vstdout.setExtern("stdout", stdout);
        vstdin.setExtern("stdin", stdin);
        vstderr.setExtern("stderr", stderr);
        add_variable("stdout", vstdout);
        add_variable("stdin", vstdin);
        add_variable("stderr", vstderr);

        add_extern("string", "strlen", true);
        add_extern("main", "printf", true);
        add_extern("main", "calloc", true);
        add_extern("main", "free", true);
        add_extern("main", "exit", true);
        add_extern("main", "atol", true);
        add_extern("main", "atof", true);
    }

    void Program::setArgs(const std::vector<std::string> &argv) {
        std::copy(argv.begin(), argv.end(), std::back_inserter(args));
    }

    void Program::setObject(bool obj) {
        object = obj;
    }

    Program::~Program() {
        for(auto &i : vars) {
            if(i.second.var_value.ptr_value != nullptr && i.second.var_value.owns) {
                free(i.second.var_value.ptr_value);
                i.second.var_value.ptr_value = nullptr;
                if(debug_mode) {
                    std::cout << " Pointer: " << i.first << "\n";
                }
            }
        }

        for(auto &h : RuntimeFunction::handles) {
            if(h.second) {
                dlclose(h.second);
                h.second = nullptr;
                char *errf = dlerror();
                if(errf != nullptr) {
                    std::cerr << "Error releaseing module: " << errf << "\n";
                } else {
                    if(mxvm::debug_mode) {
                        std::cout << "Released module: " << h.first << "\n";
                    }
                }
            }
        }
    }

    bool Program::isFunctionValid(const std::string &f) {
        auto lbl = labels.find(f);
        if(lbl != labels.end() && lbl->second.second)
            return true;
        for(auto &o : objects) {
            if(o->isFunctionValid(f))
                return true;
        }
        return false;
    }

    std::unordered_map<std::string, void *> RuntimeFunction::handles;
    std::string Base::root_name;

    RuntimeFunction::RuntimeFunction(const std::string &mod, const std::string &name) {
        fname = name;
        handle = nullptr;
        if(handles.find(mod) == handles.end()) {
            handle = dlopen(mod.c_str(), RTLD_LAZY);
            handles[mod] = handle;    
        } else {
            handle = handles[mod];
        }
        if(handle == nullptr) {
            throw mx::Exception("Error could not open module: " + mod);
        }
        func = (void*)dlsym(handle, name.c_str());
        if(func == nullptr) {
            throw mx::Exception("Error could not find symbol: " + name + " in: " + mod);
        }
        char *dl_err = dlerror();
        if(dl_err != nullptr) {
            throw mx::Exception ("Error: " + std::string(dl_err));
        }
    }
    
    void RuntimeFunction::call(Program *program, std::vector<Operand> &operands) {
        if (!func || !handle) {
            throw mx::Exception("RuntimeFunction: function: " + this->fname + " pointer is null: " + this->mod_name);
        }
        using FuncType = void(*)(Program *program, std::vector<Operand>&);
        FuncType f = reinterpret_cast<FuncType>(func);
        f(program, operands);
    }
       
    Base *Base::base = nullptr;

    void Base::add_instruction(const Instruction &i) {
        inc.push_back(i);
    }
    
    void Base::add_label(const std::string &name, uint64_t address, bool f) {
        if(labels.find(name) == labels.end())
            labels[name] = std::make_pair(address, f);
    }

    void Base::add_variable(const std::string &name, const Variable &v) {
        if(vars.find(name) == vars.end()) {
            vars[name] = v;
        } else {
            throw mx::Exception("Duplciate variable name: " + name);
        }
    }

    void Base::add_global(const std::string &objname,const std::string &name, const Variable &v) {
    }

    void Base::add_extern(const std::string &mod, const std::string &name, bool module) {
        ExternalFunction f = {name, mod, module};
        auto it = std::find(external.begin(), external.end(),f);
        if(it == external.end())
            external.push_back({name, mod, module});   
    }
    
    void Base::add_runtime_extern(const std::string &mod_name, const std::string &mod, const std::string &func_name, const std::string &name) {
        if(mod_name.empty() || mod.empty() || func_name.empty() || name.empty()) {
            throw mx::Exception("External function missing information.");
        }
    
        if(base != nullptr) {
            if(base->external_functions.find(name) == base->external_functions.end()) {
                base->external_functions[name] = RuntimeFunction(mod, func_name);
                base->external_functions[name].mod_name = mod_name;
            }
        }
    }
    Program* Program::getObjectByName(const std::string& searchName) {
        if (this->name == searchName) return this;
        for (auto& obj : objects) {
            Program* found = obj->getObjectByName(searchName);
            if (found) return found;
        }
        return nullptr;
    }
  

    std::string Program::escapeNewLines(const std::string& input) {
        std::string result;
        for (char c : input) {
            switch (c) {
                case '\t':
                    result += "\\t";
                    break;
                case '\n':
                    result += "\\n";
                    break;
                case '\r':
                    result += "\\r";
                    break;
                default:
                    result += c;
                    break;
            }
        }
        return result;
    }

  void Program::memoryDump(std::ostream &out) {
        out << "Current working directory: ";
        try {
            out << std::filesystem::current_path().string() << "\n";
        } catch (const std::exception& e) {
            out << "(unable to retrieve: " << e.what() << ")\n";
        }


        std::function<void(Program*, std::ostream&, int)> dumpProgram;
        dumpProgram = [&](Program* prog, std::ostream& out, int depth = 0) {
            std::string indent(depth * 2, ' ');
            out << indent << "=== MEMORY DUMP for " << prog->name << " ===\n";
            if (prog->vars.empty()) {
                out << indent << "  (no variables)\n";
            } else {
                out << indent << std::left << std::setw(15) << "Name"
                    << std::setw(12) << "Type"
                    << std::setw(20) << "Value"
                    << std::setw(12) << "PtrSize"
                    << std::setw(12) << "PtrCount"
                    << std::setw(8) << "Owns"
                    << "\n";
                out << indent << std::string(79, '-') << "\n";
                for (const auto &var : prog->vars) {
                    out << indent << std::setw(15) << var.first;
                    std::string typeStr;
                    switch (var.second.type) {
                        case VarType::VAR_INTEGER: typeStr = "int"; break;
                        case VarType::VAR_FLOAT: typeStr = "float"; break;
                        case VarType::VAR_STRING: typeStr = "string"; break;
                        case VarType::VAR_POINTER: typeStr = "ptr"; break;
                        case VarType::VAR_LABEL: typeStr = "label"; break;
                        case VarType::VAR_EXTERN: typeStr = "external"; break;
                        case VarType::VAR_ARRAY: typeStr = "array"; break;
                        case VarType::VAR_BYTE:  typeStr = "byte"; break;
                        default: typeStr = "unknown"; break;
                    }
                    out << std::setw(12) << typeStr;
                    switch (var.second.type) {
                        case VarType::VAR_INTEGER:
                        case VarType::VAR_BYTE:
                            out << std::setw(20) << var.second.var_value.int_value;
                            break;
                        case VarType::VAR_FLOAT:
                            out << std::setw(20) << std::fixed << std::setprecision(6)
                                << var.second.var_value.float_value;
                            break;
                        case VarType::VAR_STRING:
                            out << std::setw(20) << ("\"" + Program::escapeNewLines(var.second.var_value.str_value) + "\"");
                            break;
                        case VarType::VAR_POINTER:
                        case VarType::VAR_EXTERN:
                            if (var.second.var_value.ptr_value == nullptr)
                                out << std::setw(20) << "null";
                            else {
                                char ptr_buf[256];
                                snprintf(ptr_buf, sizeof(ptr_buf), "%p", var.second.var_value.ptr_value);
                                out << std::setw(20) << ptr_buf << std::dec;
                            }
                            break;
                        case VarType::VAR_LABEL:
                            out << std::setw(20) << var.second.var_value.label_value;
                            break;
                        default:
                            out << std::setw(20) << var.second.var_value.int_value;
                            break;
                    }
                    if (var.second.type == VarType::VAR_POINTER) {
                        out << std::setw(12) << var.second.var_value.ptr_size
                            << std::setw(12) << var.second.var_value.ptr_count
                            << std::setw(8) << (var.second.var_value.owns ? "yes" : "no");
                    } else {
                        out << std::setw(12) << "-"
                            << std::setw(12) << "-"
                            << std::setw(8) << "-";
                    }
                    out << "\n";
                }
            }
            out << "\n";
            for (const auto& obj : prog->objects) {
                if (obj) dumpProgram(obj.get(), out, depth + 1);
            }
        };
        dumpProgram(this, out, 0);
    }
    
    void Program::generateCode(bool obj, std::ostream &out) {
        std::unordered_map<int, std::string> labels_;
        for(auto &l : labels) {
            labels_[l.second.first] = l.first;
        }
        out << ".section .data\n";
        std::vector<std::string> var_names;
        for(auto &v : vars) {
            var_names.push_back(v.first);
        }
        std::sort(var_names.begin(), var_names.end());
        
        for(auto &v : var_names) {
            std::string var_out_name;
            auto varx = getVariable(v);
            if(varx.is_global) {
                var_out_name = getMangledName(v);
            } else if(!varx.obj_name.empty()) {
                var_out_name = vars[v].obj_name + "_" + v;
            } else if(this->object) {
                var_out_name = name + "_" + v;
            } else {
                var_out_name = v;
            }
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
            if(varx.type == VarType::VAR_EXTERN) {
                out << "\t.extern " << v << "\n";
            } 
        }
        for(auto &v : var_names) {
            auto varx = getVariable(v);
            if(varx.type == VarType::VAR_STRING && varx.var_value.buffer_size == 0) {
                std::string var_out_name;
                if(varx.is_global) {
                    var_out_name = getMangledName(v);
                } else if(!vars[v].obj_name.empty()) {
                    var_out_name = getMangledName(v);
                } else if(this->object) {
                    var_out_name = getMangledName(v);
                } else {
                    var_out_name = v;
                }
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
                            out << "\t.extern " << getMangledName(v) << "\n";
                        break;
                        default:
                        break;
                    }
                }
            }

            if(object) {
                for(auto &v : var_names) {
                    auto varx = getVariable(v);
                    auto t = varx.type;
                    if(varx.is_global) {
                        switch(t) {
                            case VarType::VAR_BYTE:
                            case VarType::VAR_FLOAT:
                            case VarType::VAR_INTEGER:
                                out << "\t.global " << getMangledName(v) << "\n";
                            break;
                            default:
                            break;
                        }
                        
                    }
                }
            }
        }


        out << ".section .bss\n";
        for(auto &v : var_names) {
            std::string var_out_name;
            auto varx = getVariable(v);
            if(varx.is_global) {
                var_out_name = getMangledName(v);
            } else if(!varx.obj_name.empty()) {
                var_out_name = vars[v].obj_name + "_" + v;
            } else if(this->object) {
                var_out_name = name + "_" + v;
            } else {
                var_out_name = v;
            }
            if(varx.type == VarType::VAR_POINTER) {
                out << "\t.comm " << var_out_name << ", 8\n";
            } else if(varx.type == VarType::VAR_STRING && varx.var_value.buffer_size > 0) {
                out << "\t.comm " << var_out_name << ", " << varx.var_value.buffer_size << "\n";
            }
        }
        
       
            for(auto &v : var_names) {
                auto varx = getVariable(v); 
                if(varx.is_global)
                    out << "\t.extern " << getMangledName(v) << "\n";
            }

            if(object) {
                for(auto &v : var_names) {
                    auto varx = getVariable(v);
                    if(varx.is_global && (varx.type == VarType::VAR_STRING || varx.type == VarType::VAR_POINTER)) {
                        out << "\t.global " << getMangledName(v) << "\n";
                    }
                }
            }
        
        out << ".section .text\n";

        if(this->object)
            out << "\t.global " << name << "\n";
        else
            out << "\t.global main\n";

        for(auto &lbl : labels) {
            if(lbl.second.second == true) {
                out << "\t.global " << name << "_" << lbl.first << "\n";
            }
        }
        std::sort(external.begin(), external.end(), [](const ExternalFunction &a, const ExternalFunction &b) {
            if (a.mod == b.mod)
            return a.name < b.name;
            return a.mod < b.mod;
        });

        for(auto &e : external) {
            if(e.module == true)
                out << "\t.extern " << e.name << "\n";
            else
                out << "\t.extern " << e.mod << "_" << e.name << "\n";
        }
    
        if(this->object) 
            out << name << ":\n";
        else
            out << "main:\n";

        out << "\tpush %rbp\n";
        out << "\tmov %rsp, %rbp\n";
    
        bool done_found = false;

        for(size_t i = 0; i < inc.size(); ++i) {
            const Instruction &instr = inc[i];
            if(labels_.find(i) != labels_.end()) {
                if(labels[labels_[i]].second == true) {
                    out << name << "_" << labels_[i] << ":\n";
                    out << "\tpush %rbp\n";
                    out << "\tmov %rsp, %rbp\n";

                } else {
                    out << "." << labels_[i] << ":\n";
                }
            } 

            if(instr.instruction == DONE)
                done_found = true;
            generateInstruction(out, instr);
        }
        
        if(this->object == false && done_found == false)
            throw mx::Exception("Program missing done to signal completion.\n");
            
        out << "\n\n\n.section .note.GNU-stack,\"\",@progbits\n\n";
        std::string mainFunc = " Object";
        if(root_name == name)
                mainFunc = " Program";
        std::cout << "MXVM: Compiled: " << name << ".s"  << mainFunc << "\n";
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
        gen_ret(out, i);
    }

    void Program::gen_ret(std::ostream &out, const Instruction &i) {
        out << "\tmov $0, %rax\n";
        out << "\tmov %rbp, %rsp\n";
        out << "\tpop %rbp\n";
        out << "\tret\n";
    }

    void Program::gen_return(std::ostream &out, const Instruction &i) {
        if(isVariable(i.op1.op)) {
            out << "\tmovq %rax, " << getMangledName(i.op1.op) << "(%rip)\n";
        } else {
            throw mx::Exception("return requires variable\n");
        }
    }
    
    void Program::gen_neg(std::ostream &out, const Instruction &i) {
        if(isVariable(i.op1.op)) {
            Variable &v = getVariable(i.op1.op);
            if(v.type == VarType::VAR_INTEGER) {
                generateLoadVar(out, v.type, "%rcx", i.op1);
                out << "\tnegq %rcx\n";
                out << "\tmovq %rcx, " << getMangledName(i.op1.op) << "(%rip)\n";
            } else if(v.type == VarType::VAR_FLOAT) {
                generateLoadVar(out, VarType::VAR_FLOAT, "%xmm0", i.op1);
                out << "\txorpd %xmm1, %xmm1\n";
                out << "\tsubsd %xmm0, %xmm1\n";
                out << "\tmovsd %xmm1, " << getMangledName(i.op1.op) << "(%rip)\n";
            } else {
                throw mx::Exception("neg requires float or integer.");
            }
        } else {
            throw mx::Exception("neg requires variable");
        }
    }

    void Program::gen_invoke(std::ostream &out, const Instruction &i) {
        if(!i.op1.op.empty()) {
            std::vector<Operand> op;
            op.push_back(i.op1);
            if(!i.op2.op.empty()) {
                op.push_back(i.op2);
               if(!i.op3.op.empty()) {
                    op.push_back(i.op3);
                    for(size_t z = 0; z < i.vop.size(); ++z) {
                        if(!i.vop[z].op.empty()) {
                            op.push_back(i.vop[z]);
                        }
                    }
                }
            }
            generateInvokeCall(out, op);
        } else {
            throw mx::Exception("invoke instruction requires operand of instruction name");
        }
    }

    void Program::gen_call(std::ostream &out, const Instruction &i) {
        if(isFunctionValid(i.op1.op)) {
            bool found_in_object = false;
            for(auto &obj : objects) {
                if(obj->isFunctionValid(i.op1.op)) {
                    out << "\tcall " << i.op1.object << "_" << i.op1.op << "\n";
                    found_in_object = true;
                    break;
                }
            }
            if(!found_in_object) {
                out << "\tcall " << i.op1.op << "\n";
            }
        } else {
            throw mx::Exception("function label for call: " + i.op1.op + " not found.\n");
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
                out << "\tmovq " << getMangledName(i.op2.op) << "(%rip), %rsi\n";
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
                out << "\tmovq " << getMangledName(i.op3.op) << "(%rip), %rdi\n";
            } else {
                out << "\tmovq $" << i.op3.op << ", %rdi\n";
            }
        } else {
            out << "\tmovq $1, %rdi\n"; 
        }
        out << "\txor %rax, %rax\n"; 
        out << "\tcall calloc\n";
        out << "\tmovq %rax, " << getMangledName(i.op1.op) << "(%rip)\n";
    }

    void Program::gen_free(std::ostream &out, const Instruction &i) {
        if (!isVariable(i.op1.op)) {
            throw mx::Exception("FREE argument must be a variable");
        }
        Variable &v = getVariable(i.op1.op);
        if (v.type != VarType::VAR_POINTER) {
            throw mx::Exception("FREE argument must be a pointer");
        }
        out << "\tmovq " << getMangledName(i.op1.op) << "(%rip), %rdi\n";
        out << "\tcall free\n";
    }
    void Program::gen_load(std::ostream &out, const Instruction &i) {
        if (!isVariable(i.op1.op)) {
            throw mx::Exception("LOAD destination must be a variable");
        }
        Variable &dest = getVariable(i.op1.op);
        if (!isVariable(i.op2.op)) {
            throw mx::Exception("LOAD source must be a pointer variable");
        }
        Variable &ptrVar = getVariable(i.op2.op);
        if (ptrVar.type != VarType::VAR_POINTER) {
            throw mx::Exception("LOAD " + i.op2.op + " must be a pointer");
        }
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
        out << "\tmovq " << getMangledName(i.op2.op) << "(%rip), %rax\n";
        out << "\timul $" << size << ", %rcx, %rcx\n";
        out << "\tadd %rcx, %rax\n";
        if (dest.type == VarType::VAR_INTEGER) {
            out << "\tmovq (%rax), %rdx\n";
            out << "\tmovq %rdx, " << getMangledName(i.op1.op) << "(%rip)\n";
        } else if (dest.type == VarType::VAR_FLOAT) {
            out << "\tmovsd (%rax), %xmm0\n";
            out << "\tmovsd %xmm0, " << getMangledName(i.op1.op) << "(%rip)\n";
        } else if(dest.type == VarType::VAR_BYTE) {
            out << "\tmovb (%rax), %r8b\n";
            out << "\tmovb %r8b, " << getMangledName(i.op1.op) << "(%rip)\n";
        } 
        else {
            throw mx::Exception("LOAD: unsupported destination type");
        }
    }

    void Program::gen_store(std::ostream &out, const Instruction &i) {
        if (!isVariable(i.op1.op)) {
            throw mx::Exception("STORE source must be a variable");
        }
        Variable &src = getVariable(i.op1.op);

        if (!isVariable(i.op2.op)) {
            throw mx::Exception("STORE destination must be a pointer variable");
        }
        Variable &ptrVar = getVariable(i.op2.op);
        if (ptrVar.type != VarType::VAR_POINTER) {
            throw mx::Exception("STORE destination must be a pointer");
        }
        if (!i.vop.empty() && !i.vop[0].op.empty()) {
            if (isVariable(i.vop[0].op)) {
                Variable &v = getVariable(i.vop[0].op);
                generateLoadVar(out, v.type, "%rdx", i.vop[0]);
            } else {
                generateLoadVar(out, VarType::VAR_INTEGER, "%rdx", i.vop[0]);
            }
        } else {
            throw mx::Exception("STORE: requires allocaiton size 4th argument.");
        }

        std::string idx_reg = "%rcx";
        if (!i.op3.op.empty()) {
            generateLoadVar(out, VarType::VAR_INTEGER, idx_reg, i.op3);
        } else {
           throw mx::Exception("STORE resize third argument of size count");
        }

        out << "\tmovq " << getMangledName(i.op2.op) << "(%rip), %rax\n";
        out << "\timulq %rdx, %rcx\n";
        out << "\taddq %rcx, %rax\n";

        if (src.type == VarType::VAR_INTEGER) {
            out << "\tmovq " << getMangledName(i.op1.op) << "(%rip), %rdx\n";
            out << "\tmovq %rdx, (%rax)\n";
        } else if(src.type == VarType::VAR_BYTE) {
            out << "\tmovb " << getMangledName(i.op1.op) << "(%rip), %r8b\n";
            out << "\tmovb %r8b, (%rax)\n";
        } else if (src.type == VarType::VAR_FLOAT) {
            out << "\tmovsd " << getMangledName(i.op1.op) << "(%rip), %xmm0\n";
            out << "\tmovsd %xmm0, (%rax)\n";
        } else {
            throw mx::Exception("STORE: unsupported source type");
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
        out << "\tleaq " << getMangledName(i.op2.op) << "(%rip), %rdi\n";
        out << "\tcall atol\n";
        out << "\tmovq %rax, " << getMangledName(i.op1.op) << "(%rip)\n";
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
        out << "\tleaq " << getMangledName(i.op2.op) << "(%rip), %rdi\n";
        out << "\tcall atof\n";
        out << "\tmovsd %xmm0, " <<getMangledName( i.op1.op) << "(%rip)\n";
    }
        

    void Program::gen_div(std::ostream &out, const Instruction &i) {
        if (i.op3.op.empty()) {
            if (isVariable(i.op1.op)) {
                Variable &v = getVariable(i.op1.op);
                if (v.type == VarType::VAR_INTEGER) {
                    generateLoadVar(out, VarType::VAR_INTEGER, "%rax", i.op1);
                    generateLoadVar(out, VarType::VAR_INTEGER, "%rcx", i.op2);
                    out << "\tcqto\n";
                    out << "\tidivq %rcx\n";
                    out << "\tmovq %rax, " << getMangledName(i.op1.op) << "(%rip)\n";
                } else if (v.type == VarType::VAR_FLOAT) {
                    generateLoadVar(out, VarType::VAR_FLOAT, "%xmm0", i.op1);
                    generateLoadVar(out, VarType::VAR_FLOAT, "%xmm1", i.op2);
                    out << "\tdivsd %xmm1, %xmm0\n";
                    out << "\tmovsd %xmm0, " << getMangledName(i.op1.op) << "(%rip)\n";
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
                    out << "\tcqto\n";
                    out << "\tidivq %rcx\n";
                    out << "\tmovq %rax, " << getMangledName(i.op1.op) << "(%rip)\n";
                } else if (v.type == VarType::VAR_FLOAT) {
                    generateLoadVar(out, VarType::VAR_FLOAT, "%xmm0", i.op2);
                    generateLoadVar(out, VarType::VAR_FLOAT, "%xmm1", i.op3);
                    out << "\tdivsd %xmm1, %xmm0\n";
                    out << "\tmovsd %xmm0, " << getMangledName(i.op1.op) << "(%rip)\n";
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
                    out << "\tcqto\n"; // Sign-extend rax into rdx:rax for idivq
                    out << "\tidivq %rcx\n";
                    out << "\tmovq %rdx, " << getMangledName(i.op1.op) << "(%rip)\n"; // store remainder
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
                    out << "\tcqto\n";
                    out << "\tidivq %rcx\n";
                    out << "\tmovq %rdx, " << getMangledName(i.op1.op) << "(%rip)\n";
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
        out << "\tleaq " << getMangledName(i.op1.op) << "(%rip), %rdi\n"; 
        out << "\tmovq $" << dest.var_value.buffer_size << ", %rsi\n"; 
        out << "\tmovq stdin(%rip), %rdx\n"; 
        out << "\tcall fgets\n";
        out << "\tleaq " << getMangledName(i.op1.op) << "(%rip), %rdi\n";
        out << "\tcall strlen\n";
        out << "\tmovq %rax, %rcx\n";
        out << "\tcmp $0, %rax\n";
        out << "\tje .over" << over_count << "\n";
        out << "\tsub $1, %rcx\n";
        out << "\tleaq " << getMangledName(i.op1.op) << "(%rip), %rdi\n";
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
                    out << "\tmovq %rax, " << getMangledName(i.op1.op) << "(%rip)\n";
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
                    out << "\tmovq %rax, " << getMangledName(i.op1.op) << "(%rip)\n";
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
                out << "\tmovq %rax, " << getMangledName(i.op1.op) << "(%rip)\n";
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
                out << "\tmovq " << getMangledName(i.op1.op) << "(%rip), %rax\n";
                out << "\tpushq %rax\n";
            } else if (v.type == VarType::VAR_STRING) {
                out << "\tleaq " << getMangledName(i.op1.op) << "(%rip), %rax\n";
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
            out << "\tmovq %rax, " << getMangledName(i.op1.op) << "(%rip)\n";
        } else if (v.type == VarType::VAR_POINTER || v.type == VarType::VAR_EXTERN) {
            out << "\tpopq %rax\n";
            out << "\tmovq %rax, " << getMangledName(i.op1.op) << "(%rip)\n";
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
        out << "\tmovq %rcx, " << getMangledName(i.op1.op) << "(%rip)\n";
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
        out << "\tmovq " << getMangledName(i.op1.op) << "(%rip), %rcx\n";
        out << "\tmovq %rcx, (%rsp, %rax, 8)\n";
    }

    void Program::gen_stack_sub(std::ostream &out, const Instruction &i) {
        if (!i.op1.op.empty()) {
            if (isVariable(i.op1.op)) {
                out << "\tmovq " << getMangledName(i.op1.op) << "(%rip), %rcx\n";
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
                    out << "\tmovq " << v.obj_name << "_" << op.op << "(%rip), " << reg << "\n";
                    count = 0;
                    break;
                case VarType::VAR_FLOAT:
                    out << "\tmovsd " << v.obj_name << "_" << op.op << "(%rip), " << reg << "\n";
                    count = 1;
                break;
                case VarType::VAR_STRING:
                    count = 0;
                    out << "\tleaq " << v.obj_name << "_" << op.op << "(%rip), " << reg << "\n";
                break;
                case VarType::VAR_BYTE:
                    count = 0;
                    out << "\tmovzbq " << v.obj_name << "_"  << op.op << "(%rip), " << reg << "\n";
                break;
                default:
                break;
            }
        }
        else {
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
                    out << "\tmovq "  << v.obj_name << "_"  << op.op << "(%rip), " << reg << "\n";
                    count = 0;
                    break;
                case VarType::VAR_BYTE:
                    out << "\tmovzbq " << v.obj_name << "_" << op.op << "(%rip), " << reg << "\n";
                    break;
                case VarType::VAR_FLOAT:
                    out << "\tmovsd " << v.obj_name << "_"  << op.op << "(%rip), " << reg << "\n";
                    count = 1;
                break;
                case VarType::VAR_STRING:
                    count = 0;
                    out << "\tleaq " << v.obj_name << "_"  << op.op << "(%rip), " << reg << "\n";
                break;
                default:
                break;
            }
        }
        else {
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

    void Program::gen_print(std::ostream &out, const Instruction &i) {
        xmm_offset = 0;
        out << "\tlea " << getMangledName(i.op1.op) << "(%rip), %rdi\n";
        std::vector<Operand> args;
        if (!i.op2.op.empty()) args.push_back(i.op2);
        if (!i.op3.op.empty()) args.push_back(i.op3);
        for (const auto& v : i.vop) {
            if (!v.op.empty()) args.push_back(v);
        }
        int total = 0;
        int reg_count = 1;

        size_t num_pushes = (args.size() > 5) ? (args.size() - 5) : 0;
        bool needs_dummy = (num_pushes % 2 != 0);
        
        if(needs_dummy) {
            out << "\tsub $8, %rsp\n";
        }
        
        for (size_t idx = args.size() - 1; idx >= 5 && idx < args.size(); --idx) {
            if(isVariable(args[idx].op)) {
                Variable &v = getVariable(args[idx].op);
                if(v.type == VarType::VAR_INTEGER) {
                    generateLoadVar(out, VarType::VAR_INTEGER, "%rax", args[idx]);
                    out << "\tpushq %rax\n";
                } 
            } else {
                out << "\tpushq $" << args[idx].op << "\n";
            }
        }
        reg_count = 1;
        for(size_t z = 0; z < args.size() && reg_count < 6; ++z, ++reg_count) {
            total += generateLoadVar(out, reg_count, args[z]);
        }
        if(total == 0) {
            out << "\txor %rax, %rax\n";
        } else {
            out << "\tmov $" << total << ", %rax\n";
        }
        out << "\tcall printf\n";
        if (num_pushes > 0 || needs_dummy) {
            out << "\tadd $" << ((num_pushes + (needs_dummy ? 1 : 0)) * 8) << ", %rsp\n";
        }
    }

    void Program::gen_jmp(std::ostream &out, const Instruction &i) {
        if(!i.op1.op.empty()) {
            auto pos = labels.find(i.op1.op);
            if(pos == labels.end()) {
                throw mx::Exception("Jump instruction msut have valid label.");
            }
            out << "\t" << i.instruction << " ." << i.op1.op << "\n";
        } else {
            throw mx::Exception("Jump instruction must have label");
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

    void Program::generateInvokeCall(std::ostream &out, std::vector<Operand> &op) {
        if(!op.empty()  && !op[0].op.empty()) {
            std::string name = op[0].op;
            std::vector<Operand> opz;
            for(size_t i = 1; i < op.size(); ++i) 
                opz.push_back(op[i]);

            generateFunctionCall(out, name, opz);
        }
    }

    void Program::generateFunctionCall(std::ostream &out, const std::string &name, std::vector<Operand> &op) {
        if(op.empty()) {
            out << "\tcall " << name << "\n";
            return;
        }
        xmm_offset = 0;
        if(isVariable(op[0].op)) {
            Variable &v = getVariable(op[0].op);
            switch(v.type) {
                case VarType::VAR_INTEGER:
                case VarType::VAR_EXTERN:
                case VarType::VAR_POINTER:
                out << "\tmovq " << getMangledName(op[0].op) << "(%rip), %rdi\n";
                break;
                case VarType::VAR_STRING:
                out << "\tleaq " << getMangledName(op[0].op) << "(%rip), %rdi\n";
                break;
                case VarType::VAR_BYTE:
                out << "\tmovzbq " << getMangledName(op[0].op) << "(%rip), %rdi\n";
                break;
            default:
                throw mx::Exception("Argument type not supported yet\n");
            }
        } else if(op[0].type == OperandType::OP_CONSTANT) {
                out << "\tmovq $" << op[0].op << ", %rdi\n";
        }
        std::vector<Operand> args;
        for(size_t i = 1; i < op.size(); ++i) {
            if(!op[i].op.empty()) {
                args.push_back(op[i]);
            }
        }
        int total = 0;
        int reg_count = 1;
        size_t num_pushes = (args.size() > 5) ? (args.size() - 5) : 0;
        bool needs_dummy = (num_pushes % 2 != 0);
        if(needs_dummy) {
            out << "\tsub $8, %rsp\n";
        }
        for (size_t idx = args.size() - 1; idx >= 5 && idx < args.size(); --idx) {
            if(isVariable(args[idx].op)) {
                Variable &v = getVariable(args[idx].op);
                if(v.type == VarType::VAR_INTEGER || v.type == VarType::VAR_EXTERN) {
                    generateLoadVar(out, v.type, "%rax", args[idx]);
                    out << "\tpushq %rax\n";
                } 
            } else {
                out << "\tpushq $" << args[idx].op << "\n";
            }
        }
        reg_count = 1;
        for(size_t z = 0; z < args.size() && reg_count < 6; ++z, ++reg_count) {
            total += generateLoadVar(out, reg_count, args[z]);
        }

        if(total != 0) {
            out << "\tmovq $" << total << ", %rax\n";
        } else {
            out << "\txor %rax, %rax\n";  
        }
        out << "\tcall " << name << "\n";
        if (num_pushes > 0 || needs_dummy) {
            out << "\tadd $" << ((num_pushes + (needs_dummy ? 1 : 0)) * 8) << ", %rsp\n";
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
                        out << "\tmovq %rax, " << getMangledName(i.op1.op) << "(%rip)\n";
                    break;
                    case VarType::VAR_POINTER:
                    case VarType::VAR_EXTERN:
                        generateLoadVar(out, v.type, "%rax", i.op2);    
                        out << "\tmovq %rax, " << getMangledName(i.op1.op) << "(%rip)\n";
                    break;
                    case VarType::VAR_FLOAT:
                        generateLoadVar(out, VarType::VAR_FLOAT, "%xmm0", i.op2);
                        out << "\tmovsd %xmm0, " << getMangledName(i.op1.op) << "(%rip)\n";
                    break;

                    case VarType::VAR_STRING:
                        generateLoadVar(out, VarType::VAR_STRING, "%rax", i.op2);
                        out << "\tmovq %rax, " << getMangledName(i.op1.op) << "(%rip)\n";
                    break;
                    case VarType::VAR_BYTE:
                        generateLoadVar(out, VarType::VAR_BYTE, "%rax", i.op2);
                        out << "\tmovb %al, " << getMangledName(i.op1.op) << "(%rip)\n";
                    break;
                default:
                    throw mx::Exception("type not supported for mov instruction.");
                }
            } else {
                if(i.op2.type == OperandType::OP_CONSTANT) {
                    out << "\tmovq $" << i.op2.op << ", " << getMangledName(i.op1.op) <<  "(%rip)\n";
                }
            } 
        } else {
            throw mx::Exception("For mov command first operand should be a Variable");
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
                    out << "\tmovq %rax, " << getMangledName(i.op1.op) << "(%rip)\n";
                } else if(v.type == VarType::VAR_FLOAT) {
                    generateLoadVar(out, VarType::VAR_FLOAT, "%xmm0", i.op1);
                    generateLoadVar(out, VarType::VAR_FLOAT, "%xmm1", i.op2);
                    out << "\t" << arth << "sd %xmm1, %xmm0\n";
                    out << "\tmovsd %xmm0, " << getMangledName(i.op1.op) << "(%rip)\n";
                } else {
                    // not im plement
                    throw mx::Exception("Variable type not impelmented for add function");
                }
            } else {
                throw mx::Exception("First argument of add instruction must ba a variable.");
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
                    out << "\tmovq %rax, " << getMangledName(i.op1.op) << "(%rip)\n";
                } else if(v.type == VarType::VAR_FLOAT) {
                    generateLoadVar(out,VarType::VAR_FLOAT,"%xmm0", i.op2);
                    generateLoadVar(out,VarType::VAR_FLOAT, "%xmm1", i.op3);
                    out << "\t" << arth << "sd %xmm1, %xmm0\n";
                    out << "\tmovsd %xmm0, " << getMangledName(i.op1.op) << "(%rip)\n";
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

    void Program::stop() {
        running = false;
    }

    void Program::flatten_inc(Program *root, Instruction &i) {
        root->add_instruction(i);
    }

    void Program::flatten_label(Program *root, int64_t offset, const std::string &label, bool func) {
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
        for (auto& obj : objects) {
            Program* foundObj = obj->getObjectByName(obj->name);
            if (foundObj) {
                auto it2 = foundObj->vars.find(n);
                if (it2 != foundObj->vars.end()) {
                    return it2->second;
                }
            }
        }
        auto it = vars.find(n);
        if (it != vars.end()) {
            return it->second;
        }
        throw mx::Exception("Variable not found: " + name + "." + n);
    }

    bool Program::isVariable(const std::string& name) {

        if(vars.find(name) != vars.end())
            return true;

        for(auto &o : objects)  {
            if(o->isVariable(name))
                return true;
        }
        return false;
    }

    bool Program::validateNames(Validator &v) {
        for(auto &variable :  v.var_names) {
            auto it = vars.find(variable.second.getTokenValue());
            if(it == vars.end()) {
                for(auto &o : objects) {
                    if(o->name == variable.first && !o->isVariable(variable.second.getTokenValue())) {
                        throw mx::Exception("Syntax Error: Argument variable not defined: Object: " + variable.first +  " variable: " + variable.second.getTokenValue() +" at line " + std::to_string(variable.second.getLine()));
                    }               
                }
            }
        }
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
            case VarType::VAR_BYTE:
                    dest.var_value.int_value =  *reinterpret_cast<unsigned char*>(base);
                    dest.var_value.type = VarType::VAR_BYTE;
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
        int64_t index = 0;
        int64_t size = 8;
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
        } else {
            throw mx::Exception("STORE: operand 3 is empty\n");
        }

        if (!instr.vop.empty()) {
            const auto& sizeOp = instr.vop[0];
            if (isVariable(sizeOp.op)) {
                size = getVariable(sizeOp.op).var_value.int_value;
            } else if (!sizeOp.op.empty()) {
                size = static_cast<size_t>(std::stoll(sizeOp.op, nullptr, 0));
            } else {
                throw mx::Exception("STORE: index operand is empty or invalid");
            }
        } else {
            throw mx::Exception("STORE operand 4 index is empty");
        }
        Variable& ptrVar = getVariable(instr.op2.op);

        if (index > static_cast<int64_t>(ptrVar.var_value.ptr_count)) {
            throw mx::Exception("STORE: index out of bounds for " + ptrVar.var_name + " " + std::to_string(index) + " > " + std::to_string( ptrVar.var_value.ptr_count));
        }
        if (size > static_cast<int64_t>(ptrVar.var_value.ptr_size)) {
            throw mx::Exception("STORE: size out of bounds for " + ptrVar.var_name + " " + std::to_string(size) + " > " + std::to_string(ptrVar.var_value.ptr_size));
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
            case VarType::VAR_BYTE:
                *reinterpret_cast<unsigned char*>(base) = static_cast<unsigned char>(dest.var_value.int_value);
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
        dest.var_value.int_value = ~dest.var_value.int_value;
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
                        std::snprintf(buffer, sizeof(buffer), spec.c_str(), arg->var_value.ptr_value);
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
                        std::snprintf(buffer, sizeof(buffer), spec.c_str(), arg->var_value.ptr_value);
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
                    } else {
                        throw mx::Exception("to_int second argument must be a string");
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
                    } else {
                        throw mx::Exception("to_float second argument must be a string");
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
                throw mx::Exception("Invalid return type, type mismatch.\n");
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
                    case VarType::VAR_EXTERN: typeStr = "external"; break;
                    case VarType::VAR_ARRAY: typeStr = "array"; break;
                    case VarType::VAR_BYTE:  typeStr = "byte"; break;
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
                    << std::setw(10) << label.second.first << "\n";
            }
        }
        out << "\n";

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

    void except_assert(std::string reason, bool value) {
        if(value == false) {
            throw mx::Exception(reason);
        }
    }
}