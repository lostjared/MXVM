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

namespace mxvm {
    Variable::Variable(const std::string name, const VarType &vtype, const Variable_Value &value) : type(vtype),var_name(name),var_value(value) {} 
}