#ifndef __HTML__GEN___H_
#define __HTML__GEN___H_

#include"mxvm/instruct.hpp"


namespace mxvm {

    class HTMLGen {
    public:
        HTMLGen(const std::string &src);
        void output(std::ostream &out, std::string filename);
    private:
        std::string source;

    };
}


#endif