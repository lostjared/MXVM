#ifndef __INSTRUCT_HPP_X_
#define __INSTRUCT_HPP_X_

#include<iostream>
#include<vector>


enum Inc { NULL_INC = 0, MOV, LOAD, STORE, ADD, SUB, MUL, DIV, OR, AND, XOR, NOT, CMP, JMP, JE, JNE, JL, JLE, JG, JGE, JZ, JNZ, JA, JB };
inline std::vector<std::string> IncType { "NULL", "mov", "load", "store", "add", "sub", "mul", "div", "or", "xor", "not", "cmp", "jmp", "je", "jme", "jl" , "jle", "jg", "jge", "jz", "ja", "jb", ""};
std::ostream &operator<<(std::ostream &out, const enum  Inc &i);

namespace mxvm {


}

#endif