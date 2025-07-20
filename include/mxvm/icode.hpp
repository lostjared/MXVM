#ifndef ICODE_HPP_
#define ICODE_HPP_

#include"mxvm/instruct.hpp"
#include<unordered_map>
#include<vector>


namespace mxvm {

    class Program {
    public:
        Program();
        void add_instruction(const Instruction &i);
        void add_label(const std::string &name, uint64_t address);
        void add_variable(const std::string &name, const Variable &v);
        void stop();
        void exec();
        void print(std::ostream &out);
    private:
        size_t pc;  
        bool running;
                
        bool zero_flag = false;
        bool less_flag = false;
        bool greater_flag = false;
        
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
        void exec_exit(const Instruction& instr);
        
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
        std::vector<Instruction> inc;
        std::unordered_map<std::string, Variable> vars;
        std::unordered_map<std::string, uint64_t> labels;
    };
    
}

#endif