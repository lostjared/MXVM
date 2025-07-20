#ifndef __INSTRUCT_HPP_X_
#define __INSTRUCT_HPP_X_

#include<iostream>
#include<vector>
#include<cstdint>

enum Inc { NULL_INC = 0, MOV, LOAD, STORE, ADD, SUB, MUL, DIV, OR, AND, XOR, NOT, CMP, JMP, JE, JNE, JL, JLE, JG, JGE, JZ, JNZ, JA, JB, PRINT, EXIT};

inline std::vector<std::string> IncType { 
    "NULL",     // NULL_INC = 0
    "mov",      // MOV = 1
    "load",     // LOAD = 2
    "store",    // STORE = 3
    "add",      // ADD = 4
    "sub",      // SUB = 5
    "mul",      // MUL = 6
    "div",      // DIV = 7
    "or",       // OR = 8
    "and",      // AND = 9  (was missing!)
    "xor",      // XOR = 10
    "not",      // NOT = 11
    "cmp",      // CMP = 12
    "jmp",      // JMP = 13
    "je",       // JE = 14
    "jne",      // JNE = 15 (was "jme")
    "jl",       // JL = 16
    "jle",      // JLE = 17
    "jg",       // JG = 18
    "jge",      // JGE = 19
    "jz",       // JZ = 20
    "jnz",      // JNZ = 21
    "ja",       // JA = 22
    "jb",       // JB = 23
    "print",    // PRINT = 24
    "exit"      // EXIT = 25
};

std::ostream &operator<<(std::ostream &out, const enum Inc &i);

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

    enum class VarType { VAR_NULL=0, VAR_INTEGER, VAR_FLOAT, VAR_STRING, VAR_POINTER, VAR_LABEL, VAR_ARRAY };

    struct Variable_Value {
        std::string str_value;
        std::string label_value;
        uint64_t int_value;
        double  float_value;
        void *ptr_value;
        VarType type;
    };

    struct Variable {
        VarType type;
        std::string var_name;
        Variable_Value var_value;
    };
}

#endif