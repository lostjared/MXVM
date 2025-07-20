#include"mxvm/instruct.hpp"

std::ostream &operator<<(std::ostream &out, const enum  Inc &i) {
    size_t pos = static_cast<size_t>(i);
    if(pos  <= IncType.size())
        out << IncType[pos];
    else
        out << "[undefined]";
    return out;

}

namespace mxvm {
    Variable::Variable(const std::string name, const VarType &vtype, const Variable_Value &value) : type(vtype),var_name(name),var_value(value) {} 
}