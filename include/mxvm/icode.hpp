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
        void add_label(const std::string &name);
        void stop();
        void exec();
        void printInstruction();
    protected:
        uint64_t ip = 0;
        std::vector<Instruction> inc;
        std::unordered_map<std::string, Variable> vars;
        std::unordered_map<std::string, uint64_t> labels;
    };
    
}

#endif