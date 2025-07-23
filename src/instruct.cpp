#include"mxvm/instruct.hpp"

std::ostream &operator<<(std::ostream &out, const enum  Inc &i) {
    size_t pos = static_cast<size_t>(i);
    if(pos  <= IncType.size())
        out << IncType[pos];
    else
        out << "[undefined]";
    return out;

}

std::ostream &operator<<(std::ostream &out, const mxvm::Instruction &inc) {
    out << inc.instruction << " [ ";
    if (!inc.op1.op.empty()) out << inc.op1.op << " ";
    if (!inc.op2.op.empty()) out << inc.op2.op << " ";
    if (!inc.op3.op.empty()) out << inc.op3.op << " ";
    for (const auto &arg : inc.vop) {
        if (!arg.op.empty()) out << arg.op << " ";
    }
    out << "]";
    return out;
}

std::ostream &operator<<(std::ostream &out, const mxvm::VarType &type) {
    switch(type) {
        case mxvm::VarType::VAR_INTEGER:
            out << "int";
        break;
        
        case mxvm::VarType::VAR_STRING:
            out << "string";
        break;      
        case mxvm::VarType::VAR_EXTERN:
            out << "extern";
        break;
        case mxvm::VarType::VAR_POINTER:
            out << "ptr";
        break;
        case mxvm::VarType::VAR_FLOAT:
            out << "float";
        break;
        case mxvm::VarType::VAR_BYTE:
            out << "byte";
        break;
        
        default:
            out << "unknown";
    }
    return out;
}

namespace mxvm {
    
}