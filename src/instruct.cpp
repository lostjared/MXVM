#include"mxvm/instruct.hpp"

std::ostream &operator<<(std::ostream &out, const enum  Inc &i) {
    size_t pos = static_cast<size_t>(i);
    if(pos  <= IncType.size())
        out << IncType[pos];
    else
        out << "[undefined]";
    return out;

}