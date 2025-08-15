#ifndef ICODE_HPP_
#define ICODE_HPP_

#include"mxvm/instruct.hpp"
#include"scanner/exception.hpp"
#include<unordered_map>
#include<vector>
#include<variant>
#include"mxvm/function.hpp"
#include"mxvm/parser.hpp"
#include<functional>

namespace mxvm {

    
    

    using StackValue = std::variant<int64_t, void*>;

    class Stack {
    public:
        Stack() = default;
        void push(const StackValue& value) {
            data.push_back(value);
        }
        StackValue pop() {
            if (data.empty()) throw mx::Exception("Stack underflow");
            StackValue val = data.back();
            data.pop_back();
            return val;
        }
        bool empty() const { return data.empty(); }
        size_t size() const { return data.size(); }
        StackValue &operator[](size_t index) { return data[index]; }
    private:
        std::vector<StackValue> data;
    };

    class Program;

    using runtime_call = std::function<void(Program *program, std::vector<Operand> &operands)>;

    class RuntimeFunction {
    public:
        RuntimeFunction() : func(nullptr), handle(nullptr) {}
        RuntimeFunction(const RuntimeFunction &r) : func(r.func), handle(r.handle), mod_name(r.mod_name), fname(r.fname) {}
        RuntimeFunction &operator=(const RuntimeFunction &r) {
            func = r.func;
            handle = r.handle;
            mod_name = r.mod_name;
            fname = r.fname;
            return *this;
        }
        ~RuntimeFunction() = default;
        RuntimeFunction(const std::string &mod, const std::string &name);
        void call(Program *program, std::vector<Operand> &operands);
        void *func = nullptr;
        void *handle = nullptr;
        std::string mod_name;
        std::string fname;
        static std::unordered_map<std::string, void *> handles;
    };

    class Base {
    public:
        Base() = default;
        Base(Base &&other) noexcept;
        ~Base() = default;
        Base &operator=(Base &&other) noexcept;
        void setMainBase(Base *b) { Base::base = b; }
        void add_instruction(const Instruction &i);
        void add_label(const std::string &name, uint64_t address, bool f);
        void add_variable(const std::string &name, const Variable &v);
        void add_global(const std::string &objname, const std::string &name, const Variable &v);
        void add_extern(const std::string &mod, const std::string &name, bool module);
        void add_runtime_extern(const std::string &mod_name, const std::string &mod, const std::string &func_name, const std::string &name);
        void add_filename(const std::string &fname);
        void add_allocated(const std::string &name, Variable &v);
        void add_object(const std::string &name, Program *prog);
        std::string name;
        std::vector<Instruction> inc;
        std::unordered_map<std::string, Variable> vars;
        std::unordered_map<std::string, std::pair<uint64_t, bool>> labels;
        std::vector<ExternalFunction> external;
        std::unordered_map<std::string, RuntimeFunction> external_functions;
        static Base *base;
        static std::string root_name;
        static std::vector<std::string> filenames;
        static std::unordered_map<std::string, Variable> allocated;
        static std::unordered_map<std::string, Program *> object_map;
        std::string assembly_code;
        
    };

    class Program : public Base {
    public:
        friend class Parser;
        Program();
        ~Program();
    
  
        Program(Program&& other) noexcept
        : Base(std::move(other)),
            filename(std::move(other.filename)),
            objects(std::move(other.objects)),
            object(other.object),
            pc(other.pc),
            running(other.running),
            exitCode(other.exitCode),
            zero_flag(other.zero_flag),
            less_flag(other.less_flag),
            greater_flag(other.greater_flag),
            xmm_offset(other.xmm_offset),
            args(std::move(other.args)),
            main_function(other.main_function),
            object_external(other.object_external),
            stack(std::move(other.stack)),
            result(std::move(other.result)),
            parent(other.parent),
            platform(other.platform)
        {
            other.pc = 0;
            other.running = false;
            other.exitCode = 0;
            for (auto& var_pair : vars) {
               var_pair.second.obj_name = name;
            }
        }

        Program& operator=(Program&& other) noexcept {
            if (this != &other) {
                Base::operator=(std::move(other));
                filename = std::move(other.filename);
                objects = std::move(other.objects);
                object = other.object;
                pc = other.pc;
                running = other.running;
                exitCode = other.exitCode;
                zero_flag = other.zero_flag;
                less_flag = other.less_flag;
                greater_flag = other.greater_flag;
                xmm_offset = other.xmm_offset;
                args = std::move(other.args);
                main_function = other.main_function;
                object_external = other.object_external;
                stack = std::move(other.stack);
                result = std::move(other.result);
                parent = other.parent;
                other.pc = 0;
                other.running = false;
                other.exitCode = 0;
                for (auto& var_pair : vars) {
                   var_pair.second.obj_name = name;
                }
                platform = other.platform;
            }
            return *this;
        }     

