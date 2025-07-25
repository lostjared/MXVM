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

    using runtime_call = std::function<Operand(Program *program, std::vector<Operand> &operands)>;

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
        Operand call(Program *program, std::vector<Operand> &operands);
        void *func = nullptr;
        void *handle = nullptr;
        std::string mod_name;
        std::string fname;
        static std::unordered_map<std::string, void *> handles;
    };

    class Base {
    public:
        void add_instruction(const Instruction &i);
        void add_label(const std::string &name, uint64_t address, bool f);
        void add_variable(const std::string &name, const Variable &v);
        void add_extern(const std::string &mod, const std::string &name);
        void add_runtime_extern(const std::string &mod_name, const std::string &mod, const std::string &func_name, const std::string &name);
        std::vector<Instruction> inc;
        std::unordered_map<std::string, Variable> vars;
        std::unordered_map<std::string, std::pair<uint64_t, bool>> labels;
        std::vector<ExternalFunction> external;
        std::unordered_map<std::string, RuntimeFunction> external_functions;
    };

    class Program : public Base {
    public:
        friend class Parser;
        Program();
        ~Program();
        void stop();
        int exec();
        void print(std::ostream &out);
        void post(std::ostream &out);
        int getExitCode() const { return exitCode; }
        std::string name;
        void generateCode(bool obj, std::ostream &out);
        static std::string escapeNewLines(const std::string &text);
        void memoryDump(std::ostream &out);
        void setArgs(const std::vector<std::string> &argv);
        void setObject(bool obj);
        std::vector<std::unique_ptr<Program>> objects;
        bool isFunctionValid(const std::string &f);
        bool object = false;
        bool validateNames(Validator &v);
        void flatten(Program *program);
        void flatten_inc(Program *root, Instruction &i);
        void flatten_label(Program *root, int64_t offset, const std::string &label, bool func);
        void flatten_external(Program *root, const std::string &e, RuntimeFunction &r);
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
    };

    void except_assert(std::string reason, bool value);
    
}

#endif