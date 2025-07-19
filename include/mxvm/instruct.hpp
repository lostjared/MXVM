#ifndef __INSTRUCT_HPP_X_
#define __INSTRUCT_HPP_X_

#include<iostream>
#include<vector>
#include<cstdint>

enum Inc { NULL_INC = 0, MOV, LOAD, STORE, ADD, SUB, MUL, DIV, OR, AND, XOR, NOT, CMP, JMP, JE, JNE, JL, JLE, JG, JGE, JZ, JNZ, JA, JB };
inline std::vector<std::string> IncType { "NULL", "mov", "load", "store", "add", "sub", "mul", "div", "or", "xor", "not", "cmp", "jmp", "je", "jme", "jl" , "jle", "jg", "jge", "jz", "ja", "jb", ""};
std::ostream &operator<<(std::ostream &out, const enum  Inc &i);

namespace mxvm {

    struct Operand {
        std::string label;
        std::string op;
        int op_value = 0;        
    };

    struct Instruction {
        Inc instruction;
        Operand op1, op2, op3;
    };

    enum class VarType { VAR_NULL=0, VAR_INTEGER, VAR_FLOAT, VAR_POINTER, VAR_LABEL, VAR_ARRAY };

    struct Variable_Value {
        std::string str_value;
        uint64_t int_value;
        double  float_value;
        uint64_t ptr_value;
        VarType type;
    };

    struct Variable {
        VarType type;
        std::string var_name;
        Variable_Value var_value;
    };
}

#endif