        void add_standard();
        void stop();
        int exec();
        void print(std::ostream &out);
        void post(std::ostream &out);
        int getExitCode() const { return exitCode; }
        std::string name;
        std::string filename;
        std::string getPlatformSymbolName(const std::string &name);
        void generateCode(const Platform  &platform, bool obj, std::ostream &out);
        static std::string escapeNewLines(const std::string &text);
        void memoryDump(std::ostream &out);
        void setArgs(const std::vector<std::string> &argv);
        void setObject(bool obj);
        std::vector<std::unique_ptr<Program>> objects;
        bool object = false;
        bool validateNames(Validator &v);
        void flatten(Program *program);
        void flatten_inc(Program *root, Instruction &i);
        void flatten_label(Program *root, int64_t offset, const std::string &label, bool func);
        void flatten_external(Program *root, const std::string &e, RuntimeFunction &r);
        std::string getMangledName(const std::string& var);
        std::string getMangledName(const Operand &op);
        Program *getObjectByName(const std::string &name);
        bool isFunctionValid(const std::string& label);
        //std::string resolveFunctionSymbol(const std::string& label);
    private:
        size_t pc;  
        bool running;
        int exitCode = 0;
        bool zero_flag = false;
        bool less_flag = false;
        bool greater_flag = false;
        int xmm_offset = 0;
        std::vector<std::string> args;
        bool main_function;
        bool object_external = false;
        
    public:    
        // x86_64 System V ABI Linux code generation
        void generateFunctionCall(std::ostream &out, const std::string &name, std::vector<Operand> &op);
        void generateInvokeCall(std::ostream &out, std::vector<Operand> &op);
        void generateInstruction(std::ostream &out, const Instruction  &i);
        int generateLoadVar(std::ostream &out, int reg, const Operand &op);
        int generateLoadVar(std::ostream &out, VarType type, std::string reg, const Operand &op);
        std::string getRegisterByIndex(int index, VarType type);
        void gen_print(std::ostream &out, const Instruction &i);
        void gen_arth(std::ostream &out, std::string arth, const Instruction &i);
        void gen_div(std::ostream &out, const Instruction &i);
        void gen_exit(std::ostream &out, const Instruction &i);
        void gen_mov(std::ostream &out, const Instruction &i);
        void gen_jmp(std::ostream &out, const Instruction &i);
        void gen_cmp(std::ostream &out, const Instruction &i);
        void gen_alloc(std::ostream &out, const Instruction &i);
        void gen_free(std::ostream &out, const Instruction &i);
        void gen_load(std::ostream &out, const Instruction &i);
        void gen_store(std::ostream &out, const Instruction &i);
        void gen_ret(std::ostream &out, const Instruction &i);
        void gen_call(std::ostream &out, const Instruction &i);
        void gen_done(std::ostream &out, const Instruction &i);
        void gen_bitop(std::ostream &out, const std::string &opc, const Instruction &i);
        void gen_not(std::ostream &out, const Instruction &i);
        void gen_push(std::ostream &out, const Instruction &i);
        void gen_pop(std::ostream &out, const Instruction &i);
        void gen_stack_load(std::ostream &out, const Instruction &i);
        void gen_stack_store(std::ostream &out, const Instruction &i);
        void gen_stack_sub(std::ostream &out, const Instruction &i);
        void gen_mod(std::ostream &out, const Instruction &i);
        void gen_getline(std::ostream &out, const Instruction &i);
        void gen_to_int(std::ostream &out, const Instruction &i);
        void gen_to_float(std::ostream &out, const Instruction  &i);
        void gen_invoke(std::ostream &out, const Instruction &i);
        void gen_return(std::ostream &out, const Instruction &i);
        void gen_neg(std::ostream &out, const Instruction &i);
        std::string gen_optimize(const std::string  &code, const Platform &platform);
        std::string optimize_darwin(const std::string &code);
        std::string optimize_core(const std::string &code);

