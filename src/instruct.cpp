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
    out << "[" << inc.instruction << "] ";
    std::ostringstream opc;
    if (!inc.op1.op.empty()) opc << inc.op1.op;
    if (!inc.op2.op.empty()) opc << ", " << inc.op2.op;
    if (!inc.op3.op.empty()) opc << ", " << inc.op3.op;
    for (const auto &arg : inc.vop) {
        if (!arg.op.empty()) opc << ", " << arg.op;
    }
    if(opc.str().length() > 0) {
        out << " " << opc.str();
    }  
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

    std::string Instruction::toString() const {
        std::ostringstream stream;
        stream << *this;
        return stream.str();
    }

    std::string Variable::toString() const {
        std::ostringstream stream;
        stream << type;
        return stream.str();
    }
}