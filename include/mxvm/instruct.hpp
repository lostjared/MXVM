#ifndef __INSTRUCT_HPP_X_
#define __INSTRUCT_HPP_X_

#include<iostream>
#include<vector>
#include<cstdint>
#include<string>
#include<sstream>
#include<optional>

enum Inc { NULL_INC = 0, MOV, LOAD, STORE, ADD, SUB, MUL, DIV, OR, AND, XOR, NOT, MOD, CMP, JMP, JE, JNE, JL, JLE, JG, JGE, JZ, JNZ, JA, JB, PRINT, EXIT, ALLOC, FREE, GETLINE, PUSH, POP, STACK_LOAD, STACK_STORE, STACK_SUB, CALL, RET, STRING_PRINT, DONE, TO_INT, TO_FLOAT, INVOKE, RETURN , NEG};

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
    "to_float", // TO_INT = 40
    "invoke", // INVOKE = 41
    "return", // RETURN = 42
    "neg" // NEG = 43
};

std::ostream &operator<<(std::ostream &out, const enum Inc &i);


namespace mxvm {

    enum class OperandType { OP_CONSTANT, OP_VARIABLE };

    struct Operand {
        std::string label;
        std::string op;
        int op_value = 0;        
        OperandType type;
        std::string object;
    };

    struct Instruction {
        Inc instruction;
        Operand op1, op2, op3;
        std::vector<Operand> vop;
        std::string label = "";
        std::string toString() const;
    };

    enum class VarType { VAR_NULL=0, VAR_INTEGER, VAR_FLOAT, VAR_STRING, VAR_POINTER, VAR_LABEL, VAR_ARRAY, VAR_EXTERN, VAR_BYTE };

    struct Variable_Value {
        std::string str_value;
        std::string label_value;
        union {
            int64_t int_value;
            double float_value;
            void *ptr_value;
        };
        uint64_t ptr_size = 0;
        uint64_t ptr_count = 0;
        VarType type;
        uint64_t buffer_size;
        bool owns = false;

        Variable_Value(const Variable_Value& other)
            : str_value(other.str_value),
            label_value(other.label_value),
            ptr_size(other.ptr_size),
            ptr_count(other.ptr_count),
            type(other.type),
            buffer_size(other.buffer_size),
            owns(other.owns)
        {
            switch (type) {
                case VarType::VAR_INTEGER: 
                case VarType::VAR_BYTE:
                        int_value = other.int_value; 
                    break;
                case VarType::VAR_FLOAT:
                        float_value = other.float_value; 
                   break;
                case VarType::VAR_POINTER: 
                        ptr_value = other.ptr_value; 
                    break;
                default:
                        int_value = other.int_value; 
                    break;
            }
        }

        Variable_Value& operator=(const Variable_Value& other) {
            if (this != &other) {
                str_value = other.str_value;
                label_value = other.label_value;
                ptr_size = other.ptr_size;
                ptr_count = other.ptr_count;
                type = other.type;
                buffer_size = other.buffer_size;
                owns = other.owns;
                switch (type) {
                    case VarType::VAR_INTEGER:
                    case VarType::VAR_BYTE:
                         int_value = other.int_value; 
                         break;
                    case VarType::VAR_FLOAT:
                        float_value = other.float_value;
                        break;
                    case VarType::VAR_POINTER: 
                        ptr_value = other.ptr_value; 
                        break;
                    default:
                        int_value = other.int_value; 
                        break;
                }
            }
            return *this;
        }

        Variable_Value() : int_value(0), ptr_size(0), ptr_count(0), type(VarType::VAR_NULL), buffer_size(0), owns(false) {}
    };

    // Variable struct
    struct Variable {
        VarType type;
        std::string var_name;
        Variable_Value var_value;
        bool is_global = false;
        std::string obj_name;
        Variable() : type(VarType::VAR_NULL), var_name(), var_value() {}
        Variable(const std::string& name, const VarType &vtype, const Variable_Value &value)
        : type(vtype), var_name(name), var_value(value), is_global(false) {}

        
        Variable(const Variable& v)
            : type(v.type), var_name(v.var_name), var_value(v.var_value), is_global(v.is_global), obj_name(v.obj_name) {}

        void setExtern(std::string name, void *value) {
            type = VarType::VAR_EXTERN;
            var_value.type = VarType::VAR_EXTERN;
            var_value.ptr_value = value;
            var_value.ptr_count = 0;
            var_value.ptr_size = 0;
            var_value.owns = false;
        }
        
        Variable& operator=(const Variable& v) {
            if (this != &v) {
                type = v.type;
                var_name = v.var_name;
                var_value = v.var_value;
                is_global = v.is_global;
                obj_name = v.obj_name;
            }
            return *this;
        }
        std::string toString() const;
    };
}

std::ostream &operator<<(std::ostream &out, const mxvm::Instruction &inc);
std::ostream &operator<<(std::ostream &out, const mxvm::VarType &type);
#endif