        void x64_emit_iob_func(std::ostream &out, int index, const std::string &dstReg);
        std::string x64_getRegisterByIndex(int index, VarType type);
        int x64_generateLoadVar(std::ostream &out, int r, const Operand &op);
        int x64_generateLoadVar(std::ostream &out, VarType type, std::string reg, const Operand &op);
        void x64_generateFunctionCall(std::ostream &out, const std::string &name, std::vector<Operand> &args);
        void x64_generateInvokeCall(std::ostream &out, std::vector<Operand> &op);
        void x64_generateCode(const Platform &platform, bool obj, std::ostream &out);
        void x64_generateInstruction(std::ostream &out, const Instruction &i);
        void x64_gen_done(std::ostream &out, const Instruction &i);
        void x64_gen_ret(std::ostream &out, const Instruction &i);
        void x64_gen_return(std::ostream &out, const Instruction &i);
        void x64_gen_neg(std::ostream &out, const Instruction &i);
        void x64_gen_invoke(std::ostream &out, const Instruction &i);
        void x64_gen_call(std::ostream &out, const Instruction &i);
        void x64_gen_alloc(std::ostream &out, const Instruction &i);
        void x64_gen_free(std::ostream &out, const Instruction &i);
        void x64_gen_load(std::ostream &out, const Instruction &i);
        void x64_gen_store(std::ostream &out, const Instruction &i);
        void x64_gen_to_int(std::ostream &out, const Instruction &i);
        void x64_gen_to_float(std::ostream &out, const Instruction &i);
        void x64_gen_div(std::ostream &out, const Instruction &i);
        void x64_gen_mod(std::ostream &out, const Instruction &i);
        void x64_gen_getline(std::ostream &out, const Instruction &i);
        void x64_gen_bitop(std::ostream &out, const std::string &opc, const Instruction &i);
        void x64_gen_not(std::ostream &out, const Instruction &i);
        void x64_gen_push(std::ostream &out, const Instruction &i);
        void x64_gen_pop(std::ostream &out, const Instruction &i);
        void x64_gen_stack_load(std::ostream &out, const Instruction &i);
        void x64_gen_stack_store(std::ostream &out, const Instruction &i);
        void x64_gen_stack_sub(std::ostream &out, const Instruction &i);
        void x64_gen_print(std::ostream &out, const Instruction &i);
        void x64_gen_jmp(std::ostream &out, const Instruction &i);
        void x64_gen_cmp(std::ostream &out, const Instruction &i);
        void x64_gen_mov(std::ostream &out, const Instruction &i);
        void x64_gen_arth(std::ostream &out, std::string arth, const Instruction &i);
        void x64_gen_exit(std::ostream &out, const Instruction &i);


        // code interpretation
        void exec_mov(const Instruction& instr);
        void exec_add(const Instruction& instr);
        void exec_sub(const Instruction& instr);
        void exec_mul(const Instruction& instr);
        void exec_div(const Instruction& instr);
        void exec_cmp(const Instruction& instr);
        void exec_jmp(const Instruction& instr);
        void exec_load(const Instruction& instr);
        void exec_store(const Instruction& instr);
        void exec_or(const Instruction& instr);
        void exec_and(const Instruction& instr);
        void exec_xor(const Instruction& instr);
        void exec_not(const Instruction& instr);
        void exec_mod(const Instruction& instr);
        void exec_je(const Instruction& instr);
        void exec_jne(const Instruction& instr);
        void exec_jl(const Instruction& instr);
        void exec_jle(const Instruction& instr);
        void exec_jg(const Instruction& instr);
        void exec_jge(const Instruction& instr);
        void exec_jz(const Instruction& instr);
        void exec_jnz(const Instruction& instr);
        void exec_ja(const Instruction& instr);
        void exec_jb(const Instruction& instr);
        void exec_print(const Instruction& instr);
        void exec_string_print(const Instruction &instr);
        void exec_exit(const Instruction& instr);
        void exec_alloc(const Instruction& instr);
        void exec_free(const Instruction& instr);
        void exec_getline(const Instruction &instr);
        void exec_push(const Instruction &instr);
        void exec_pop(const Instruction &instr);
        void exec_stack_load(const Instruction &instr);
        void exec_stack_store(const Instruction &instr);
        void exec_stack_sub(const Instruction &instr);
        void exec_call(const Instruction &instr);
        void exec_ret(const Instruction &instr); 
        void exec_done(const Instruction &instr); 
        void exec_to_int(const Instruction &instr);
        void exec_to_float(const Instruction &instr);
        void exec_invoke(const Instruction  &instr);
        void exec_return(const Instruction &instr);
        void exec_neg(const Instruction &instr);

        Variable& getVariable(const std::string& name);
        bool isVariable(const std::string& name);
        void setVariableFromString(Variable& var, const std::string& value);
        void addVariables(Variable& dest, Variable& src1, Variable& src2);
        void subVariables(Variable& dest, Variable& src1, Variable& src2);
        void mulVariables(Variable& dest, Variable& src1, Variable& src2);
        void divVariables(Variable& dest, Variable& src1, Variable& src2);
        std::string printFormatted(const std::string& format, const std::vector<Variable*>& args, bool output = true);
        void setVariableFromConstant(Variable& var, const std::string& value);
        Variable createTempVariable(VarType type, const std::string& value);
        bool isConstant(const std::string& value);
        Variable variableFromOperand(const Operand &op);
        Stack stack;
        Operand result;
        Program *parent = nullptr;
        Platform platform;
        size_t x64_reserve_call_area(std::ostream &out, size_t spill_bytes);
        void x64_release_call_area(std::ostream &out, size_t total);
        void x64_direct_stack_adjust(std::ostream &out, int64_t bytes);
    };

    void except_assert(std::string reason, bool value);
    
}

#endif