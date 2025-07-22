#ifndef ICODE_HPP_
#define ICODE_HPP_

#include"mxvm/instruct.hpp"
#include"scanner/exception.hpp"
#include<unordered_map>
#include<vector>
#include<variant>

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

    class Base {
    public:
        void add_instruction(const Instruction &i);
        void add_label(const std::string &name, uint64_t address, bool f);
        void add_variable(const std::string &name, const Variable &v);
    protected:
        std::vector<Instruction> inc;
        std::unordered_map<std::string, Variable> vars;
        std::unordered_map<std::string, std::pair<uint64_t, bool>> labels;
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
        void generateCode(std::ostream &out);
        static std::string escapeNewLines(const std::string &text);
    private:
        size_t pc;  
        bool running;
        int exitCode = 0;
        bool zero_flag = false;
        bool less_flag = false;
        bool greater_flag = false;
        int xmm_offset = 0;
        
        // x86_64 System V ABI Linux code generation
        void generateFunctionCall(std::ostream &out, const std::string &name, std::vector<Operand> &op);
        void generateInstruction(std::ostream &out, const Instruction  &i);
        int generateLoadVar(std::ostream &out, int reg, const Operand &op);
        int generateLoadVar(std::ostream &out, VarType type, std::string reg, const Operand &op);
        std::string getRegisterByIndex(int index, VarType type);
        void gen_print(std::ostream &out, const Instruction &i);
        void gen_arth(std::ostream &out, std::string arth, const Instruction &i);
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
        Variable& getVariable(const std::string& name);
        bool isVariable(const std::string& name);
        void setVariableFromString(Variable& var, const std::string& value);
        void addVariables(Variable& dest, Variable& src1, Variable& src2);
        void subVariables(Variable& dest, Variable& src1, Variable& src2);
        void mulVariables(Variable& dest, Variable& src1, Variable& src2);
        void divVariables(Variable& dest, Variable& src1, Variable& src2);
        void printFormatted(const std::string& format, const std::vector<Variable*>& args);
        void setVariableFromConstant(Variable& var, const std::string& value);
        Variable createTempVariable(VarType type, const std::string& value);
        bool isConstant(const std::string& value);
    protected:
        Stack stack;
    };
    
}

#endif