#ifndef __INSTRUCT_HPP_X_
#define __INSTRUCT_HPP_X_

#include<iostream>
#include<vector>
#include<cstdint>

enum Inc { NULL_INC = 0, MOV, LOAD, STORE, ADD, SUB, MUL, DIV, OR, AND, XOR, NOT, MOD, CMP, JMP, JE, JNE, JL, JLE, JG, JGE, JZ, JNZ, JA, JB, PRINT, EXIT, ALLOC, FREE, GETLINE, PUSH, POP, STACK_LOAD, STACK_STORE, STACK_SUB, CALL, RET, STRING_PRINT, DONE, TO_INT, TO_FLOAT };

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
    "free",     // FREE =  28
    "getline",  // GETLINE = 29
    "push",     // PUSH = 30
    "pop",      // POP = 31
    "stack_load", // STACK_LOAD = 32
    "stack_store", // STACK_STORE = 33
    "stack_sub", // STACK_SUB = 34
    "call",     // CALL = 35
    "ret",       // RET = 36
    "string_print", // STRING_PRINT = 37
    "done",     // DONE = 38
    "to_int",   // TO_INT = 39
    "to_float" // TO_INT = 40
};

std::ostream &operator<<(std::ostream &out, const enum Inc &i);


namespace mxvm {

    enum class OperandType { OP_CONSTANT, OP_VARIABLE };

    struct Operand {
        std::string label;
        std::string op;
        int op_value = 0;        
        OperandType type;
    };

    struct Instruction {
        Inc instruction;
        Operand op1, op2, op3;
        std::vector<Operand> vop;
        std::string label = "";
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
        uint64_t buffer_size;
        char *buffer;
    };

    struct Variable {
        VarType type;
        std::string var_name;
        Variable_Value var_value;
        Variable() = default;
        Variable(const std::string name, const VarType &vtype, const Variable_Value &value);
    };
}

std::ostream &operator<<(std::ostream &out, const mxvm::Instruction &inc);

#endif