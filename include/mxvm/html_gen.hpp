#ifndef __HTML__GEN___H_
#define __HTML__GEN___H_

#include"mxvm/instruct.hpp"


namespace mxvm {

    class HTMLGen {
    public:
        HTMLGen(const std::string &src);
        void output(std::ostream &out);
    private:
        std::string source;

    };
}


#endif