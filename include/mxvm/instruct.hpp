#ifndef __INSTRUCT_HPP_X_
#define __INSTRUCT_HPP_X_

#include<iostream>
#include<vector>
#include<cstdint>

enum Inc { NULL_INC = 0, MOV, LOAD, STORE, ADD, SUB, MUL, DIV, OR, AND, XOR, NOT, MOD, CMP, JMP, JE, JNE, JL, JLE, JG, JGE, JZ, JNZ, JA, JB, PRINT, EXIT, ALLOC, FREE, GETLINE};

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
    "mod",      // MOD = 12
    "cmp",      // CMP = 13
    "jmp",      // JMP = 14
    "je",       // JE = 15
    "jne",      // JNE = 16
    "jl",       // JL = 17
    "jle",      // JLE = 18
    "jg",       // JG = 19
    "jge",      // JGE = 20
    "jz",       // JZ = 21
    "jnz",      // JNZ = 22
    "ja",       // JA = 23
    "jb",       // JB = 24
    "print",    // PRINT = 25
    "exit",     // EXIT = 26
    "alloc",    // ALLOC = 27
    "free",      // FREE =  28
    "getline",  // GETLINE = 29
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
        std::vector<Operand> vop;
    };

    enum class VarType { VAR_NULL=0, VAR_INTEGER, VAR_FLOAT, VAR_STRING, VAR_POINTER, VAR_LABEL, VAR_ARRAY };

    struct Variable_Value {
        std::string str_value;
        std::string label_value;
        int64_t int_value = 0;
        double float_value = 0;
        void *ptr_value = nullptr;
        uint64_t ptr_size = 0;
        uint64_t ptr_count = 0;
        VarType type;
    };

    struct Variable {
        VarType type;
        std::string var_name;
        Variable_Value var_value;
        Variable() = default;
        Variable(const std::string name, const VarType &vtype, const Variable_Value &value);
    };
}

#endif